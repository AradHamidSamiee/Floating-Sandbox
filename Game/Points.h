/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-05-06
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "GameEventDispatcher.h"
#include "GameParameters.h"
#include "MaterialDatabase.h"
#include "Materials.h"
#include "RenderContext.h"

#include <GameCore/Buffer.h>
#include <GameCore/BufferAllocator.h>
#include <GameCore/ElementContainer.h>
#include <GameCore/ElementIndexRangeIterator.h>
#include <GameCore/EnumFlags.h>
#include <GameCore/FixedSizeVector.h>
#include <GameCore/GameRandomEngine.h>
#include <GameCore/GameTypes.h>
#include <GameCore/Vectors.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <functional>
#include <vector>

namespace Physics
{

class Points : public ElementContainer
{
public:

    enum class DetachOptions
    {
        DoNotGenerateDebris = 0,
        GenerateDebris = 1,

        DoNotFireDestroyEvent = 0,
        FireDestroyEvent = 2,
    };

    /*
     * The types of ephemeral particles.
     */
    enum class EphemeralType
    {
        None,   // Not an ephemeral particle (or not an active ephemeral particle)
        AirBubble,
        Debris,
        Smoke,
        Sparkle
    };

    /*
     * The state required for repairing particles.
     */
    struct RepairState
    {
        // Each point is only allowed one role (attractor or attracted) per session step;
        // these keep track of when they last had each role
        RepairSessionId LastAttractorSessionId;
        RepairSessionStepId LastAttractorSessionStepId;
        RepairSessionId LastAttractedSessionId;
        RepairSessionStepId LastAttractedSessionStepId;

        // The number of consecutive steps - in this session - that this point has been
        // acting as attracted for
        std::uint64_t CurrentAttractedNumberOfSteps;

        RepairState()
            : LastAttractorSessionId(0)
            , LastAttractorSessionStepId(0)
            , LastAttractedSessionId(0)
            , LastAttractedSessionStepId(0)
            , CurrentAttractedNumberOfSteps(0)
        {}
    };

private:

    /*
     * Packed precalculated buoyancy coefficients.
     */
    struct BuoyancyCoefficients
    {
        float Coefficient1; // Temperature-independent
        float Coefficient2; // Temperature-dependent

        BuoyancyCoefficients(
            float coefficient1,
            float coefficient2)
            : Coefficient1(coefficient1)
            , Coefficient2(coefficient2)
        {}
    };

    /*
     * The combustion state.
     */
    struct CombustionState
    {
    public:

        enum class StateType
        {
            NotBurning,
            Developing_1,
            Developing_2,
            Burning,
            Extinguishing_Consumed,
            Extinguishing_SmotheredRain,
            Extinguishing_SmotheredWater,
            Exploded
        };

    public:

        StateType State;

        float FlameDevelopment;
        float MaxFlameDevelopment;
        float NextSmokeEmissionSimulationTimestamp;

        // The current flame vector, which provides direction and magnitude
        // of the flame quad. Slowly converges to the target vector, which
        // is the resultant of buoyancy making the flame upwards, added to
        // the particle's current velocity
        vec2f FlameVector;

        CombustionState()
        {
            Reset();
        }

        inline void Reset()
        {
            State = StateType::NotBurning;
            FlameDevelopment = 0.0f;
            MaxFlameDevelopment = 0.0f;
            NextSmokeEmissionSimulationTimestamp = 0.0f;
            FlameVector = vec2f(0.0f, 1.0f);
        }
    };

    /*
     * The state of ephemeral particles.
     */
    union EphemeralState
    {
        struct AirBubbleState
        {
            float VortexAmplitude;
            float NormalizedVortexAngularVelocity;

            float CurrentDeltaY;
            float Progress;
            float LastVortexValue;

            AirBubbleState()
            {}

            AirBubbleState(
                float vortexAmplitude,
                float vortexPeriod)
                : VortexAmplitude(vortexAmplitude)
                , NormalizedVortexAngularVelocity(1.0f / vortexPeriod) // (2PI/vortexPeriod)/2PI
                , CurrentDeltaY(0.0f)
                , Progress(0.0f)
                , LastVortexValue(0.0f)
            {}
        };

        struct DebrisState
        {
        };

        struct SmokeState
        {
            enum class GrowthType
            {
                Slow,
                Fast
            };

            Render::GenericMipMappedTextureGroups TextureGroup;
            GrowthType Growth;
            float PersonalitySeed;
            float LifetimeProgress;
            float ScaleProgress;

            SmokeState()
                : TextureGroup(Render::GenericMipMappedTextureGroups::SmokeLight) // Arbitrary
                , Growth(GrowthType::Slow) // Arbitrary
                , PersonalitySeed(0.0f)
                , LifetimeProgress(0.0f)
                , ScaleProgress(0.0f)
            {}

            SmokeState(
                Render::GenericMipMappedTextureGroups textureGroup,
                GrowthType growth,
                float personalitySeed)
                : TextureGroup(textureGroup)
                , Growth(growth)
                , PersonalitySeed(personalitySeed)
                , LifetimeProgress(0.0f)
                , ScaleProgress(0.0f)
            {}
        };

        struct SparkleState
        {
            float Progress;

            SparkleState()
                : Progress(0.0f)
            {}
        };

        AirBubbleState AirBubble;
        DebrisState Debris;
        SmokeState Smoke;
        SparkleState Sparkle;

        EphemeralState(AirBubbleState airBubble)
            : AirBubble(airBubble)
        {}

        EphemeralState(DebrisState debris)
            : Debris(debris)
        {}

        EphemeralState(SmokeState smoke)
            : Smoke(smoke)
        {}

        EphemeralState(SparkleState sparkle)
            : Sparkle(sparkle)
        {}
    };

    /*
     * First cluster of ephemeral particle attributes that are used
     * always together, mostly when looking for free ephemeral
     * particle slots.
     */
    struct EphemeralParticleAttributes1
    {
        EphemeralType Type;
        float StartSimulationTime;

        EphemeralParticleAttributes1()
            : Type(EphemeralType::None)
            , StartSimulationTime(0.0f)
        {}
    };

    /*
     * Second cluster of ephemeral particle attributes that are used
     * (almost) always together.
     */
    struct EphemeralParticleAttributes2
    {
        EphemeralState State;
        float MaxSimulationLifetime;

        EphemeralParticleAttributes2()
            : State(EphemeralState::DebrisState()) // Arbitrary
            , MaxSimulationLifetime(0.0f)
        {}
    };

    /*
     * The metadata of a single spring connected to a point.
     */
    struct ConnectedSpring
    {
        ElementIndex SpringIndex;
        ElementIndex OtherEndpointIndex;

        ConnectedSpring()
            : SpringIndex(NoneElementIndex)
            , OtherEndpointIndex(NoneElementIndex)
        {}

        ConnectedSpring(
            ElementIndex springIndex,
            ElementIndex otherEndpointIndex)
            : SpringIndex(springIndex)
            , OtherEndpointIndex(otherEndpointIndex)
        {}
    };

    /*
     * The metadata of all the springs connected to a point.
     */
    struct ConnectedSpringsVector
    {
        FixedSizeVector<ConnectedSpring, GameParameters::MaxSpringsPerPoint> ConnectedSprings;
        size_t OwnedConnectedSpringsCount;

        ConnectedSpringsVector()
            : ConnectedSprings()
            , OwnedConnectedSpringsCount(0u)
        {}

        inline void ConnectSpring(
            ElementIndex springElementIndex,
            ElementIndex otherEndpointElementIndex,
            bool isAtOwner)
        {
            // Add so that all springs owned by this point come first
            if (isAtOwner)
            {
                ConnectedSprings.emplace_front(springElementIndex, otherEndpointElementIndex);
                ++OwnedConnectedSpringsCount;
            }
            else
            {
                ConnectedSprings.emplace_back(springElementIndex, otherEndpointElementIndex);
            }
        }

        inline void DisconnectSpring(
            ElementIndex springElementIndex,
            bool isAtOwner)
        {
            bool found = ConnectedSprings.erase_first(
                [springElementIndex](ConnectedSpring const & c)
                {
                    return c.SpringIndex == springElementIndex;
                });

            assert(found);
            (void)found;

            // Update count of owned springs, if this spring is owned
            if (isAtOwner)
            {
                assert(OwnedConnectedSpringsCount > 0);
                --OwnedConnectedSpringsCount;
            }
        }
    };

    /*
     * The metadata of all the triangles connected to a point.
     */
    struct ConnectedTrianglesVector
    {
        FixedSizeVector<ElementIndex, GameParameters::MaxTrianglesPerPoint> ConnectedTriangles;
        size_t OwnedConnectedTrianglesCount;

        ConnectedTrianglesVector()
            : ConnectedTriangles()
            , OwnedConnectedTrianglesCount(0u)
        {}

        inline void ConnectTriangle(
            ElementIndex triangleElementIndex,
            bool isAtOwner)
        {
            // Add so that all triangles owned by this point come first
            if (isAtOwner)
            {
                ConnectedTriangles.emplace_front(triangleElementIndex);
                ++OwnedConnectedTrianglesCount;
            }
            else
            {
                ConnectedTriangles.emplace_back(triangleElementIndex);
            }
        }

        inline void DisconnectTriangle(
            ElementIndex triangleElementIndex,
            bool isAtOwner)
        {
            bool found = ConnectedTriangles.erase_first(
                [triangleElementIndex](ElementIndex c)
                {
                    return c == triangleElementIndex;
                });

            assert(found);
            (void)found;

            // Update count of owned triangles, if this triangle is owned
            if (isAtOwner)
            {
                assert(OwnedConnectedTrianglesCount > 0);
                --OwnedConnectedTrianglesCount;
            }
        }
    };

    /*
     * The materials of this point.
     */
    struct Materials
    {
        StructuralMaterial const * Structural; // The only reason this is a pointer is that placeholders have no material
        ElectricalMaterial const * Electrical;

        Materials(
            StructuralMaterial const * structural,
            ElectricalMaterial const * electrical)
            : Structural(structural)
            , Electrical(electrical)
        {}
    };

public:

    Points(
        ElementCount shipPointCount,
        World & parentWorld,
        MaterialDatabase const & materialDatabase,
        std::shared_ptr<GameEventDispatcher> gameEventDispatcher,
        GameParameters const & gameParameters)
        : ElementContainer(make_aligned_float_element_count(shipPointCount) + GameParameters::MaxEphemeralParticles)
        //////////////////////////////////
        // Buffers
        //////////////////////////////////
        , mIsDamagedBuffer(mBufferElementCount, shipPointCount, false)
        // Materials
        , mMaterialsBuffer(mBufferElementCount, shipPointCount, Materials(nullptr, nullptr))
        , mIsRopeBuffer(mBufferElementCount, shipPointCount, false)
        // Mechanical dynamics
        , mPositionBuffer(mBufferElementCount, shipPointCount, vec2f::zero())
        , mVelocityBuffer(mBufferElementCount, shipPointCount, vec2f::zero())
        , mForceBuffer(mBufferElementCount, shipPointCount, vec2f::zero())
        , mAugmentedMaterialMassBuffer(mBufferElementCount, shipPointCount, 1.0f)
        , mMassBuffer(mBufferElementCount, shipPointCount, 1.0f)
        , mMaterialBuoyancyVolumeFillBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mDecayBuffer(mBufferElementCount, shipPointCount, 1.0f)
        , mIsDecayBufferDirty(true)
        , mFrozenCoefficientBuffer(mBufferElementCount, shipPointCount, 1.0f)
        , mIntegrationFactorTimeCoefficientBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mBuoyancyCoefficientsBuffer(mBufferElementCount, shipPointCount, BuoyancyCoefficients(0.0f, 0.0f))
        , mIntegrationFactorBuffer(mBufferElementCount, shipPointCount, vec2f::zero())
        , mForceRenderBuffer(mBufferElementCount, shipPointCount, vec2f::zero())
        // Water dynamics
        , mMaterialIsHullBuffer(mBufferElementCount, shipPointCount, false)
        , mMaterialWaterIntakeBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mMaterialWaterRestitutionBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mMaterialWaterDiffusionSpeedBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mWaterBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mWaterVelocityBuffer(mBufferElementCount, shipPointCount, vec2f::zero())
        , mWaterMomentumBuffer(mBufferElementCount, shipPointCount, vec2f::zero())
        , mCumulatedIntakenWater(mBufferElementCount, shipPointCount, 0.0f)
        , mIsLeakingBuffer(mBufferElementCount, shipPointCount, false)
        , mFactoryIsLeakingBuffer(mBufferElementCount, shipPointCount, false)
        // Heat dynamics
        , mTemperatureBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mMaterialHeatCapacityReciprocalBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mMaterialThermalExpansionCoefficientBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mMaterialIgnitionTemperatureBuffer(mBufferElementCount, shipPointCount, 0.0f)
        , mMaterialCombustionTypeBuffer(mBufferElementCount, shipPointCount, StructuralMaterial::MaterialCombustionType::Combustion) // Arbitrary
        , mCombustionStateBuffer(mBufferElementCount, shipPointCount, CombustionState())
        // Electrical dynamics
        , mElectricalElementBuffer(mBufferElementCount, shipPointCount, NoneElementIndex)
        , mLightBuffer(mBufferElementCount, shipPointCount, 0.0f)
        // Wind dynamics
        , mMaterialWindReceptivityBuffer(mBufferElementCount, shipPointCount, 0.0f)
        // Rust dynamics
        , mMaterialRustReceptivityBuffer(mBufferElementCount, shipPointCount, 0.0f)
        // Ephemeral particles
        , mEphemeralParticleAttributes1Buffer(mBufferElementCount, shipPointCount, EphemeralParticleAttributes1())
        , mEphemeralParticleAttributes2Buffer(mBufferElementCount, shipPointCount, EphemeralParticleAttributes2())
        // Structure
        , mConnectedSpringsBuffer(mBufferElementCount, shipPointCount, ConnectedSpringsVector())
        , mFactoryConnectedSpringsBuffer(mBufferElementCount, shipPointCount, ConnectedSpringsVector())
        , mConnectedTrianglesBuffer(mBufferElementCount, shipPointCount, ConnectedTrianglesVector())
        , mFactoryConnectedTrianglesBuffer(mBufferElementCount, shipPointCount, ConnectedTrianglesVector())
        // Connected component and plane ID
        , mConnectedComponentIdBuffer(mBufferElementCount, shipPointCount, NoneConnectedComponentId)
        , mPlaneIdBuffer(mBufferElementCount, shipPointCount, NonePlaneId)
        , mPlaneIdFloatBuffer(mBufferElementCount, shipPointCount, 0.0)
        , mIsPlaneIdBufferNonEphemeralDirty(true)
        , mIsPlaneIdBufferEphemeralDirty(true)
        , mCurrentConnectivityVisitSequenceNumberBuffer(mBufferElementCount, shipPointCount, SequenceNumber())
        // Repair
        , mRepairStateBuffer(mBufferElementCount, shipPointCount, RepairState())
		// Randomness
		, mRandomNormalizedUniformFloatBuffer(mBufferElementCount, shipPointCount, [](size_t){ return GameRandomEngine::GetInstance().GenerateNormalizedUniformReal(); })
        // Immutable render attributes
        , mColorBuffer(mBufferElementCount, shipPointCount, vec4f::zero())
        , mIsWholeColorBufferDirty(true)
        , mTextureCoordinatesBuffer(mBufferElementCount, shipPointCount, vec2f::zero())
        , mIsTextureCoordinatesBufferDirty(true)
        //////////////////////////////////
        // Container
        //////////////////////////////////
        , mRawShipPointCount(shipPointCount)
        , mAlignedShipPointCount(make_aligned_float_element_count(shipPointCount))
        , mEphemeralPointCount(GameParameters::MaxEphemeralParticles)
        , mAllPointCount(mAlignedShipPointCount + mEphemeralPointCount)
        , mParentWorld(parentWorld)
        , mMaterialDatabase(materialDatabase)
        , mGameEventHandler(std::move(gameEventDispatcher))
        , mShipPhysicsHandler(nullptr)
        , mHaveWholeBuffersBeenUploadedOnce(false)
        , mCurrentNumMechanicalDynamicsIterations(gameParameters.NumMechanicalDynamicsIterations<float>())
        , mCurrentCumulatedIntakenWaterThresholdForAirBubbles(gameParameters.CumulatedIntakenWaterThresholdForAirBubbles)
        , mFloatBufferAllocator(mBufferElementCount)
        , mVec2fBufferAllocator(mBufferElementCount)
        , mCombustionIgnitionCandidates(mRawShipPointCount)
        , mCombustionExplosionCandidates(mRawShipPointCount)
        , mBurningPoints()
        , mFreeEphemeralParticleSearchStartIndex(mAlignedShipPointCount)
        , mAreEphemeralPointsDirtyForRendering(false)
    {
    }

    Points(Points && other) = default;

    /*
     * Returns an iterator for the (unaligned) ship (i.e. non-ephemeral) points only.
     */
    inline auto const RawShipPoints() const
    {
        return ElementIndexRangeIterable(0, mRawShipPointCount);
    }

    ElementCount GetRawShipPointCount() const
    {
        return mRawShipPointCount;
    }

    ElementCount GetAlignedShipPointCount() const
    {
        return mAlignedShipPointCount;
    }

    /*
     * Returns a reverse iterator for the (unaligned) ship (i.e. non-ephemeral) points only.
     */
    inline auto const RawShipPointsReverse() const
    {
        return ElementIndexReverseRangeIterable(0, mRawShipPointCount);
    }

    /*
     * Returns an iterator for the ephemeral points only.
     */
    inline auto const EphemeralPoints() const
    {
        return ElementIndexRangeIterable(mAlignedShipPointCount, mAllPointCount);
    }

    /*
     * Returns a flag indicating whether the point is active in the world.
     *
     * Active points are all non-ephemeral points and non-expired ephemeral points.
     */
    inline bool IsActive(ElementIndex pointIndex) const
    {
        return pointIndex < mRawShipPointCount
            || EphemeralType::None != mEphemeralParticleAttributes1Buffer[pointIndex].Type;
    }

    inline bool IsEphemeral(ElementIndex pointIndex) const
    {
        return pointIndex >= mAlignedShipPointCount;
    }

    void RegisterShipPhysicsHandler(IShipPhysicsHandler * shipPhysicsHandler)
    {
        mShipPhysicsHandler = shipPhysicsHandler;
    }

    void Add(
        vec2f const & position,
        StructuralMaterial const & structuralMaterial,
        ElectricalMaterial const * electricalMaterial,
        bool isRope,
        ElementIndex electricalElementIndex,
        bool isLeaking,
        vec4f const & color,
        vec2f const & textureCoordinates,
		float randomNormalizedUniformFloat);

    void CreateEphemeralParticleAirBubble(
        vec2f const & position,
        float temperature,
        float vortexAmplitude,
        float vortexPeriod,
        float currentSimulationTime,
        PlaneId planeId);

    void CreateEphemeralParticleDebris(
        vec2f const & position,
        vec2f const & velocity,
        StructuralMaterial const & structuralMaterial,
        float currentSimulationTime,
        float maxSimulationLifetime,
        PlaneId planeId);

    void CreateEphemeralParticleLightSmoke(
        vec2f const & position,
        float temperature,
        float currentSimulationTime,
        PlaneId planeId,
        GameParameters const & gameParameters)
    {
        CreateEphemeralParticleSmoke(
            Render::GenericMipMappedTextureGroups::SmokeLight,
            EphemeralState::SmokeState::GrowthType::Slow,
            position,
            temperature,
            currentSimulationTime,
            planeId,
            gameParameters);
    }

    void CreateEphemeralParticleHeavySmoke(
        vec2f const & position,
        float temperature,
        float currentSimulationTime,
        PlaneId planeId,
        GameParameters const & gameParameters)
    {
        CreateEphemeralParticleSmoke(
            Render::GenericMipMappedTextureGroups::SmokeDark,
            EphemeralState::SmokeState::GrowthType::Fast,
            position,
            temperature,
            currentSimulationTime,
            planeId,
            gameParameters);
    }

    void CreateEphemeralParticleSmoke(
        Render::GenericMipMappedTextureGroups textureGroup,
        EphemeralState::SmokeState::GrowthType growth,
        vec2f const & position,
        float temperature,
        float currentSimulationTime,
        PlaneId planeId,
        GameParameters const & gameParameters);

    void CreateEphemeralParticleSparkle(
        vec2f const & position,
        vec2f const & velocity,
        StructuralMaterial const & structuralMaterial,
        float currentSimulationTime,
        float maxSimulationLifetime,
        PlaneId planeId);

    void DestroyEphemeralParticle(
        ElementIndex pointElementIndex);

    void Detach(
        ElementIndex pointElementIndex,
        vec2f const & detachVelocity,
        DetachOptions detachOptions,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void Restore(ElementIndex pointElementIndex);

    void OnOrphaned(ElementIndex pointElementIndex);

    void UpdateForGameParameters(GameParameters const & gameParameters);

    void UpdateCombustionLowFrequency(
        ElementIndex pointOffset,
        ElementIndex pointStride,
        float currentSimulationTime,
        float dt,
		Storm::Parameters const & stormParameters,
        GameParameters const & gameParameters);

    void UpdateCombustionHighFrequency(
        float currentSimulationTime,
        float dt,
        GameParameters const & gameParameters);

    void ReorderBurningPointsForDepth();

    void UpdateEphemeralParticles(
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void Query(ElementIndex pointElementIndex) const;

    //
    // Render
    //

    void UploadAttributes(
        ShipId shipId,
        Render::RenderContext & renderContext) const;

    void UploadNonEphemeralPointElements(
        ShipId shipId,
        Render::RenderContext & renderContext) const;

    void UploadFlames(
        ShipId shipId,
        float windSpeedMagnitude,
        Render::RenderContext & renderContext) const;

    void UploadVectors(
        ShipId shipId,
        Render::RenderContext & renderContext) const;

    inline bool AreEphemeralPointsDirtyForRendering() const noexcept
    {
        return mAreEphemeralPointsDirtyForRendering;
    }

    void UploadEphemeralParticles(
        ShipId shipId,
        Render::RenderContext & renderContext) const;

public:

    //
    // IsDamaged (i.e. whether it has been irrevocable modified, such as detached or
    // set to leaking)
    //

    bool IsDamaged(ElementIndex springElementIndex) const
    {
        return mIsDamagedBuffer[springElementIndex];
    }

    //
    // Materials
    //

    StructuralMaterial const & GetStructuralMaterial(ElementIndex pointElementIndex) const
    {
        // If this method is invoked, this is not a placeholder
        assert(nullptr != mMaterialsBuffer[pointElementIndex].Structural);
        return *(mMaterialsBuffer[pointElementIndex].Structural);
    }

    ElectricalMaterial const * GetElectricalMaterial(ElementIndex pointElementIndex) const
    {
        return mMaterialsBuffer[pointElementIndex].Electrical;
    }

    bool IsRope(ElementIndex pointElementIndex) const
    {
        return mIsRopeBuffer[pointElementIndex];
    }

    //
    // Dynamics
    //

    vec2f const & GetPosition(ElementIndex pointElementIndex) const noexcept
    {
        return mPositionBuffer[pointElementIndex];
    }

    vec2f & GetPosition(ElementIndex pointElementIndex) noexcept
    {
        return mPositionBuffer[pointElementIndex];
    }

    vec2f * restrict GetPositionBufferAsVec2()
    {
        return mPositionBuffer.data();
    }

    float * restrict GetPositionBufferAsFloat()
    {
        return reinterpret_cast<float *>(mPositionBuffer.data());
    }

    std::shared_ptr<Buffer<vec2f>> MakePositionBufferCopy()
    {
        auto positionBufferCopy = mVec2fBufferAllocator.Allocate();
        positionBufferCopy->copy_from(mPositionBuffer);

        return positionBufferCopy;
    }

    vec2f const & GetVelocity(ElementIndex pointElementIndex) const noexcept
    {
        return mVelocityBuffer[pointElementIndex];
    }

    vec2f & GetVelocity(ElementIndex pointElementIndex) noexcept
    {
        return mVelocityBuffer[pointElementIndex];
    }

    vec2f * restrict GetVelocityBufferAsVec2()
    {
        return mVelocityBuffer.data();
    }

    float * restrict GetVelocityBufferAsFloat()
    {
        return reinterpret_cast<float *>(mVelocityBuffer.data());
    }

    void SetVelocity(
        ElementIndex pointElementIndex,
        vec2f const & velocity) noexcept
    {
        mVelocityBuffer[pointElementIndex] = velocity;
    }

    /*
     * Adds the velocities gained beetween a previous position snapshot and the current
     * positions to the velocity buffer.
     */
    void UpdateVelocitiesFromPositionDeltas(
        std::shared_ptr<Buffer<vec2f>> const & previousPositions,
        float dt)
    {
        vec2f const * const restrict previousPositionsBuffer = previousPositions->data();
        vec2f const * const restrict currentPositionsBuffer = mPositionBuffer.data();
        vec2f * const restrict velocityBuffer = mVelocityBuffer.data();
        for (ElementIndex p = 0; p < mBufferElementCount; ++p)
        {
            velocityBuffer[p] +=
                (currentPositionsBuffer[p] - previousPositionsBuffer[p])
                / dt;
        }
    }

    vec2f const & GetForce(ElementIndex pointElementIndex) const noexcept
    {
        return mForceBuffer[pointElementIndex];
    }

    vec2f & GetForce(ElementIndex pointElementIndex) noexcept
    {
        return mForceBuffer[pointElementIndex];
    }

    void AddForce(
        ElementIndex pointElementIndex,
        vec2f const & force) noexcept
    {
        mForceBuffer[pointElementIndex] += force;
    }

    float GetAugmentedMaterialMass(ElementIndex pointElementIndex) const
    {
        return mAugmentedMaterialMassBuffer[pointElementIndex];
    }

    void AugmentMaterialMass(
        ElementIndex pointElementIndex,
        float offset,
        Springs & springs);

    /*
     * Returns the total mass of the point, which equals the point's material's mass with
     * all modifiers (offsets, water, etc.).
     *
     * Only valid after a call to UpdateTotalMasses() and when
     * neither water quantities nor masses have changed since then.
     */
    float GetMass(ElementIndex pointElementIndex) noexcept
    {
        return mMassBuffer[pointElementIndex];
    }

    void UpdateMasses(GameParameters const & gameParameters);

    float GetDecay(ElementIndex pointElementIndex) const
    {
        return mDecayBuffer[pointElementIndex];
    }

    void SetDecay(
        ElementIndex pointElementIndex,
        float value)
    {
        mDecayBuffer[pointElementIndex] = value;
    }

    void MarkDecayBufferAsDirty()
    {
        mIsDecayBufferDirty = true;
    }

    bool IsPinned(ElementIndex pointElementIndex) const
    {
        return (mFrozenCoefficientBuffer[pointElementIndex] == 0.0f);
    }

    void Pin(ElementIndex pointElementIndex)
    {
        assert(1.0f == mFrozenCoefficientBuffer[pointElementIndex]);

        Freeze(pointElementIndex); // Recalculates integration coefficient
    }

    void Unpin(ElementIndex pointElementIndex)
    {
        assert(0.0f == mFrozenCoefficientBuffer[pointElementIndex]);

        Thaw(pointElementIndex); // Recalculates integration coefficient
    }

    BuoyancyCoefficients const & GetBuoyancyCoefficients(ElementIndex pointElementIndex)
    {
        return mBuoyancyCoefficientsBuffer[pointElementIndex];
    }

    /*
     * The integration factor is the quantity which, when multiplied with the force on the point,
     * yields the change in position that occurs during a time interval equal to the dynamics simulation step.
     *
     * Only valid after a call to UpdateMasses() and when
     * neither water quantities nor masses have changed since then.
     */
    vec2f GetIntegrationFactor(ElementIndex pointElementIndex) const
    {
        return mIntegrationFactorBuffer[pointElementIndex];
    }

    float * restrict GetIntegrationFactorBufferAsFloat()
    {
        return reinterpret_cast<float *>(mIntegrationFactorBuffer.data());
    }

    // Changes the point's dynamics so that it freezes in place
    // and becomes oblivious to forces
    void Freeze(ElementIndex pointElementIndex)
    {
        // Remember this point is now frozen
        mFrozenCoefficientBuffer[pointElementIndex] = 0.0f;

        // Recalc integration factor time coefficient, freezing point
        mIntegrationFactorTimeCoefficientBuffer[pointElementIndex] = CalculateIntegrationFactorTimeCoefficient(
            mFrozenCoefficientBuffer[pointElementIndex]);

        // Also zero-out velocity, wiping all traces of this point moving
        mVelocityBuffer[pointElementIndex] = vec2f(0.0f, 0.0f);
    }

    // Changes the point's dynamics so that the point reacts again to forces
    void Thaw(ElementIndex pointElementIndex)
    {
        // This point is not frozen anymore
        mFrozenCoefficientBuffer[pointElementIndex] = 1.0f;

        // Re-populate its integration factor time coefficient, thawing point
        mIntegrationFactorTimeCoefficientBuffer[pointElementIndex] = CalculateIntegrationFactorTimeCoefficient(
            mFrozenCoefficientBuffer[pointElementIndex]);
    }


    float * restrict GetForceBufferAsFloat()
    {
        return reinterpret_cast<float *>(mForceBuffer.data());
    }

    void CopyForceBufferToForceRenderBuffer()
    {
        mForceRenderBuffer.copy_from(mForceBuffer);
    }

    //
    // Water dynamics
    //

    bool GetMaterialIsHull(ElementIndex pointElementIndex) const
    {
        return mMaterialIsHullBuffer[pointElementIndex];
    }

    float GetMaterialWaterIntake(ElementIndex pointElementIndex) const
    {
        return mMaterialWaterIntakeBuffer[pointElementIndex];
    }

    float GetMaterialWaterRestitution(ElementIndex pointElementIndex) const
    {
        return mMaterialWaterRestitutionBuffer[pointElementIndex];
    }

    float GetMaterialWaterDiffusionSpeed(ElementIndex pointElementIndex) const
    {
        return mMaterialWaterDiffusionSpeedBuffer[pointElementIndex];
    }

    float * restrict GetWaterBufferAsFloat()
    {
        return mWaterBuffer.data();
    }

    float GetWater(ElementIndex pointElementIndex) const
    {
        return mWaterBuffer[pointElementIndex];
    }

    float & GetWater(ElementIndex pointElementIndex)
    {
        return mWaterBuffer[pointElementIndex];
    }

    bool IsWet(
        ElementIndex pointElementIndex,
        float threshold) const
    {
        return mWaterBuffer[pointElementIndex] > threshold;
    }

    std::shared_ptr<Buffer<float>> MakeWaterBufferCopy()
    {
        auto waterBufferCopy = mFloatBufferAllocator.Allocate();
        waterBufferCopy->copy_from(mWaterBuffer);

        return waterBufferCopy;
    }

    void UpdateWaterBuffer(std::shared_ptr<Buffer<float>> newWaterBuffer)
    {
        mWaterBuffer.copy_from(*newWaterBuffer);
    }

    vec2f * restrict GetWaterVelocityBufferAsVec2()
    {
        return mWaterVelocityBuffer.data();
    }

    /*
     * Only valid after a call to UpdateWaterMomentaFromVelocities() and when
     * neither water quantities nor velocities have changed.
     */
    vec2f * restrict GetWaterMomentumBufferAsVec2f()
    {
        return mWaterMomentumBuffer.data();
    }

    void UpdateWaterMomentaFromVelocities()
    {
        float * const restrict waterBuffer = mWaterBuffer.data();
        vec2f * const restrict waterVelocityBuffer = mWaterVelocityBuffer.data();
        vec2f * restrict waterMomentumBuffer = mWaterMomentumBuffer.data();

        // No need to visit ephemerals, as they don't get water
        for (ElementIndex p = 0; p < mRawShipPointCount; ++p)
        {
            waterMomentumBuffer[p] =
                waterVelocityBuffer[p]
                * waterBuffer[p];
        }
    }

    void UpdateWaterVelocitiesFromMomenta()
    {
        float * const restrict waterBuffer = mWaterBuffer.data();
        vec2f * restrict waterVelocityBuffer = mWaterVelocityBuffer.data();
        vec2f * const restrict waterMomentumBuffer = mWaterMomentumBuffer.data();

        // No need to visit ephemerals, as they don't get water
        for (ElementIndex p = 0; p < mRawShipPointCount; ++p)
        {
            if (waterBuffer[p] != 0.0f)
            {
                waterVelocityBuffer[p] =
                    waterMomentumBuffer[p]
                    / waterBuffer[p];
            }
            else
            {
                // No mass, no velocity
                waterVelocityBuffer[p] = vec2f::zero();
            }
        }
    }

    float GetCumulatedIntakenWater(ElementIndex pointElementIndex) const
    {
        return mCumulatedIntakenWater[pointElementIndex];
    }

    float & GetCumulatedIntakenWater(ElementIndex pointElementIndex)
    {
        return mCumulatedIntakenWater[pointElementIndex];
    }

    bool IsLeaking(ElementIndex pointElementIndex) const
    {
        return mIsLeakingBuffer[pointElementIndex];
    }

    void Damage(ElementIndex pointElementIndex)
    {
        if (!mMaterialIsHullBuffer[pointElementIndex])
        {
            //
            // Start leaking
            //

            // Set as leaking
            mIsLeakingBuffer[pointElementIndex] = true;

            // Randomize the initial water intaken, so that air bubbles won't come out all at the same moment
            mCumulatedIntakenWater[pointElementIndex] = RandomizeCumulatedIntakenWater(mCurrentCumulatedIntakenWaterThresholdForAirBubbles);
        }

        // Check if it's the first time we get damaged
        if (!mIsDamagedBuffer[pointElementIndex])
        {
            // Invoke handler
            mShipPhysicsHandler->HandlePointDamaged(pointElementIndex);

            // Flag ourselves as damaged
            mIsDamagedBuffer[pointElementIndex] = true;
        }
    }

    //
    // Heat dynamics
    //

    float GetTemperature(ElementIndex pointElementIndex) const
    {
        return mTemperatureBuffer[pointElementIndex];
    }

    float * restrict GetTemperatureBufferAsFloat()
    {
        return mTemperatureBuffer.data();
    }

    void SetTemperature(
        ElementIndex pointElementIndex,
        float value)
    {
        mTemperatureBuffer[pointElementIndex] = value;
    }

    std::shared_ptr<Buffer<float>> MakeTemperatureBufferCopy()
    {
        auto temperatureBufferCopy = mFloatBufferAllocator.Allocate();
        temperatureBufferCopy->copy_from(mTemperatureBuffer);

        return temperatureBufferCopy;
    }

    void UpdateTemperatureBuffer(std::shared_ptr<Buffer<float>> newTemperatureBuffer)
    {
        mTemperatureBuffer.copy_from(*newTemperatureBuffer);
    }

    float GetMaterialHeatCapacityReciprocal(ElementIndex pointElementIndex) const
    {
        return mMaterialHeatCapacityReciprocalBuffer[pointElementIndex];
    }

    /*
     * Checks whether a point is eligible for being extinguished by smothering.
     */
    bool IsBurningForSmothering(ElementIndex pointElementIndex) const
    {
        auto const combustionState = mCombustionStateBuffer[pointElementIndex].State;

        return combustionState == CombustionState::StateType::Burning
            || combustionState == CombustionState::StateType::Developing_1
            || combustionState == CombustionState::StateType::Developing_2
            || combustionState == CombustionState::StateType::Extinguishing_Consumed;
    }

    void SmotherCombustion(
        ElementIndex pointElementIndex,
        bool isWater)
    {
        assert(IsBurningForSmothering(pointElementIndex));

        auto const combustionState = mCombustionStateBuffer[pointElementIndex].State;

        // Notify combustion end - if we are burning
        if (combustionState == CombustionState::StateType::Developing_1
            || combustionState == CombustionState::StateType::Developing_2
            || combustionState == CombustionState::StateType::Burning)
            mGameEventHandler->OnPointCombustionEnd();

        // Transition
        mCombustionStateBuffer[pointElementIndex].State = isWater
            ? CombustionState::StateType::Extinguishing_SmotheredWater
            : CombustionState::StateType::Extinguishing_SmotheredRain;

        // Notify sizzling
        mGameEventHandler->OnCombustionSmothered();
    }

    void AddHeat(
        ElementIndex pointElementIndex,
        float heat) // J
    {
        mTemperatureBuffer[pointElementIndex] +=
            heat
            * GetMaterialHeatCapacityReciprocal(pointElementIndex);
    }

    //
    // Electrical dynamics
    //

    ElementIndex GetElectricalElement(ElementIndex pointElementIndex) const
    {
        return mElectricalElementBuffer[pointElementIndex];
    }

    float GetLight(ElementIndex pointElementIndex) const
    {
        return mLightBuffer[pointElementIndex];
    }

    float *restrict GetLightBufferAsFloat()
    {
        return mLightBuffer.data();
    }

    //
    // Wind dynamics
    //

    float GetMaterialWindReceptivity(ElementIndex pointElementIndex) const
    {
        return mMaterialWindReceptivityBuffer[pointElementIndex];
    }

    //
    // Rust dynamics
    //

    float GetMaterialRustReceptivity(ElementIndex pointElementIndex) const
    {
        return mMaterialRustReceptivityBuffer[pointElementIndex];
    }

    //
    // Ephemeral Particles
    //

    EphemeralType GetEphemeralType(ElementIndex pointElementIndex) const
    {
        return mEphemeralParticleAttributes1Buffer[pointElementIndex].Type;
    }

    //
    // Network
    //

    auto const & GetConnectedSprings(ElementIndex pointElementIndex) const
    {
        return mConnectedSpringsBuffer[pointElementIndex];
    }

    void ConnectSpring(
        ElementIndex pointElementIndex,
        ElementIndex springElementIndex,
        ElementIndex otherEndpointElementIndex,
        bool isAtOwner)
    {
        assert(mFactoryConnectedSpringsBuffer[pointElementIndex].ConnectedSprings.contains(
            [springElementIndex](auto const & cs)
            {
                return cs.SpringIndex == springElementIndex;
            }));

        mConnectedSpringsBuffer[pointElementIndex].ConnectSpring(
            springElementIndex,
            otherEndpointElementIndex,
            isAtOwner);
    }

    void DisconnectSpring(
        ElementIndex pointElementIndex,
        ElementIndex springElementIndex,
        bool isAtOwner)
    {
        mConnectedSpringsBuffer[pointElementIndex].DisconnectSpring(
            springElementIndex,
            isAtOwner);
    }

    auto const & GetFactoryConnectedSprings(ElementIndex pointElementIndex) const
    {
        return mFactoryConnectedSpringsBuffer[pointElementIndex];
    }

    void AddFactoryConnectedSpring(
        ElementIndex pointElementIndex,
        ElementIndex springElementIndex,
        ElementIndex otherEndpointElementIndex,
        bool isAtOwner)
    {
        // Add spring
        mFactoryConnectedSpringsBuffer[pointElementIndex].ConnectSpring(
            springElementIndex,
            otherEndpointElementIndex,
            isAtOwner);

        // Connect spring
        ConnectSpring(
            pointElementIndex,
            springElementIndex,
            otherEndpointElementIndex,
            isAtOwner);
    }

    auto const & GetConnectedTriangles(ElementIndex pointElementIndex) const
    {
        return mConnectedTrianglesBuffer[pointElementIndex];
    }

    void ConnectTriangle(
        ElementIndex pointElementIndex,
        ElementIndex triangleElementIndex,
        bool isAtOwner)
    {
        assert(mFactoryConnectedTrianglesBuffer[pointElementIndex].ConnectedTriangles.contains(
            [triangleElementIndex](auto const & ct)
            {
                return ct == triangleElementIndex;
            }));

        mConnectedTrianglesBuffer[pointElementIndex].ConnectTriangle(
            triangleElementIndex,
            isAtOwner);
    }

    void DisconnectTriangle(
        ElementIndex pointElementIndex,
        ElementIndex triangleElementIndex,
        bool isAtOwner)
    {
        mConnectedTrianglesBuffer[pointElementIndex].DisconnectTriangle(
            triangleElementIndex,
            isAtOwner);
    }

    size_t GetConnectedOwnedTrianglesCount(ElementIndex pointElementIndex) const
    {
        return mConnectedTrianglesBuffer[pointElementIndex].OwnedConnectedTrianglesCount;
    }

    auto const & GetFactoryConnectedTriangles(ElementIndex pointElementIndex) const
    {
        return mFactoryConnectedTrianglesBuffer[pointElementIndex];
    }

    void AddFactoryConnectedTriangle(
        ElementIndex pointElementIndex,
        ElementIndex triangleElementIndex,
        bool isAtOwner)
    {
        // Add triangle
        mFactoryConnectedTrianglesBuffer[pointElementIndex].ConnectTriangle(
            triangleElementIndex,
            isAtOwner);

        // Connect triangle
        ConnectTriangle(
            pointElementIndex,
            triangleElementIndex,
            isAtOwner);
    }

    //
    // Connected components and plane IDs
    //

    ConnectedComponentId GetConnectedComponentId(ElementIndex pointElementIndex) const
    {
        return mConnectedComponentIdBuffer[pointElementIndex];
    }

    void SetConnectedComponentId(
        ElementIndex pointElementIndex,
        ConnectedComponentId connectedComponentId)
    {
        mConnectedComponentIdBuffer[pointElementIndex] = connectedComponentId;
    }

    PlaneId GetPlaneId(ElementIndex pointElementIndex) const
    {
        return mPlaneIdBuffer[pointElementIndex];
    }

    PlaneId * restrict GetPlaneIdBufferAsPlaneId()
    {
        return mPlaneIdBuffer.data();
    }

    void SetPlaneId(
        ElementIndex pointElementIndex,
        PlaneId planeId,
        float planeIdFloat)
    {
        mPlaneIdBuffer[pointElementIndex] = planeId;
        mPlaneIdFloatBuffer[pointElementIndex] = planeIdFloat;
    }

    void MarkPlaneIdBufferNonEphemeralAsDirty()
    {
        mIsPlaneIdBufferNonEphemeralDirty = true;
    }

    SequenceNumber GetCurrentConnectivityVisitSequenceNumber(ElementIndex pointElementIndex) const
    {
        return mCurrentConnectivityVisitSequenceNumberBuffer[pointElementIndex];
    }

    void SetCurrentConnectivityVisitSequenceNumber(
        ElementIndex pointElementIndex,
        SequenceNumber ConnectivityVisitSequenceNumber)
    {
        mCurrentConnectivityVisitSequenceNumberBuffer[pointElementIndex] =
            ConnectivityVisitSequenceNumber;
    }

    //
    // Repair
    //

    RepairState & GetRepairState(ElementIndex pointElementIndex)
    {
        return mRepairStateBuffer[pointElementIndex];
    }

    //
    // Immutable attributes
    //

    vec4f & GetColor(ElementIndex pointElementIndex)
    {
        return mColorBuffer[pointElementIndex];
    }

    // Mostly for debugging
    void MarkColorBufferAsDirty()
    {
        mIsWholeColorBufferDirty = true;
    }


    //
    // Temporary buffer
    //

    std::shared_ptr<Buffer<float>> AllocateWorkBufferFloat()
    {
        return mFloatBufferAllocator.Allocate();
    }

    std::shared_ptr<Buffer<vec2f>> AllocateWorkBufferVec2f()
    {
        return mVec2fBufferAllocator.Allocate();
    }

private:

    static inline float CalculateIntegrationFactorTimeCoefficient(
        float frozenCoefficient)
    {
        return GameParameters::SimulationStepTimeDuration<float>
            * GameParameters::SimulationStepTimeDuration<float>
            * frozenCoefficient;
    }

    static inline BuoyancyCoefficients CalculateBuoyancyCoefficients(
        float buoyancyVolumeFill,
        float thermalExpansionCoefficient)
    {
        float const coefficient1 =
            GameParameters::GravityMagnitude
            * buoyancyVolumeFill
            * (1.0f - thermalExpansionCoefficient * GameParameters::Temperature0);

        float const coefficient2 =
            GameParameters::GravityMagnitude
            * buoyancyVolumeFill
            * thermalExpansionCoefficient;

        return BuoyancyCoefficients(
            coefficient1,
            coefficient2);
    }

    static inline float RandomizeCumulatedIntakenWater(float cumulatedIntakenWaterThresholdForAirBubbles)
    {
        return GameRandomEngine::GetInstance().GenerateUniformReal(
            0.0f,
            cumulatedIntakenWaterThresholdForAirBubbles);
    }

    inline void SetLeaking(ElementIndex pointElementIndex)
    {
        mIsLeakingBuffer[pointElementIndex] = true;

        // Randomize the initial water intaken, so that air bubbles won't come out all at the same moment
        mCumulatedIntakenWater[pointElementIndex] = RandomizeCumulatedIntakenWater(mCurrentCumulatedIntakenWaterThresholdForAirBubbles);
    }

    ElementIndex FindFreeEphemeralParticle(
        float currentSimulationTime,
        bool force);

    inline void ExpireEphemeralParticle(ElementIndex pointElementIndex)
    {
        // Freeze the particle (just to prevent drifting)
        Freeze(pointElementIndex);

        // Hide this particle from ephemeral particles; this will prevent this particle from:
        // - Being rendered
        // - Being updated
        // ...and it will allow its slot to be chosen for a new ephemeral particle
        mEphemeralParticleAttributes1Buffer[pointElementIndex].Type = EphemeralType::None;
    }

private:

    //////////////////////////////////////////////////////////
    // Buffers
    //////////////////////////////////////////////////////////

    // Damage: true when the point has been irrevocably modified
    // (such as detached or set to leaking); only a Restore will
    // make things right again
    Buffer<bool> mIsDamagedBuffer;

    // Materials
    Buffer<Materials> mMaterialsBuffer;
    Buffer<bool> mIsRopeBuffer;

    //
    // Dynamics
    //

    Buffer<vec2f> mPositionBuffer;
    Buffer<vec2f> mVelocityBuffer;
    Buffer<vec2f> mForceBuffer;
    Buffer<float> mAugmentedMaterialMassBuffer; // Structural + Offset
    Buffer<float> mMassBuffer; // Augmented + Water
    Buffer<float> mMaterialBuoyancyVolumeFillBuffer;
    Buffer<float> mDecayBuffer; // 1.0 -> 0.0 (completely decayed)
    bool mutable mIsDecayBufferDirty; // Only tracks non-ephemerals
    Buffer<float> mFrozenCoefficientBuffer; // 1.0: not frozen; 0.0f: frozen
    Buffer<float> mIntegrationFactorTimeCoefficientBuffer; // dt^2 or zero when the point is frozen
    Buffer<BuoyancyCoefficients> mBuoyancyCoefficientsBuffer;

    Buffer<vec2f> mIntegrationFactorBuffer;
    Buffer<vec2f> mForceRenderBuffer;

    //
    // Water dynamics
    //

    Buffer<bool> mMaterialIsHullBuffer;
    Buffer<float> mMaterialWaterIntakeBuffer;
    Buffer<float> mMaterialWaterRestitutionBuffer;
    Buffer<float> mMaterialWaterDiffusionSpeedBuffer;

    // Height of a 1m2 column of water which provides a pressure equivalent to the pressure at
    // this point. Quantity of water is max(water, 1.0)
    Buffer<float> mWaterBuffer;

    // Total velocity of the water at this point
    Buffer<vec2f> mWaterVelocityBuffer;

    // Total momentum of the water at this point
    Buffer<vec2f> mWaterMomentumBuffer;

    // Total amount of water in/out taken which has not yet been
    // utilized for air bubbles
    Buffer<float> mCumulatedIntakenWater;

    // When true, the point is intaking water
    Buffer<bool> mIsLeakingBuffer;
    Buffer<bool> mFactoryIsLeakingBuffer;

    //
    // Heat dynamics
    //

    Buffer<float> mTemperatureBuffer; // Kelvin
    Buffer<float> mMaterialHeatCapacityReciprocalBuffer;
    Buffer<float> mMaterialThermalExpansionCoefficientBuffer;
    Buffer<float> mMaterialIgnitionTemperatureBuffer;
    Buffer<StructuralMaterial::MaterialCombustionType> mMaterialCombustionTypeBuffer;
    Buffer<CombustionState> mCombustionStateBuffer;

    //
    // Electrical dynamics
    //

    // Electrical element (index in ElectricalElements container), if any
    Buffer<ElementIndex> mElectricalElementBuffer;

    // Total illumination, 0.0->1.0
    Buffer<float> mLightBuffer;

    //
    // Wind dynamics
    //

    Buffer<float> mMaterialWindReceptivityBuffer;

    //
    // Rust dynamics
    //

    Buffer<float> mMaterialRustReceptivityBuffer;

    //
    // Ephemeral Particles
    //

    Buffer<EphemeralParticleAttributes1> mEphemeralParticleAttributes1Buffer;
    Buffer<EphemeralParticleAttributes2> mEphemeralParticleAttributes2Buffer;

    //
    // Structure
    //

    Buffer<ConnectedSpringsVector> mConnectedSpringsBuffer;
    Buffer<ConnectedSpringsVector> mFactoryConnectedSpringsBuffer;
    Buffer<ConnectedTrianglesVector> mConnectedTrianglesBuffer;
    Buffer<ConnectedTrianglesVector> mFactoryConnectedTrianglesBuffer;

    //
    // Connectivity
    //

    Buffer<ConnectedComponentId> mConnectedComponentIdBuffer;
    Buffer<PlaneId> mPlaneIdBuffer;
    Buffer<float> mPlaneIdFloatBuffer;
    bool mutable mIsPlaneIdBufferNonEphemeralDirty;
    bool mutable mIsPlaneIdBufferEphemeralDirty;
    Buffer<SequenceNumber> mCurrentConnectivityVisitSequenceNumberBuffer;

    //
    // Repair state
    //

    Buffer<RepairState> mRepairStateBuffer;

	//
	// Randomness
	//

	Buffer<float> mRandomNormalizedUniformFloatBuffer;

    //
    // Immutable render attributes
    //

    Buffer<vec4f> mColorBuffer;
    bool mutable mIsWholeColorBufferDirty;  // Whether or not is dirty since last render upload
    Buffer<vec2f> mTextureCoordinatesBuffer;
    bool mutable mIsTextureCoordinatesBufferDirty; // Whether or not is dirty since last render upload


    //////////////////////////////////////////////////////////
    // Container
    //////////////////////////////////////////////////////////

    // Count of ship points; these are followed by ephemeral points
    ElementCount const mRawShipPointCount;
    ElementCount const mAlignedShipPointCount;

    // Count of ephemeral points
    ElementCount const mEphemeralPointCount;

    // Count of all points (sum of two above, including ship point padding, but not aligned)
    ElementCount const mAllPointCount;

    World & mParentWorld;
    MaterialDatabase const & mMaterialDatabase;
    std::shared_ptr<GameEventDispatcher> const mGameEventHandler;
    IShipPhysicsHandler * mShipPhysicsHandler;

    // Flag remembering whether or not we've uploaded *entire*
    // (as opposed to just non-ephemeral portion) buffers at
    // least once
    bool mutable mHaveWholeBuffersBeenUploadedOnce;

    // The game parameter values that we are current with; changes
    // in the values of these parameters will trigger a re-calculation
    // of pre-calculated coefficients
    float mCurrentNumMechanicalDynamicsIterations;
    float mCurrentCumulatedIntakenWaterThresholdForAirBubbles;

    // Allocators for work buffers
    BufferAllocator<float> mFloatBufferAllocator;
    BufferAllocator<vec2f> mVec2fBufferAllocator;

    // The list of candidates for burning and exploding during combustion;
    // member only to save allocations at use time
    BoundedVector<std::tuple<ElementIndex, float>> mCombustionIgnitionCandidates;
    BoundedVector<std::tuple<ElementIndex, float>> mCombustionExplosionCandidates;

    // The indices of the points that are currently burning
    std::vector<ElementIndex> mBurningPoints;

    // The index at which to start searching for free ephemeral particles
    // (just an optimization over restarting from zero each time)
    ElementIndex mFreeEphemeralParticleSearchStartIndex;

    // Flag remembering whether the set of ephemeral points is dirty
    // (i.e. whether there are more or less points than previously
    // reported to the rendering engine); only tracks dirtyness
    // of ephemeral types that are uploaded as ephemeral points
    // (thus no AirBubbles nor Sparkles, which are both uploaded specially)
    bool mutable mAreEphemeralPointsDirtyForRendering;
};

}

template <> struct is_flag<Physics::Points::DetachOptions> : std::true_type {};
