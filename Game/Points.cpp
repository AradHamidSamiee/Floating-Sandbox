/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-05-06
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Physics.h"

#include <GameCore/GameMath.h>
#include <GameCore/Log.h>
#include <GameCore/PrecalculatedFunction.h>

#include <cmath>
#include <limits>

namespace Physics {

void Points::Add(
    vec2f const & position,
    float water,
    StructuralMaterial const & structuralMaterial,
    ElectricalMaterial const * electricalMaterial,
    bool isRope,
    ElementIndex electricalElementIndex,
    bool isStructurallyLeaking,
    vec4f const & color,
    vec2f const & textureCoordinates,
    float randomNormalizedUniformFloat)
{
    ElementIndex const pointIndex = static_cast<ElementIndex>(mIsDamagedBuffer.GetCurrentPopulatedSize());

    mIsDamagedBuffer.emplace_back(false);
    mMaterialsBuffer.emplace_back(&structuralMaterial, electricalMaterial);
    mIsRopeBuffer.emplace_back(isRope);

    mPositionBuffer.emplace_back(position);
    mVelocityBuffer.emplace_back(vec2f::zero());
    mSpringForceBuffer.emplace_back(vec2f::zero());
    mNonSpringForceBuffer.emplace_back(vec2f::zero());
    mAugmentedMaterialMassBuffer.emplace_back(structuralMaterial.GetMass());
    mMassBuffer.emplace_back(structuralMaterial.GetMass());
    mMaterialBuoyancyVolumeFillBuffer.emplace_back(structuralMaterial.BuoyancyVolumeFill);
    mDecayBuffer.emplace_back(1.0f);
    mFrozenCoefficientBuffer.emplace_back(1.0f);
    mIntegrationFactorTimeCoefficientBuffer.emplace_back(CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations, 1.0f));
    mBuoyancyCoefficientsBuffer.emplace_back(CalculateBuoyancyCoefficients(
        structuralMaterial.BuoyancyVolumeFill,
        structuralMaterial.ThermalExpansionCoefficient));

    mIntegrationFactorBuffer.emplace_back(vec2f::zero());
    mForceRenderBuffer.emplace_back(vec2f::zero());

    mIsHullBuffer.emplace_back(structuralMaterial.IsHull); // Default is from material
    mMaterialWaterIntakeBuffer.emplace_back(structuralMaterial.WaterIntake);
    mMaterialWaterRestitutionBuffer.emplace_back(1.0f - structuralMaterial.WaterRetention);
    mMaterialWaterDiffusionSpeedBuffer.emplace_back(structuralMaterial.WaterDiffusionSpeed);

    mWaterBuffer.emplace_back(water);
    mWaterVelocityBuffer.emplace_back(vec2f::zero());
    mWaterMomentumBuffer.emplace_back(vec2f::zero());
    mCumulatedIntakenWater.emplace_back(0.0f);
    mLeakingCompositeBuffer.emplace_back(LeakingComposite(isStructurallyLeaking));
    if (isStructurallyLeaking)
        SetStructurallyLeaking(pointIndex);
    mFactoryIsStructurallyLeakingBuffer.emplace_back(isStructurallyLeaking);
    mTotalFactoryWetPoints += (water > 0.0f ? 1 : 0);

    // Heat dynamics
    mTemperatureBuffer.emplace_back(GameParameters::Temperature0);
    assert(structuralMaterial.GetHeatCapacity() > 0.0f);
    mMaterialHeatCapacityReciprocalBuffer.emplace_back(1.0f / structuralMaterial.GetHeatCapacity());
    mMaterialThermalExpansionCoefficientBuffer.emplace_back(structuralMaterial.ThermalExpansionCoefficient);
    mMaterialIgnitionTemperatureBuffer.emplace_back(structuralMaterial.IgnitionTemperature);
    mMaterialCombustionTypeBuffer.emplace_back(structuralMaterial.CombustionType);
    mCombustionStateBuffer.emplace_back(CombustionState());

    // Electrical dynamics
    mElectricalElementBuffer.emplace_back(electricalElementIndex);
    mLightBuffer.emplace_back(0.0f);

    // Wind dynamics
    mMaterialWindReceptivityBuffer.emplace_back(structuralMaterial.WindReceptivity);

    // Rust dynamics
    mMaterialRustReceptivityBuffer.emplace_back(structuralMaterial.RustReceptivity);

    // Ephemeral particles
    mEphemeralParticleAttributes1Buffer.emplace_back();
    mEphemeralParticleAttributes2Buffer.emplace_back();

    // Structure
    mConnectedSpringsBuffer.emplace_back();
    mFactoryConnectedSpringsBuffer.emplace_back();
    mConnectedTrianglesBuffer.emplace_back();
    mFactoryConnectedTrianglesBuffer.emplace_back();

    // Connectivity
    mConnectedComponentIdBuffer.emplace_back(NoneConnectedComponentId);
    mPlaneIdBuffer.emplace_back(NonePlaneId);
    mPlaneIdFloatBuffer.emplace_back(0.0f);
    mCurrentConnectivityVisitSequenceNumberBuffer.emplace_back();

    // Repair state
    mRepairStateBuffer.emplace_back();

    // Randomness
    mRandomNormalizedUniformFloatBuffer.emplace_back(randomNormalizedUniformFloat);

    // Immutable render attributes
    mColorBuffer.emplace_back(color);
    mTextureCoordinatesBuffer.emplace_back(textureCoordinates);
}

void Points::CreateEphemeralParticleAirBubble(
    vec2f const & position,
    float temperature,
    float vortexAmplitude,
    float vortexPeriod,
    float currentSimulationTime,
    PlaneId planeId)
{
    // Get a free slot (but don't steal one)
    auto pointIndex = FindFreeEphemeralParticle(currentSimulationTime, false);
    if (NoneElementIndex == pointIndex)
        return; // No luck

    //
    // Store attributes
    //

    StructuralMaterial const & airStructuralMaterial = mMaterialDatabase.GetUniqueStructuralMaterial(StructuralMaterial::MaterialUniqueType::Air);

    // We want to limit the buoyancy applied to air - using 1.0 makes an air particle boost up too quickly
    float constexpr BuoyancyVolumeFill = 0.003f;

    assert(mIsDamagedBuffer[pointIndex] == false); // Ephemeral points are never damaged
    mPositionBuffer[pointIndex] = position;
    mVelocityBuffer[pointIndex] = vec2f::zero();
    assert(mSpringForceBuffer[pointIndex] == vec2f::zero()); // Ephemeral points never participate in springs
    mNonSpringForceBuffer[pointIndex] = vec2f::zero();
    mAugmentedMaterialMassBuffer[pointIndex] = airStructuralMaterial.GetMass();
    mMassBuffer[pointIndex] = airStructuralMaterial.GetMass();
    mMaterialBuoyancyVolumeFillBuffer[pointIndex] = BuoyancyVolumeFill;
    assert(mDecayBuffer[pointIndex] == 1.0f);
    //mDecayBuffer[pointIndex] = 1.0f;
    mFrozenCoefficientBuffer[pointIndex] = 1.0f;
    mIntegrationFactorTimeCoefficientBuffer[pointIndex] = CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations, 1.0f);
    mBuoyancyCoefficientsBuffer[pointIndex] = CalculateBuoyancyCoefficients(
        BuoyancyVolumeFill,
        airStructuralMaterial.ThermalExpansionCoefficient);
    mMaterialsBuffer[pointIndex] = Materials(&airStructuralMaterial, nullptr);

    //mMaterialWaterIntakeBuffer[pointIndex] = airStructuralMaterial.WaterIntake;
    //mMaterialWaterRestitutionBuffer[pointIndex] = 1.0f - airStructuralMaterial.WaterRetention;
    //mMaterialWaterDiffusionSpeedBuffer[pointIndex] = airStructuralMaterial.WaterDiffusionSpeed;
    assert(mWaterBuffer[pointIndex] == 0.0f);
    //mWaterBuffer[pointIndex] = 0.0f;
    assert(!mLeakingCompositeBuffer[pointIndex].IsCumulativelyLeaking);
    //mLeakingCompositeBuffer[pointIndex] = LeakingComposite(false);

    mTemperatureBuffer[pointIndex] = temperature;
    assert(airStructuralMaterial.GetHeatCapacity() > 0.0f);
    mMaterialHeatCapacityReciprocalBuffer[pointIndex] = 1.0f / airStructuralMaterial.GetHeatCapacity();
    mMaterialThermalExpansionCoefficientBuffer[pointIndex] = airStructuralMaterial.ThermalExpansionCoefficient;
    //mMaterialIgnitionTemperatureBuffer[pointIndex] = airStructuralMaterial.IgnitionTemperature;
    //mMaterialCombustionTypeBuffer[pointIndex] = airStructuralMaterial.CombustionType;
    //mCombustionStateBuffer[pointIndex] = CombustionState();

    assert(mLightBuffer[pointIndex] == 0.0f);
    //mLightBuffer[pointIndex] = 0.0f;

    mMaterialWindReceptivityBuffer[pointIndex] = 0.0f; // Air bubbles (underwater) do not care about wind

    assert(mMaterialRustReceptivityBuffer[pointIndex] == 0.0f);
    //mMaterialRustReceptivityBuffer[pointIndex] = 0.0f;

    mEphemeralParticleAttributes1Buffer[pointIndex].Type = EphemeralType::AirBubble;
    mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime = currentSimulationTime;
    mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime = std::numeric_limits<float>::max();
    mEphemeralParticleAttributes2Buffer[pointIndex].State = EphemeralState::AirBubbleState(
        vortexAmplitude,
        vortexPeriod);

    assert(mConnectedComponentIdBuffer[pointIndex] == NoneConnectedComponentId);
    //mConnectedComponentIdBuffer[pointIndex] = NoneConnectedComponentId;
    mPlaneIdBuffer[pointIndex] = planeId;
    mPlaneIdFloatBuffer[pointIndex] = static_cast<float>(planeId);
    mIsPlaneIdBufferEphemeralDirty = true;

    mColorBuffer[pointIndex] = airStructuralMaterial.RenderColor;
    mIsEphemeralColorBufferDirty = true;
}

void Points::CreateEphemeralParticleDebris(
    vec2f const & position,
    vec2f const & velocity,
    StructuralMaterial const & structuralMaterial,
    float currentSimulationTime,
    float maxSimulationLifetime,
    PlaneId planeId)
{
    // Get a free slot (or steal one)
    auto pointIndex = FindFreeEphemeralParticle(currentSimulationTime, true);
    assert(NoneElementIndex != pointIndex);

    //
    // Store attributes
    //

    assert(mIsDamagedBuffer[pointIndex] == false); // Ephemeral points are never damaged
    mPositionBuffer[pointIndex] = position;
    mVelocityBuffer[pointIndex] = velocity;
    assert(mSpringForceBuffer[pointIndex] == vec2f::zero()); // Ephemeral points never participate in springs
    mNonSpringForceBuffer[pointIndex] = vec2f::zero();
    mAugmentedMaterialMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mMaterialBuoyancyVolumeFillBuffer[pointIndex] = 0.0f; // No buoyancy
    assert(mDecayBuffer[pointIndex] == 1.0f);
    //mDecayBuffer[pointIndex] = 1.0f;
    mFrozenCoefficientBuffer[pointIndex] = 1.0f;
    mIntegrationFactorTimeCoefficientBuffer[pointIndex] = CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations, 1.0f);
    mBuoyancyCoefficientsBuffer[pointIndex] = BuoyancyCoefficients(0.0f, 0.0f); // No buoyancy
    mMaterialsBuffer[pointIndex] = Materials(&structuralMaterial, nullptr);

    //mMaterialWaterIntakeBuffer[pointIndex] = structuralMaterial.WaterIntake;
    //mMaterialWaterRestitutionBuffer[pointIndex] = 1.0f - structuralMaterial.WaterRetention;
    //mMaterialWaterDiffusionSpeedBuffer[pointIndex] = structuralMaterial.WaterDiffusionSpeed;
    assert(mWaterBuffer[pointIndex] == 0.0f);
    //mWaterBuffer[pointIndex] = 0.0f;
    assert(!mLeakingCompositeBuffer[pointIndex].IsCumulativelyLeaking);
    //mLeakingCompositeBuffer[pointIndex] = LeakingComposite(false);

    mTemperatureBuffer[pointIndex] = GameParameters::Temperature0;
    assert(structuralMaterial.GetHeatCapacity() > 0.0f);
    mMaterialHeatCapacityReciprocalBuffer[pointIndex] = 1.0f / structuralMaterial.GetHeatCapacity();
    //mMaterialThermalExpansionCoefficientBuffer[pointIndex] = structuralMaterial.ThermalExpansionCoefficient;
    //mMaterialIgnitionTemperatureBuffer[pointIndex] = structuralMaterial.IgnitionTemperature;
    //mMaterialCombustionTypeBuffer[pointIndex] = structuralMaterial.CombustionType;
    //mCombustionStateBuffer[pointIndex] = CombustionState();

    assert(mLightBuffer[pointIndex] == 0.0f);
    //mLightBuffer[pointIndex] = 0.0f;

    mMaterialWindReceptivityBuffer[pointIndex] = 3.0f; // Debris are susceptible to wind

    assert(mMaterialRustReceptivityBuffer[pointIndex] == 0.0f);
    //mMaterialRustReceptivityBuffer[pointIndex] = 0.0f;

    mEphemeralParticleAttributes1Buffer[pointIndex].Type = EphemeralType::Debris;
    mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime = currentSimulationTime;
    mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime = maxSimulationLifetime;
    mEphemeralParticleAttributes2Buffer[pointIndex].State = EphemeralState::DebrisState();

    assert(mConnectedComponentIdBuffer[pointIndex] == NoneConnectedComponentId);
    //mConnectedComponentIdBuffer[pointIndex] = NoneConnectedComponentId;
    mPlaneIdBuffer[pointIndex] = planeId;
    mPlaneIdFloatBuffer[pointIndex] = static_cast<float>(planeId);
    mIsPlaneIdBufferEphemeralDirty = true;

    mColorBuffer[pointIndex] = structuralMaterial.RenderColor;
    mIsEphemeralColorBufferDirty = true;

    // Remember that ephemeral points are dirty now
    mAreEphemeralPointsDirtyForRendering = true;
}

void Points::CreateEphemeralParticleSmoke(
    Render::GenericMipMappedTextureGroups textureGroup,
    EphemeralState::SmokeState::GrowthType growth,
    vec2f const & position,
    float temperature,
    float currentSimulationTime,
    PlaneId planeId,
    GameParameters const & gameParameters)
{
    // Get a free slot (or steal one)
    auto pointIndex = FindFreeEphemeralParticle(currentSimulationTime, true);
    assert(NoneElementIndex != pointIndex);

    // Choose a lifetime
    float const maxSimulationLifetime =
        gameParameters.SmokeParticleLifetimeAdjustment
        * GameRandomEngine::GetInstance().GenerateUniformReal(
            GameParameters::MinSmokeParticlesLifetime,
            GameParameters::MaxSmokeParticlesLifetime);

    //
    // Store attributes
    //

    StructuralMaterial const & airStructuralMaterial = mMaterialDatabase.GetUniqueStructuralMaterial(StructuralMaterial::MaterialUniqueType::Air);

    float constexpr BuoyancyVolumeFill = 1.0f;

    assert(mIsDamagedBuffer[pointIndex] == false); // Ephemeral points are never damaged
    mPositionBuffer[pointIndex] = position;
    mVelocityBuffer[pointIndex] = vec2f::zero();
    assert(mSpringForceBuffer[pointIndex] == vec2f::zero()); // Ephemeral points never participate in springs
    mNonSpringForceBuffer[pointIndex] = vec2f::zero();
    mAugmentedMaterialMassBuffer[pointIndex] = airStructuralMaterial.GetMass();
    mMassBuffer[pointIndex] = airStructuralMaterial.GetMass();
    mMaterialBuoyancyVolumeFillBuffer[pointIndex] = BuoyancyVolumeFill;
    assert(mDecayBuffer[pointIndex] == 1.0f);
    //mDecayBuffer[pointIndex] = 1.0f;
    mFrozenCoefficientBuffer[pointIndex] = 1.0f;
    mIntegrationFactorTimeCoefficientBuffer[pointIndex] = CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations, 1.0f);
    mBuoyancyCoefficientsBuffer[pointIndex] = CalculateBuoyancyCoefficients(
        BuoyancyVolumeFill,
        airStructuralMaterial.ThermalExpansionCoefficient);
    mMaterialsBuffer[pointIndex] = Materials(&airStructuralMaterial, nullptr);

    //mMaterialWaterIntakeBuffer[pointIndex] = airStructuralMaterial.WaterIntake;
    //mMaterialWaterRestitutionBuffer[pointIndex] = 1.0f - airStructuralMaterial.WaterRetention;
    //mMaterialWaterDiffusionSpeedBuffer[pointIndex] = airStructuralMaterial.WaterDiffusionSpeed;
    assert(mWaterBuffer[pointIndex] == 0.0f);
    //mWaterBuffer[pointIndex] = 0.0f;
    assert(!mLeakingCompositeBuffer[pointIndex].IsCumulativelyLeaking);
    //mLeakingCompositeBuffer[pointIndex] = LeakingComposite(false);

    mTemperatureBuffer[pointIndex] = temperature;
    assert(airStructuralMaterial.GetHeatCapacity() > 0.0f);
    mMaterialHeatCapacityReciprocalBuffer[pointIndex] = 1.0f / airStructuralMaterial.GetHeatCapacity();
    mMaterialThermalExpansionCoefficientBuffer[pointIndex] = airStructuralMaterial.ThermalExpansionCoefficient;
    //mMaterialIgnitionTemperatureBuffer[pointIndex] = airStructuralMaterial.IgnitionTemperature;
    //mMaterialCombustionTypeBuffer[pointIndex] = airStructuralMaterial.CombustionType;
    //mCombustionStateBuffer[pointIndex] = CombustionState();

    assert(mLightBuffer[pointIndex] == 0.0f);
    //mLightBuffer[pointIndex] = 0.0f;

    mMaterialWindReceptivityBuffer[pointIndex] = 0.2f; // Smoke cares about wind

    assert(mMaterialRustReceptivityBuffer[pointIndex] == 0.0f);
    //mMaterialRustReceptivityBuffer[pointIndex] = 0.0f;

    mEphemeralParticleAttributes1Buffer[pointIndex].Type = EphemeralType::Smoke;
    mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime = currentSimulationTime;
    mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime = maxSimulationLifetime;
    mEphemeralParticleAttributes2Buffer[pointIndex].State = EphemeralState::SmokeState(
        textureGroup,
        growth,
        GameRandomEngine::GetInstance().GenerateNormalizedUniformReal());

    assert(mConnectedComponentIdBuffer[pointIndex] == NoneConnectedComponentId);
    //mConnectedComponentIdBuffer[pointIndex] = NoneConnectedComponentId;
    mPlaneIdBuffer[pointIndex] = planeId;
    mPlaneIdFloatBuffer[pointIndex] = static_cast<float>(planeId);
    mIsPlaneIdBufferEphemeralDirty = true;

    mColorBuffer[pointIndex] = airStructuralMaterial.RenderColor;
    mIsEphemeralColorBufferDirty = true;
}

void Points::CreateEphemeralParticleSparkle(
    vec2f const & position,
    vec2f const & velocity,
    StructuralMaterial const & structuralMaterial,
    float currentSimulationTime,
    float maxSimulationLifetime,
    PlaneId planeId)
{
    // Get a free slot (or steal one)
    auto pointIndex = FindFreeEphemeralParticle(currentSimulationTime, true);
    assert(NoneElementIndex != pointIndex);

    //
    // Store attributes
    //

    assert(mIsDamagedBuffer[pointIndex] == false); // Ephemeral points are never damaged
    mPositionBuffer[pointIndex] = position;
    mVelocityBuffer[pointIndex] = velocity;
    assert(mSpringForceBuffer[pointIndex] == vec2f::zero()); // Ephemeral points never participate in springs
    mNonSpringForceBuffer[pointIndex] = vec2f::zero();
    mAugmentedMaterialMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mMaterialBuoyancyVolumeFillBuffer[pointIndex] = 0.0f; // No buoyancy
    assert(mDecayBuffer[pointIndex] == 1.0f);
    //mDecayBuffer[pointIndex] = 1.0f;
    mFrozenCoefficientBuffer[pointIndex] = 1.0f;
    mIntegrationFactorTimeCoefficientBuffer[pointIndex] = CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations, 1.0f);
    mBuoyancyCoefficientsBuffer[pointIndex] = BuoyancyCoefficients(0.0f, 0.0f); // No buoyancy
    mMaterialsBuffer[pointIndex] = Materials(&structuralMaterial, nullptr);

    //mMaterialWaterIntakeBuffer[pointIndex] = structuralMaterial.WaterIntake;
    //mMaterialWaterRestitutionBuffer[pointIndex] = 1.0f - structuralMaterial.WaterRetention;
    //mMaterialWaterDiffusionSpeedBuffer[pointIndex] = structuralMaterial.WaterDiffusionSpeed;
    assert(mWaterBuffer[pointIndex] == 0.0f);
    //mWaterBuffer[pointIndex] = 0.0f;
    assert(!mLeakingCompositeBuffer[pointIndex].IsCumulativelyLeaking);
    //mLeakingCompositeBuffer[pointIndex] = LeakingComposite(false);

    mTemperatureBuffer[pointIndex] = GameParameters::Temperature0;
    assert(structuralMaterial.GetHeatCapacity() > 0.0f);
    mMaterialHeatCapacityReciprocalBuffer[pointIndex] = 1.0f / structuralMaterial.GetHeatCapacity();
    //mMaterialThermalExpansionCoefficientBuffer[pointIndex] = structuralMaterial.ThermalExpansionCoefficient;
    //mMaterialIgnitionTemperatureBuffer[pointIndex] = structuralMaterial.IgnitionTemperature;
    //mMaterialCombustionTypeBuffer[pointIndex] = structuralMaterial.CombustionType;
    //mCombustionStateBuffer[pointIndex] = CombustionState();

    assert(mLightBuffer[pointIndex] == 0.0f);
    //mLightBuffer[pointIndex] = 0.0f;

    mMaterialWindReceptivityBuffer[pointIndex] = 20.0f; // Sparkles are susceptible to wind

    assert(mMaterialRustReceptivityBuffer[pointIndex] == 0.0f);
    //mMaterialRustReceptivityBuffer[pointIndex] = 0.0f;

    mEphemeralParticleAttributes1Buffer[pointIndex].Type = EphemeralType::Sparkle;
    mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime = currentSimulationTime;
    mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime = maxSimulationLifetime;
    mEphemeralParticleAttributes2Buffer[pointIndex].State = EphemeralState::SparkleState();

    assert(mConnectedComponentIdBuffer[pointIndex] == NoneConnectedComponentId);
    //mConnectedComponentIdBuffer[pointIndex] = NoneConnectedComponentId;
    mPlaneIdBuffer[pointIndex] = planeId;
    mPlaneIdFloatBuffer[pointIndex] = static_cast<float>(planeId);
    mIsPlaneIdBufferEphemeralDirty = true;
}

void Points::CreateEphemeralParticleWakeBubble(
    vec2f const & position,
    vec2f const & velocity,
    float currentSimulationTime,
    PlaneId planeId,
    GameParameters const & gameParameters)
{
    // Get a free slot (but don't steal one)
    auto pointIndex = FindFreeEphemeralParticle(currentSimulationTime, false);
    if (NoneElementIndex == pointIndex)
        return; // No luck

    //
    // Store attributes
    //

    StructuralMaterial const & waterStructuralMaterial = mMaterialDatabase.GetUniqueStructuralMaterial(StructuralMaterial::MaterialUniqueType::Water);

    assert(mIsDamagedBuffer[pointIndex] == false); // Ephemeral points are never damaged
    mPositionBuffer[pointIndex] = position;
    mVelocityBuffer[pointIndex] = velocity;
    assert(mSpringForceBuffer[pointIndex] == vec2f::zero()); // Ephemeral points never participate in springs
    mNonSpringForceBuffer[pointIndex] = vec2f::zero();
    mAugmentedMaterialMassBuffer[pointIndex] = waterStructuralMaterial.GetMass();
    mMassBuffer[pointIndex] = waterStructuralMaterial.GetMass();
    mMaterialBuoyancyVolumeFillBuffer[pointIndex] = waterStructuralMaterial.BuoyancyVolumeFill;
    assert(mDecayBuffer[pointIndex] == 1.0f);
    //mDecayBuffer[pointIndex] = 1.0f;
    mFrozenCoefficientBuffer[pointIndex] = 1.0f;
    mIntegrationFactorTimeCoefficientBuffer[pointIndex] = CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations, 1.0f);
    mBuoyancyCoefficientsBuffer[pointIndex] = CalculateBuoyancyCoefficients(
        waterStructuralMaterial.BuoyancyVolumeFill,
        waterStructuralMaterial.ThermalExpansionCoefficient);
    mMaterialsBuffer[pointIndex] = Materials(&waterStructuralMaterial, nullptr);

    //mMaterialWaterIntakeBuffer[pointIndex] = waterStructuralMaterial.WaterIntake;
    //mMaterialWaterRestitutionBuffer[pointIndex] = 1.0f - waterStructuralMaterial.WaterRetention;
    //mMaterialWaterDiffusionSpeedBuffer[pointIndex] = waterStructuralMaterial.WaterDiffusionSpeed;
    assert(mWaterBuffer[pointIndex] == 0.0f);
    //mWaterBuffer[pointIndex] = 0.0f;
    assert(!mLeakingCompositeBuffer[pointIndex].IsCumulativelyLeaking);
    //mLeakingCompositeBuffer[pointIndex] = LeakingComposite(false);

    mTemperatureBuffer[pointIndex] = gameParameters.WaterTemperature;
    assert(waterStructuralMaterial.GetHeatCapacity() > 0.0f);
    mMaterialHeatCapacityReciprocalBuffer[pointIndex] = 1.0f / waterStructuralMaterial.GetHeatCapacity();
    mMaterialThermalExpansionCoefficientBuffer[pointIndex] = waterStructuralMaterial.ThermalExpansionCoefficient;
    //mMaterialIgnitionTemperatureBuffer[pointIndex] = waterStructuralMaterial.IgnitionTemperature;
    //mMaterialCombustionTypeBuffer[pointIndex] = waterStructuralMaterial.CombustionType;
    //mCombustionStateBuffer[pointIndex] = CombustionState();

    assert(mLightBuffer[pointIndex] == 0.0f);
    //mLightBuffer[pointIndex] = 0.0f;

    mMaterialWindReceptivityBuffer[pointIndex] = 0.0f; // Wake bubbles (underwater) do not care about wind

    assert(mMaterialRustReceptivityBuffer[pointIndex] == 0.0f);
    //mMaterialRustReceptivityBuffer[pointIndex] = 0.0f;

    mEphemeralParticleAttributes1Buffer[pointIndex].Type = EphemeralType::WakeBubble;
    mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime = currentSimulationTime;
    mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime = 0.4f; // Magic number
    mEphemeralParticleAttributes2Buffer[pointIndex].State = EphemeralState::WakeBubbleState();

    assert(mConnectedComponentIdBuffer[pointIndex] == NoneConnectedComponentId);
    //mConnectedComponentIdBuffer[pointIndex] = NoneConnectedComponentId;
    mPlaneIdBuffer[pointIndex] = planeId;
    mPlaneIdFloatBuffer[pointIndex] = static_cast<float>(planeId);
    mIsPlaneIdBufferEphemeralDirty = true;

    mColorBuffer[pointIndex] = waterStructuralMaterial.RenderColor;
    mIsEphemeralColorBufferDirty = true;
}

void Points::Detach(
    ElementIndex pointElementIndex,
    vec2f const & velocity,
    DetachOptions detachOptions,
    float currentSimulationTime,
    GameParameters const & gameParameters)
{
    // We don't detach ephemeral points
    assert(pointElementIndex < mAlignedShipPointCount);

    // Invoke ship detach handler
    assert(nullptr != mShipPhysicsHandler);
    mShipPhysicsHandler->HandlePointDetach(
        pointElementIndex,
        !!(detachOptions & Points::DetachOptions::GenerateDebris),
        !!(detachOptions & Points::DetachOptions::FireDestroyEvent),
        currentSimulationTime,
        gameParameters);

    // Imprint velocity, unless the point is pinned
    if (!IsPinned(pointElementIndex))
    {
        mVelocityBuffer[pointElementIndex] = velocity;
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

void Points::Restore(ElementIndex pointElementIndex)
{
    assert(IsDamaged(pointElementIndex));

    // Clear the damaged flag
    mIsDamagedBuffer[pointElementIndex] = false;

    // Restore factory-time structural IsLeaking
    mLeakingCompositeBuffer[pointElementIndex].LeakingSources.StructuralLeak =
        mFactoryIsStructurallyLeakingBuffer[pointElementIndex] ? 1.0f : 0.0f;

    // Remove point from set of burning points, in case it was burning
    if (mCombustionStateBuffer[pointElementIndex].State != CombustionState::StateType::NotBurning)
    {
        auto pointIt = std::find(
            mBurningPoints.cbegin(),
            mBurningPoints.cend(),
            pointElementIndex);
        assert(pointIt != mBurningPoints.cend());
        mBurningPoints.erase(pointIt);

        // Restore combustion state
        mCombustionStateBuffer[pointElementIndex].Reset();
    }

    // Invoke ship handler
    assert(nullptr != mShipPhysicsHandler);
    mShipPhysicsHandler->HandlePointRestore(pointElementIndex);
}

void Points::OnOrphaned(ElementIndex pointElementIndex)
{
    //
    // If we're in flames, make the flame tiny
    //

    if (mCombustionStateBuffer[pointElementIndex].State == CombustionState::StateType::Burning)
    {
        // New target: fraction of current size plus something
        mCombustionStateBuffer[pointElementIndex].MaxFlameDevelopment =
            mCombustionStateBuffer[pointElementIndex].FlameDevelopment / 3.0f
            + 0.04f * mRandomNormalizedUniformFloatBuffer[pointElementIndex];

        mCombustionStateBuffer[pointElementIndex].State = CombustionState::StateType::Developing_2;
    }
}

void Points::DestroyEphemeralParticle(
    ElementIndex pointElementIndex)
{
    // Invoke ship handler
    assert(nullptr != mShipPhysicsHandler);
    mShipPhysicsHandler->HandleEphemeralParticleDestroy(pointElementIndex);

    // Fire destroy event
    mGameEventHandler->OnDestroy(
        GetStructuralMaterial(pointElementIndex),
        mParentWorld.IsUnderwater(GetPosition(pointElementIndex)),
        1u);

    // Expire particle
    ExpireEphemeralParticle(pointElementIndex);
}

void Points::UpdateForGameParameters(GameParameters const & gameParameters)
{
    //
    // Check parameter changes
    //

    float const numMechanicalDynamicsIterations = gameParameters.NumMechanicalDynamicsIterations<float>();
    if (numMechanicalDynamicsIterations != mCurrentNumMechanicalDynamicsIterations)
    {
        // Recalc integration factor time coefficients
        for (ElementIndex i : *this)
        {
            mIntegrationFactorTimeCoefficientBuffer[i] = CalculateIntegrationFactorTimeCoefficient(
                numMechanicalDynamicsIterations,
                mFrozenCoefficientBuffer[i]);
        }

        // Remember the new value
        mCurrentNumMechanicalDynamicsIterations = numMechanicalDynamicsIterations;
    }

    float const cumulatedIntakenWaterThresholdForAirBubbles = gameParameters.CumulatedIntakenWaterThresholdForAirBubbles;
    if (cumulatedIntakenWaterThresholdForAirBubbles != mCurrentCumulatedIntakenWaterThresholdForAirBubbles)
    {
        // Randomize cumulated water intaken for each leaking point
        for (ElementIndex i : RawShipPoints())
        {
            if (GetLeakingComposite(i).IsCumulativelyLeaking)
            {
                mCumulatedIntakenWater[i] = RandomizeCumulatedIntakenWater(cumulatedIntakenWaterThresholdForAirBubbles);
            }
        }

        // Remember the new value
        mCurrentCumulatedIntakenWaterThresholdForAirBubbles = cumulatedIntakenWaterThresholdForAirBubbles;
    }
}

void Points::UpdateCombustionLowFrequency(
    ElementIndex pointOffset,
    ElementIndex pointStride,
    float currentSimulationTime,
    float dt,
    Storm::Parameters const & stormParameters,
    GameParameters const & gameParameters)
{
    //
    // Take care of following:
    // - NotBurning->Developing transition (Ignition)
    // - Burning->Decay, Extinguishing transition
    //

    // Prepare candidates for ignition and explosion; we'll pick the top N ones
    // based on the ignition temperature delta
    mCombustionIgnitionCandidates.clear();
    mCombustionExplosionCandidates.clear();

    // Decay rate - the higher this value, the slower fire consumes materials
    float const effectiveCombustionDecayRate = (90.0f / (gameParameters.CombustionSpeedAdjustment * dt));

    // The cdf for rain: we stop burning with a probability equal to this
    float const rainExtinguishCdf = FastPow(stormParameters.RainDensity, 0.5f);

    // No real reason not to do ephemeral points as well, other than they're
    // currently not expected to burn
    for (ElementIndex pointIndex = pointOffset; pointIndex < mRawShipPointCount; pointIndex += pointStride)
    {
        auto const currentState = mCombustionStateBuffer[pointIndex].State;
        if (currentState == CombustionState::StateType::NotBurning)
        {
            //
            // See if this point should start burning
            //

            float const effectiveIgnitionTemperature =
                mMaterialIgnitionTemperatureBuffer[pointIndex] * gameParameters.IgnitionTemperatureAdjustment;

            // Note: we don't check for rain on purpose: we allow flames to develop even if it rains,
            // we'll eventually smother them later
            if (GetTemperature(pointIndex) >= effectiveIgnitionTemperature + GameParameters::IgnitionTemperatureHighWatermark
                && GetWater(pointIndex) < GameParameters::SmotheringWaterLowWatermark
                && GetDecay(pointIndex) > GameParameters::SmotheringDecayHighWatermark)
            {
                auto const combustionType = mMaterialCombustionTypeBuffer[pointIndex];

                if (combustionType == StructuralMaterial::MaterialCombustionType::Combustion
                    && !mParentWorld.IsUnderwater(GetPosition(pointIndex)))
                {
                    // Store point as ignition candidate
                    mCombustionIgnitionCandidates.emplace_back(
                        pointIndex,
                        (GetTemperature(pointIndex) - effectiveIgnitionTemperature) / effectiveIgnitionTemperature);
                }
                else if (combustionType == StructuralMaterial::MaterialCombustionType::Explosion)
                {
                    // Store point as explosion candidate
                    mCombustionExplosionCandidates.emplace_back(
                        pointIndex,
                        (GetTemperature(pointIndex) - effectiveIgnitionTemperature) / effectiveIgnitionTemperature);
                }
            }
        }
        else if (currentState == CombustionState::StateType::Burning)
        {
            //
            // See if this point should start extinguishing...
            //

            // ...for water or sea: we do this check at high frequency

            // ...for temperature or decay or rain: we check it here

            float const effectiveIgnitionTemperature =
                mMaterialIgnitionTemperatureBuffer[pointIndex] * gameParameters.IgnitionTemperatureAdjustment;

            if (GetTemperature(pointIndex) <= (effectiveIgnitionTemperature + GameParameters::IgnitionTemperatureLowWatermark)
                || GetDecay(pointIndex) < GameParameters::SmotheringDecayLowWatermark)
            {
                //
                // Transition to Extinguishing - by consumption
                //

                mCombustionStateBuffer[pointIndex].State = CombustionState::StateType::Extinguishing_Consumed;

                // Notify combustion end
                mGameEventHandler->OnPointCombustionEnd();
            }
            else if (GameRandomEngine::GetInstance().GenerateUniformBoolean(rainExtinguishCdf))
            {
                //
                // Transition to Extinguishing - by smothering for rain
                //

                SmotherCombustion(pointIndex, false);
            }
            else
            {
                // Apply effects of burning

                //
                // 1. Decay - proportionally to mass
                //
                // Our goal:
                // - An iron hull mass (750Kg) decays completely (goes to 0.01)
                //   in 30 (simulated) seconds
                // - A smaller (larger) mass decays in shorter (longer) time,
                //   but a very small mass shouldn't burn in too short of a time
                //

                float const massMultiplier = pow(
                    mMaterialsBuffer[pointIndex].Structural->GetMass() / 750.0f,
                    0.15f); // Magic number: one tenth of the mass is 0.70 times the number of steps

                float const totalDecaySteps =
                    effectiveCombustionDecayRate
                    * massMultiplier;

                // decay(@ step i) = alpha^i
                // decay(@ step T) = min_decay => alpha^T = min_decay => alpha = min_decay^(1/T)
                float const decayAlpha = pow(0.01f, 1.0f / totalDecaySteps);

                // Decay point
                mDecayBuffer[pointIndex] *= decayAlpha;


                //
                // 2. Decay neighbors
                //

                for (auto const s : GetConnectedSprings(pointIndex).ConnectedSprings)
                {
                    mDecayBuffer[s.OtherEndpointIndex] *= decayAlpha;
                }
            }
        }
    }


    //
    // Pick candidates for ignition
    //

    if (!mCombustionIgnitionCandidates.empty())
    {
        // Randomly choose the max number of points we want to ignite now,
        // honoring MaxBurningParticles at the same time
        size_t const maxIgnitionPoints = std::min(
            std::min(
                size_t(4) + GameRandomEngine::GetInstance().Choose(size_t(6)), // 4->9
                mBurningPoints.size() < gameParameters.MaxBurningParticles
                ? static_cast<size_t>(gameParameters.MaxBurningParticles) - mBurningPoints.size()
                : size_t(0)),
            mCombustionIgnitionCandidates.size());

        // Sort top N candidates by ignition temperature delta
        std::nth_element(
            mCombustionIgnitionCandidates.data(),
            mCombustionIgnitionCandidates.data() + maxIgnitionPoints,
            mCombustionIgnitionCandidates.data() + mCombustionIgnitionCandidates.size(),
            [](auto const & t1, auto const & t2)
            {
                return std::get<1>(t1) > std::get<1>(t2);
            });

        // Ignite these points
        for (size_t i = 0; i < maxIgnitionPoints; ++i)
        {
            assert(i < mCombustionIgnitionCandidates.size());

            auto const pointIndex = std::get<0>(mCombustionIgnitionCandidates[i]);

            //
            // Ignite!
            //

            mCombustionStateBuffer[pointIndex].State = CombustionState::StateType::Developing_1;

            // Initial development depends on how deep this particle is in its burning zone
            mCombustionStateBuffer[pointIndex].FlameDevelopment =
                0.1f + 0.5f * SmoothStep(0.0f, 2.0f, std::get<1>(mCombustionIgnitionCandidates[i]));

            // Max development: random and depending on number of springs connected to this point
            // (so chains have smaller flames)
            float const deltaSizeDueToConnectedSprings =
                static_cast<float>(mConnectedSpringsBuffer[pointIndex].ConnectedSprings.size())
                * 0.0625f; // 0.0625 -> 0.50 (@8)
            mCombustionStateBuffer[pointIndex].MaxFlameDevelopment = std::max(
                0.25f + deltaSizeDueToConnectedSprings + 0.5f * mRandomNormalizedUniformFloatBuffer[pointIndex], // 0.25 + dsdtcs -> 0.75 + dsdtcs
                mCombustionStateBuffer[pointIndex].FlameDevelopment);

            // Reset flame vector
            mCombustionStateBuffer[pointIndex].FlameVector = CalculateIdealFlameVector(
                GetVelocity(pointIndex),
                200.0f); // For an initial flame, we want the particle's current velocity to have a smaller impact on the flame vector

            // Add point to vector of burning points, sorted by plane ID
            assert(mBurningPoints.cend() == std::find(mBurningPoints.cbegin(), mBurningPoints.cend(), pointIndex));
            mBurningPoints.insert(
                std::lower_bound( // Earlier than others at same plane ID, so it's drawn behind them
                    mBurningPoints.cbegin(),
                    mBurningPoints.cend(),
                    pointIndex,
                    [this](auto p1, auto p2)
                    {
                        return this->mPlaneIdBuffer[p1] < mPlaneIdBuffer[p2];
                    }),
                pointIndex);

            // Notify
            mGameEventHandler->OnPointCombustionBegin();
        }
    }


    //
    // Pick candidates for explosion
    //

    if (!mCombustionExplosionCandidates.empty())
    {
        size_t const maxExplosionPoints = std::min(
            size_t(10), // Magic number
            mCombustionExplosionCandidates.size());

        // Sort top N candidates by ignition temperature delta
        std::nth_element(
            mCombustionExplosionCandidates.data(),
            mCombustionExplosionCandidates.data() + maxExplosionPoints,
            mCombustionExplosionCandidates.data() + mCombustionExplosionCandidates.size(),
            [](auto const & t1, auto const & t2)
            {
                return std::get<1>(t1) > std::get<1>(t2);
            });

        // Calculate blast heat
        float const blastHeat =
            GameParameters::CombustionHeat
            * 1.5f // Arbitrary multiplier
            * dt
            * gameParameters.CombustionHeatAdjustment
            * (gameParameters.IsUltraViolentMode ? 10.0f : 1.0f);

        // Explode these points
        for (size_t i = 0; i < maxExplosionPoints; ++i)
        {
            assert(i < mCombustionExplosionCandidates.size());

            auto const pointIndex = std::get<0>(mCombustionExplosionCandidates[i]);
            auto const pointPosition = GetPosition(pointIndex);

            //
            // Explode!
            //

            float const blastRadius =
                mMaterialsBuffer[pointIndex].Structural->ExplosiveCombustionRadius
                * (gameParameters.IsUltraViolentMode ? 4.0f : 1.0f);

            float const blastStrength = mMaterialsBuffer[pointIndex].Structural->ExplosiveCombustionStrength;

            // Start explosion
            mShipPhysicsHandler->StartExplosion(
                currentSimulationTime,
                GetPlaneId(pointIndex),
                pointPosition,
                blastRadius,
                blastStrength,
                blastHeat,
                ExplosionType::Combustion,
                gameParameters);

            // Notify explosion
            mGameEventHandler->OnCombustionExplosion(
                mParentWorld.IsUnderwater(pointPosition),
                1);

            // Transition state
            mCombustionStateBuffer[pointIndex].State = CombustionState::StateType::Exploded;
        }
    }
}

void Points::UpdateCombustionHighFrequency(
    float /*currentSimulationTime*/,
    float dt,
    GameParameters const & gameParameters)
{
    //
    // For all burning points, take care of following:
    // - Developing points: development up
    // - Burning points: heat generation
    // - Extinguishing points: development down
    //

    // Heat generated by combustion in this step
    float const effectiveCombustionHeat =
        GameParameters::CombustionHeat
        * dt
        * gameParameters.CombustionHeatAdjustment;

    // Points that are not burning anymore after this step
    assert(mStoppedBurningPoints.empty());

    for (auto const pointIndex : mBurningPoints)
    {
        CombustionState & pointCombustionState = mCombustionStateBuffer[pointIndex];

        assert(pointCombustionState.State != CombustionState::StateType::NotBurning); // Otherwise it wouldn't be in this set

        //
        // Check if this point should stop developing/burning or start extinguishing faster
        //

        auto const currentState = pointCombustionState.State;

        if ((currentState == CombustionState::StateType::Developing_1
            || currentState == CombustionState::StateType::Developing_2
            || currentState == CombustionState::StateType::Burning
            || currentState == CombustionState::StateType::Extinguishing_Consumed)
            && (mParentWorld.IsUnderwater(GetPosition(pointIndex))
                || GetWater(pointIndex) > GameParameters::SmotheringWaterHighWatermark))
        {
            //
            // Transition to Extinguishing - by smothering for water
            //

            SmotherCombustion(pointIndex, true);
        }
        else if (currentState == CombustionState::StateType::Burning)
        {
            //
            // Generate heat at:
            // - point itself: fix to constant temperature = ignition temperature + 10%
            // - neighbors: 100Kw * C, scaled by directional alpha
            //

            mTemperatureBuffer[pointIndex] =
                mMaterialIgnitionTemperatureBuffer[pointIndex]
                * gameParameters.IgnitionTemperatureAdjustment
                * 1.1f;

            for (auto const s : GetConnectedSprings(pointIndex).ConnectedSprings)
            {
                auto const otherEndpointIndex = s.OtherEndpointIndex;

                // Calculate direction coefficient so to prefer upwards direction:
                // 0.9 + 1.0*(1 - cos(theta)): 2.9 N, 0.9 S, 1.9 W and E
                vec2f const springDir = (GetPosition(otherEndpointIndex) - GetPosition(pointIndex)).normalise();
                float const dirAlpha =
                    (0.9f + 1.0f * (1.0f - springDir.dot(GameParameters::GravityNormalized)));
                // No normalization: when using normalization flame does not propagate along rope

                // Add heat to point
                mTemperatureBuffer[otherEndpointIndex] +=
                    effectiveCombustionHeat
                    * dirAlpha
                    * mMaterialHeatCapacityReciprocalBuffer[otherEndpointIndex];
            }
        }

        /* FUTUREWORK

        The following code emits smoke for burning particles, but there are rendering problems:
        Smoke should be drawn behind flames, hence GenericTexture's should be drawn in a layer that is earlier than flames.
        However, generic textures (smoke) have internal transparency, while flames have none; the Z test makes it so then
        that smoke at plane ID P shows the ship behind it, even though there are flames at plane IDs < P.
        The only way out that I may think of, at this moment, is to draw flames and generic textures alternatively, for each
        plane ID (!), or to make smoke fully opaque.

        //
        // Check if this point should emit smoke
        //

        if (pointCombustionState.State != CombustionState::StateType::NotBurning)
        {
            // See if we need to calculate the next emission timestamp
            if (pointCombustionState.NextSmokeEmissionSimulationTimestamp == 0.0f)
            {
                pointCombustionState.NextSmokeEmissionSimulationTimestamp =
                    currentSimulationTime
                    + GameRandomEngine::GetInstance().GenerateExponentialReal(
                        gameParameters.SmokeEmissionDensityAdjustment
                        * pointCombustionState.FlameDevelopment
                        / 1.5f); // TODOHERE: replace with Material's properties, precalc'd with gameParameters.SmokeEmissionDensityAdjustment
            }

            // See if it's time to emit smoke
            if (currentSimulationTime >= pointCombustionState.NextSmokeEmissionSimulationTimestamp)
            {
                //
                // Emit smoke
                //

                // Generate particle
                CreateEphemeralParticleHeavySmoke(
                    GetPosition(pointIndex),
                    GetTemperature(pointIndex),
                    currentSimulationTime,
                    GetPlaneId(pointIndex),
                    gameParameters);

                // Make sure we re-calculate the next emission timestamp
                pointCombustionState.NextSmokeEmissionSimulationTimestamp = 0.0f;
            }
        }
        */

        //
        // Run development/extinguishing state machine now
        //

        switch (pointCombustionState.State)
        {
            case CombustionState::StateType::Developing_1:
            {
                //
                // Develop
                //
                // f(n-1) + 0.105*f(n-1): when starting from 0.1, after 25 steps (0.5s) it's 1.21
                // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:0.1,%22valid%22:true}]&expression=f(n-1)%20+%200.105*f(n-1)&target=0&endTarget=25&range=true
                //

                pointCombustionState.FlameDevelopment +=
                    0.105f * pointCombustionState.FlameDevelopment;

                // Check whether it's time to transition to the next development phase
                if (pointCombustionState.FlameDevelopment > pointCombustionState.MaxFlameDevelopment + 0.2f)
                {
                    pointCombustionState.State = CombustionState::StateType::Developing_2;
                }

                break;
            }

            case CombustionState::StateType::Developing_2:
            {
                //
                // Develop
                //
                // f(n-1) - 0.2*f(n-1): when starting from 0.2, after 10 steps (0.2s) it's below 0.02
                // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:0.2,%22valid%22:true}]&expression=f(n-1)%20-%200.2*f(n-1)&target=0&endTarget=25&range=true
                //

                // FlameDevelopment is now in the (MFD + 0.2, MFD) range
                auto extraFlameDevelopment = pointCombustionState.FlameDevelopment - pointCombustionState.MaxFlameDevelopment;
                extraFlameDevelopment =
                    extraFlameDevelopment
                    - 0.2f * extraFlameDevelopment;

                pointCombustionState.FlameDevelopment =
                    pointCombustionState.MaxFlameDevelopment + extraFlameDevelopment;

                // Check whether it's time to transition to burning
                if (extraFlameDevelopment < 0.02f)
                {
                    pointCombustionState.State = CombustionState::StateType::Burning;
                    pointCombustionState.FlameDevelopment = pointCombustionState.MaxFlameDevelopment;
                }

                break;
            }

            case CombustionState::StateType::Extinguishing_Consumed:
            case CombustionState::StateType::Extinguishing_SmotheredRain:
            case CombustionState::StateType::Extinguishing_SmotheredWater:
            {
                //
                // Un-develop
                //

                if (pointCombustionState.State == CombustionState::StateType::Extinguishing_Consumed)
                {
                    //
                    // f(n-1) - 0.0625*(1.01 - f(n-1)): when starting from 1, after 75 steps (1.5s) it's under 0.02
                    // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:1,%22valid%22:true}]&expression=f(n-1)%20-%200.0625*(1.01%20-%20f(n-1))&target=0&endTarget=80&range=true
                    //

                    pointCombustionState.FlameDevelopment -=
                        0.0625f
                        * (pointCombustionState.MaxFlameDevelopment - pointCombustionState.FlameDevelopment + 0.01f);
                }
                else if (pointCombustionState.State == CombustionState::StateType::Extinguishing_SmotheredRain)
                {
                    //
                    // f(n-1) - 0.075*f(n-1): when starting from 1, after 50 steps (1.0s) it's under 0.02
                    // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:1,%22valid%22:true}]&expression=f(n-1)%20-%200.075*f(n-1)&target=0&endTarget=75&range=true
                    //

                    pointCombustionState.FlameDevelopment -=
                        0.075f * pointCombustionState.FlameDevelopment;
                }
                else
                {
                    assert(pointCombustionState.State == CombustionState::StateType::Extinguishing_SmotheredWater);

                    //
                    // f(n-1) - 0.3*f(n-1): when starting from 1, after 10 steps (0.2s) it's under 0.02
                    // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:1,%22valid%22:true}]&expression=f(n-1)%20-%200.3*f(n-1)&target=0&endTarget=25&range=true
                    //

                    pointCombustionState.FlameDevelopment -=
                        0.3f * pointCombustionState.FlameDevelopment;
                }

                // Check whether we are done now
                if (pointCombustionState.FlameDevelopment <= 0.02f)
                {
                    //
                    // Stop burning
                    //

                    pointCombustionState.State = CombustionState::StateType::NotBurning;

                    // Remove point from set of burning points
                    mStoppedBurningPoints.emplace_back(pointIndex);
                }

                break;
            }

            case CombustionState::StateType::Burning:
            case CombustionState::StateType::Exploded:
            case CombustionState::StateType::NotBurning:
            {
                // Nothing to do here
                break;
            }
        }


        //
        // Calculate flame vector
        //
        // Note: the point might not be burning anymore, in case we've just extinguished it
        //

        // Vector Q is the vector describing the ideal, final flame's
        // direction and length
        vec2f const Q = CalculateIdealFlameVector(
            GetVelocity(pointIndex),
            100.0f); // Particle's velocity has a larger impact on the final vector

        //
        // Converge current flame vector towards target vector Q
        //
        //  fv(n) = rate * Q + (1 - rate) * fv(n-1)
        // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:1,%22valid%22:true}]&expression=0.2%20*%205%20+%20(1%20-%200.2)*f(n-1)&target=0&endTarget=80&range=true
        //

        // Rate inversely depends on the magnitude of change:
        // - A big change: little rate (lots of inertia)
        // - A small change: big rate (immediately responsive)
        float constexpr MinConvergenceRate = 0.02f;
        float constexpr MaxConvergenceRate = 0.05f;
        float const changeMagnitude = std::abs(Q.angleCw(pointCombustionState.FlameVector));
        float const convergenceRate =
            MinConvergenceRate
            + (MaxConvergenceRate - MinConvergenceRate) * (1.0f - LinearStep(0.0f, Pi<float>, changeMagnitude));

        pointCombustionState.FlameVector += (Q - pointCombustionState.FlameVector) * convergenceRate;
    }

    //
    // Remove points that have stopped burning
    //

    if (!mStoppedBurningPoints.empty())
    {
        for (auto stoppedBurningPoint : mStoppedBurningPoints)
        {
            auto burningPointIt = std::find(
                mBurningPoints.cbegin(),
                mBurningPoints.cend(),
                stoppedBurningPoint);
            assert(burningPointIt != mBurningPoints.cend());
            mBurningPoints.erase(burningPointIt);
        }

        mStoppedBurningPoints.clear();
    }
}

void Points::ReorderBurningPointsForDepth()
{
    std::sort(
        mBurningPoints.begin(),
        mBurningPoints.end(),
        [this](auto p1, auto p2)
        {
            return this->mPlaneIdBuffer[p1] < this->mPlaneIdBuffer[p2];
        });
}

void Points::UpdateEphemeralParticles(
    float currentSimulationTime,
    GameParameters const & gameParameters)
{
    // Transformation from desired velocity impulse to force
    float const randomWalkVelocityImpulseToForceCoefficient =
        GameParameters::AirMass
        / gameParameters.SimulationStepTimeDuration<float>;

    // Ocean surface displacement at bubbles surfacing
    float const oceanFloorDisplacementAtAirBubbleSurfacingSurfaceOffset =
        (gameParameters.DoDisplaceOceanSurfaceAtAirBubblesSurfacing ? 1.0f : 0.0f)
        * 1.0f;

    for (ElementIndex pointIndex : this->EphemeralPoints())
    {
        auto const ephemeralType = GetEphemeralType(pointIndex);
        if (EphemeralType::None != ephemeralType)
        {
            //
            // Run this particle's state machine
            //

            switch (ephemeralType)
            {
                case EphemeralType::AirBubble:
                {
                    // Do not advance air bubble if it's pinned
                    if (!IsPinned(pointIndex))
                    {
                        auto const & position = GetPosition(pointIndex);
                        float const waterHeight = mParentWorld.GetOceanSurfaceHeightAt(position.x);
                        float const deltaY = waterHeight - position.y; // Positive when point _below_ surface
                        if (deltaY <= 0.0f)
                        {
                            // Got to the surface, expire
                            ExpireEphemeralParticle(pointIndex);
                        }
                        else
                        {
                            //
                            // Update progress based off y
                            //

                            auto & state = mEphemeralParticleAttributes2Buffer[pointIndex].State.AirBubble;

                            state.CurrentDeltaY = deltaY;
                            state.Progress = // 0.00..001 (@ way below surface) -> 1.0 (@ surface)
                                -1.0f
                                / (-1.0f + std::min(GetPosition(pointIndex).y, 0.0f));

                            //
                            // Update vortex
                            //

                            float const simulationLifetime =
                                currentSimulationTime
                                - mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime;

                            float const vortexAmplitude = 
                                state.VortexAmplitude 
                                * std::min(1.0f, simulationLifetime / 5.0f);

                            float const vortexValue =
                                vortexAmplitude
                                * PrecalcLoFreqSin.GetNearestPeriodic(
                                    state.NormalizedVortexAngularVelocity * simulationLifetime);

                            // Update position with delta
                            mPositionBuffer[pointIndex].x += vortexValue - state.LastVortexValue;

                            state.LastVortexValue = vortexValue;

                            //
                            // Displace ocean surface, if surfacing
                            //

                            if (deltaY < oceanFloorDisplacementAtAirBubbleSurfacingSurfaceOffset)
                            {
                                mParentWorld.DisplaceOceanSurfaceAt(
                                    mPositionBuffer[pointIndex].x,
                                    (oceanFloorDisplacementAtAirBubbleSurfacingSurfaceOffset - deltaY) / 8.0f); // Magic number

                                mGameEventHandler->OnAirBubbleSurfaced(1);
                            }
                        }
                    }

                    break;
                }

                case EphemeralType::Debris:
                {
                    // Check if expired
                    auto const elapsedSimulationLifetime = currentSimulationTime - mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime;
                    auto const maxSimulationLifetime = mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime;
                    if (elapsedSimulationLifetime >= maxSimulationLifetime)
                    {
                        ExpireEphemeralParticle(pointIndex);

                        // Remember that ephemeral points are now dirty
                        mAreEphemeralPointsDirtyForRendering = true;
                    }
                    else
                    {
                        // Update alpha based off remaining time

                        float alpha = std::max(
                            1.0f - elapsedSimulationLifetime / maxSimulationLifetime,
                            0.0f);

                        mColorBuffer[pointIndex].w = alpha;
                        mIsEphemeralColorBufferDirty = true;
                    }

                    break;
                }

                case EphemeralType::Smoke:
                {
                    // Calculate progress
                    auto const elapsedSimulationLifetime = currentSimulationTime - mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime;
                    assert(mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime > 0.0f);
                    float const lifetimeProgress =
                        elapsedSimulationLifetime
                        / mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime;

                    // Check if expired
                    auto const & position = GetPosition(pointIndex);
                    if (lifetimeProgress >= 1.0f
                        || mParentWorld.IsUnderwater(position))
                    {
                        //
                        /// Expired
                        //

                        ExpireEphemeralParticle(pointIndex);
                    }
                    else
                    {
                        //
                        // Still alive
                        //

                        // Update progress
                        mEphemeralParticleAttributes2Buffer[pointIndex].State.Smoke.LifetimeProgress = lifetimeProgress;
                        if (EphemeralState::SmokeState::GrowthType::Slow == mEphemeralParticleAttributes2Buffer[pointIndex].State.Smoke.Growth)
                        {
                            mEphemeralParticleAttributes2Buffer[pointIndex].State.Smoke.ScaleProgress =
                                std::min(1.0f, elapsedSimulationLifetime / 5.0f);
                        }
                        else
                        {
                            assert(EphemeralState::SmokeState::GrowthType::Fast == mEphemeralParticleAttributes2Buffer[pointIndex].State.Smoke.Growth);
                            mEphemeralParticleAttributes2Buffer[pointIndex].State.Smoke.ScaleProgress =
                                1.07f * (1.0f - exp(-3.0f * lifetimeProgress));
                        }

                        // Inject random walk in direction orthogonal to current velocity
                        float const randomWalkMagnitude =
                            0.3f * (static_cast<float>(GameRandomEngine::GetInstance().Choose<int>(2)) - 0.5f);
                        vec2f const deviationDirection =
                            GetVelocity(pointIndex).normalise().to_perpendicular();
                        mNonSpringForceBuffer[pointIndex] +=
                            deviationDirection * randomWalkMagnitude
                            * randomWalkVelocityImpulseToForceCoefficient;
                    }

                    break;
                }

                case EphemeralType::Sparkle:
                {
                    // Check if expired
                    auto const elapsedSimulationLifetime = currentSimulationTime - mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime;
                    auto const maxSimulationLifetime = mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime;
                    if (elapsedSimulationLifetime >= maxSimulationLifetime
                        || mParentWorld.IsUnderwater(GetPosition(pointIndex)))
                    {
                        ExpireEphemeralParticle(pointIndex);
                    }
                    else
                    {
                        // Update progress based off remaining time
                        assert(maxSimulationLifetime > 0.0f);
                        mEphemeralParticleAttributes2Buffer[pointIndex].State.Sparkle.Progress =
                            elapsedSimulationLifetime / maxSimulationLifetime;
                    }

                    break;
                }

                case EphemeralType::WakeBubble:
                {
                    // Check if expired
                    auto const elapsedSimulationLifetime = currentSimulationTime - mEphemeralParticleAttributes1Buffer[pointIndex].StartSimulationTime;
                    auto const maxSimulationLifetime = mEphemeralParticleAttributes2Buffer[pointIndex].MaxSimulationLifetime;
                    if (elapsedSimulationLifetime >= maxSimulationLifetime
                        || !mParentWorld.IsUnderwater(GetPosition(pointIndex)))
                    {
                        ExpireEphemeralParticle(pointIndex);
                    }
                    else
                    {
                        // Update progress based off remaining time
                        assert(maxSimulationLifetime > 0.0f);
                        mEphemeralParticleAttributes2Buffer[pointIndex].State.WakeBubble.Progress =
                            elapsedSimulationLifetime / maxSimulationLifetime;
                    }

                    break;
                }

                default:
                {
                    // Do nothing
                }
            }
        }
    }
}

void Points::UpdateHighlights(GameWallClock::float_time currentWallClockTime)
{
    //
    // ElectricalElement highlights
    //

    auto constexpr ElectricalElementHighlightLifetime = 1s;

    for (auto it = mElectricalElementHighlightedPoints.begin(); it != mElectricalElementHighlightedPoints.end(); /* incremented in loop */)
    {
        // Calculate progress
        float const progress = GameWallClock::Progress(
            currentWallClockTime,
            it->StartTime,
            ElectricalElementHighlightLifetime);

        if (progress > 1.0f)
        {
            // Expire
            it = mElectricalElementHighlightedPoints.erase(it);
        }
        else
        {
            // Update
            it->Progress = progress;
            ++it;
        }
    }

    //
    // Circle
    //

    for (auto it = mCircleHighlightedPoints.begin(); it != mCircleHighlightedPoints.end(); /* incremented in loop */)
    {
        // Expected sequence when not renewed:
        // - Highlight created: SimulationStepsExperienced = 0
        // - Points::Update: SimulationStepsExperienced = 1
        // - Render
        // - Points::Update: SimulationStepsExperienced = 2 => removed
        // - Render (none)
        ++(it->SimulationStepsExperienced);
        if (it->SimulationStepsExperienced > 1)
        {
            // Expire
            it = mCircleHighlightedPoints.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Points::Query(ElementIndex pointElementIndex) const
{
    LogMessage("PointIndex: ", pointElementIndex, (nullptr != mMaterialsBuffer[pointElementIndex].Structural) ? (" (" + mMaterialsBuffer[pointElementIndex].Structural->Name) + ")" : "");
    LogMessage("P=", mPositionBuffer[pointElementIndex].toString(), " V=", mVelocityBuffer[pointElementIndex].toString());
    LogMessage("W=", mWaterBuffer[pointElementIndex], " L=", mLightBuffer[pointElementIndex], " T=", mTemperatureBuffer[pointElementIndex], " Decay=", mDecayBuffer[pointElementIndex]);
    //LogMessage("Springs: ", mConnectedSpringsBuffer[pointElementIndex].ConnectedSprings.size(), " (factory: ", mFactoryConnectedSpringsBuffer[pointElementIndex].ConnectedSprings.size(), ")");
    LogMessage("PlaneID: ", mPlaneIdBuffer[pointElementIndex], " ConnectedComponentID: ", mConnectedComponentIdBuffer[pointElementIndex]);
}

void Points::UploadAttributes(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    // Upload immutable attributes, if we haven't uploaded them yet
    if (mIsTextureCoordinatesBufferDirty)
    {
        renderContext.UploadShipPointImmutableAttributes(
            shipId,
            mTextureCoordinatesBuffer.data());

        mIsTextureCoordinatesBufferDirty = false;
    }

    // Upload colors, if dirty
    if (mIsWholeColorBufferDirty)
    {
        renderContext.UploadShipPointColors(
            shipId,
            mColorBuffer.data(),
            0,
            mAllPointCount);

        mIsWholeColorBufferDirty = false;
        mIsEphemeralColorBufferDirty = false;
    }
    else if (mIsEphemeralColorBufferDirty)
    {
        // Only upload ephemeral particle portion
        renderContext.UploadShipPointColors(
            shipId,
            &(mColorBuffer.data()[mAlignedShipPointCount]),
            mAlignedShipPointCount,
            mEphemeralPointCount);

        mIsEphemeralColorBufferDirty = false;
    }

    //
    // Upload mutable attributes
    //

    // We only upload all points for the first upload, so that also ephemeral
    // points have reasonable (default) values; for subsequent uploads,
    // for some buffers we only need to upload non-ephemeral points
    size_t const partialPointCount = mHaveWholeBuffersBeenUploadedOnce ? mRawShipPointCount : mAllPointCount;

    renderContext.UploadShipPointMutableAttributesStart(shipId);

    renderContext.UploadShipPointMutableAttributes(
        shipId,
        mPositionBuffer.data(),
        mLightBuffer.data(),
        mWaterBuffer.data(),
        partialPointCount);

    if (mIsPlaneIdBufferNonEphemeralDirty)
    {
        if (mIsPlaneIdBufferEphemeralDirty)
        {
            // Whole

            renderContext.UploadShipPointMutableAttributesPlaneId(
                shipId,
                mPlaneIdFloatBuffer.data(),
                0,
                mAllPointCount);

            mIsPlaneIdBufferEphemeralDirty = false;
        }
        else
        {
            // Just non-ephemeral portion

            renderContext.UploadShipPointMutableAttributesPlaneId(
                shipId,
                mPlaneIdFloatBuffer.data(),
                0,
                mRawShipPointCount);
        }

        mIsPlaneIdBufferNonEphemeralDirty = false;
    }
    else if (mIsPlaneIdBufferEphemeralDirty)
    {
        // Just ephemeral portion

        renderContext.UploadShipPointMutableAttributesPlaneId(
            shipId,
            &(mPlaneIdFloatBuffer.data()[mAlignedShipPointCount]),
            mAlignedShipPointCount,
            mEphemeralPointCount);

        mIsPlaneIdBufferEphemeralDirty = false;
    }

    if (mIsDecayBufferDirty)
    {
        renderContext.UploadShipPointMutableAttributesDecay(
            shipId,
            mDecayBuffer.data(),
            0,
            partialPointCount);

        mIsDecayBufferDirty = false;
    }

    if (renderContext.GetDrawHeatOverlay())
    {
        renderContext.UploadShipPointTemperature(
            shipId,
            mTemperatureBuffer.data(),
            0,
            partialPointCount);
    }

    renderContext.UploadShipPointMutableAttributesEnd(shipId);

    mHaveWholeBuffersBeenUploadedOnce = true;
}

void Points::UploadNonEphemeralPointElements(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    bool const doUploadAllPoints = (DebugShipRenderModeType::Points == renderContext.GetDebugShipRenderMode());

    for (ElementIndex pointIndex : RawShipPoints())
    {
        if (doUploadAllPoints
            || mConnectedSpringsBuffer[pointIndex].ConnectedSprings.empty()) // orphaned
        {
            renderContext.UploadShipElementPoint(
                shipId,
                pointIndex);
        }
    }
}

void Points::UploadFlames(
    ShipId shipId,
    float windSpeedMagnitude,
    Render::RenderContext & renderContext) const
{
    renderContext.UploadShipFlamesStart(shipId, mBurningPoints.size(), windSpeedMagnitude);

    // Upload flames, in order of plane ID
    for (auto const pointIndex : mBurningPoints)
    {
        renderContext.UploadShipFlame(
            shipId,
            GetPlaneId(pointIndex),
            GetPosition(pointIndex),
            mCombustionStateBuffer[pointIndex].FlameVector,
            mCombustionStateBuffer[pointIndex].FlameDevelopment, // scale
            mRandomNormalizedUniformFloatBuffer[pointIndex],
            // IsOnChain: we use # of triangles as a heuristic for the point being on a chain,
            // and we use the *factory* ones to avoid sudden depth jumps when triangles are destroyed by fire
            mFactoryConnectedTrianglesBuffer[pointIndex].ConnectedTriangles.empty());
    }

    renderContext.UploadShipFlamesEnd(shipId);
}

void Points::UploadVectors(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    if (renderContext.GetVectorFieldRenderMode() == VectorFieldRenderModeType::PointVelocity)
    {
        static vec4f constexpr VectorColor(0.203f, 0.552f, 0.219f, 1.0f);

        renderContext.UploadShipVectors(
            shipId,
            mElementCount,
            mPositionBuffer.data(),
            mPlaneIdFloatBuffer.data(),
            mVelocityBuffer.data(),
            0.25f,
            VectorColor);
    }
    else if (renderContext.GetVectorFieldRenderMode() == VectorFieldRenderModeType::PointForce)
    {
        static vec4f constexpr VectorColor(0.5f, 0.1f, 0.f, 1.0f);

        renderContext.UploadShipVectors(
            shipId,
            mElementCount,
            mPositionBuffer.data(),
            mPlaneIdFloatBuffer.data(),
            mForceRenderBuffer.data(),
            0.0005f,
            VectorColor);
    }
    else if (renderContext.GetVectorFieldRenderMode() == VectorFieldRenderModeType::PointWaterVelocity)
    {
        static vec4f constexpr VectorColor(0.094f, 0.509f, 0.925f, 1.0f);

        renderContext.UploadShipVectors(
            shipId,
            mElementCount,
            mPositionBuffer.data(),
            mPlaneIdFloatBuffer.data(),
            mWaterVelocityBuffer.data(),
            1.0f,
            VectorColor);
    }
    else if (renderContext.GetVectorFieldRenderMode() == VectorFieldRenderModeType::PointWaterMomentum)
    {
        static vec4f constexpr VectorColor(0.054f, 0.066f, 0.443f, 1.0f);

        renderContext.UploadShipVectors(
            shipId,
            mElementCount,
            mPositionBuffer.data(),
            mPlaneIdFloatBuffer.data(),
            mWaterMomentumBuffer.data(),
            0.4f,
            VectorColor);
    }
}

void Points::UploadEphemeralParticles(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    //
    // Upload points and/or textures
    //

    if (mAreEphemeralPointsDirtyForRendering)
    {
        renderContext.UploadShipElementEphemeralPointsStart(shipId);
    }

    for (ElementIndex pointIndex : this->EphemeralPoints())
    {
        switch (GetEphemeralType(pointIndex))
        {
            case EphemeralType::AirBubble:
            {
                auto const & state = mEphemeralParticleAttributes2Buffer[pointIndex].State.AirBubble;

                float constexpr ScaleMax = 0.3f;
                float constexpr ScaleMin = 0.1f;
                float const scale =
                    ScaleMin + (ScaleMax - ScaleMin) * (1.0f - LinearStep(80.0f, 400.0f, state.CurrentDeltaY));

                renderContext.UploadShipAirBubble(
                    shipId,
                    GetPlaneId(pointIndex),
                    GetPosition(pointIndex),
                    scale,
                    std::min(1.0f, state.CurrentDeltaY)); // Alpha

                break;
            }

            case EphemeralType::Debris:
            {
                // Don't upload point unless there's been a change
                if (mAreEphemeralPointsDirtyForRendering)
                {
                    renderContext.UploadShipElementEphemeralPoint(
                        shipId,
                        pointIndex);
                }

                break;
            }

            case EphemeralType::Smoke:
            {
                auto const & state = mEphemeralParticleAttributes2Buffer[pointIndex].State.Smoke;

                // Calculate scale
                float const scale = state.ScaleProgress;

                // Calculate alpha
                float const lifetimeProgress = state.LifetimeProgress;
                float const alpha =
                    SmoothStep(0.0f, 0.05f, lifetimeProgress)
                    - SmoothStep(0.7f, 1.0f, lifetimeProgress);

                // Upload smoke
                renderContext.UploadShipGenericMipMappedTextureRenderSpecification(
                    shipId,
                    GetPlaneId(pointIndex),
                    state.PersonalitySeed,
                    state.TextureGroup,
                    GetPosition(pointIndex),
                    scale,
                    alpha);

                break;
            }

            case EphemeralType::Sparkle:
            {
                vec2f const velocityVector =
                    -GetVelocity(pointIndex)
                    / GameParameters::MaxSparkleParticlesForCutVelocity; // We use the cut sparkles arbitrarily

                renderContext.UploadShipSparkle(
                    shipId,
                    GetPlaneId(pointIndex),
                    GetPosition(pointIndex),
                    velocityVector,
                    mEphemeralParticleAttributes2Buffer[pointIndex].State.Sparkle.Progress);

                break;
            }

            case EphemeralType::WakeBubble:
            {
                auto const & state = mEphemeralParticleAttributes2Buffer[pointIndex].State.WakeBubble;

                renderContext.UploadShipGenericMipMappedTextureRenderSpecification(
                    shipId,
                    GetPlaneId(pointIndex),
                    TextureFrameId(Render::GenericMipMappedTextureGroups::EngineWake, 0),
                    GetPosition(pointIndex),
                    0.10f + 1.22f * state.Progress, // Scale, magic formula
                    mRandomNormalizedUniformFloatBuffer[pointIndex] * 2.0f * Pi<float>, // Angle
                    1.0f - state.Progress); // Alpha

                break;
            }

            case EphemeralType::None:
            default:
            {
                // Ignore
                break;
            }
        }
    }

    if (mAreEphemeralPointsDirtyForRendering)
    {
        renderContext.UploadShipElementEphemeralPointsEnd(shipId);

        // Not dirty anymore
        mAreEphemeralPointsDirtyForRendering = false;
    }
}

void Points::UploadHighlights(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    for (auto const & h : mElectricalElementHighlightedPoints)
    {
        renderContext.UploadShipHighlight(
            shipId,
            HighlightModeType::ElectricalElement,
            GetPlaneId(h.PointIndex),
            GetPosition(h.PointIndex),
            5.0f, // HalfQuadSize, magic number
            h.HighlightColor,
            h.Progress);
    }

    for (auto const & h : mCircleHighlightedPoints)
    {
        renderContext.UploadShipHighlight(
            shipId,
            HighlightModeType::Circle,
            GetPlaneId(h.PointIndex),
            GetPosition(h.PointIndex),
            4.0f, // HalfQuadSize, magic number
            h.HighlightColor,
            1.0f);
    }
}

void Points::AugmentMaterialMass(
    ElementIndex pointElementIndex,
    float offset,
    Springs & springs)
{
    assert(pointElementIndex < mElementCount);

    mAugmentedMaterialMassBuffer[pointElementIndex] =
        GetStructuralMaterial(pointElementIndex).GetMass()
        + offset;

    // Notify all connected springs
    for (auto connectedSpring : mConnectedSpringsBuffer[pointElementIndex].ConnectedSprings)
    {
        springs.UpdateForMass(connectedSpring.SpringIndex, *this);
    }
}

void Points::UpdateMasses(GameParameters const & gameParameters)
{
    //
    // Update:
    //  - CurrentMass: augmented material mass + point's water mass
    //  - Integration factor: integration factor time coefficient / total mass
    //

    float const densityAdjustedWaterMass = GameParameters::WaterMass * gameParameters.WaterDensityAdjustment;

    float const * restrict const augmentedMaterialMassBuffer = mAugmentedMaterialMassBuffer.data();
    float const * restrict const waterBuffer = mWaterBuffer.data();
    float const * restrict const materialBuoyancyVolumeFillBuffer = mMaterialBuoyancyVolumeFillBuffer.data();
    float * restrict const massBuffer = mMassBuffer.data();
    float const * restrict const integrationFactorTimeCoefficientBuffer = mIntegrationFactorTimeCoefficientBuffer.data();
    float * restrict const integrationFactorBuffer = reinterpret_cast<float *>(mIntegrationFactorBuffer.data());

    size_t const count = GetBufferElementCount();
    for (size_t i = 0; i < count; ++i)
    {
        float const mass =
            augmentedMaterialMassBuffer[i]
            + std::min(waterBuffer[i], materialBuoyancyVolumeFillBuffer[i]) * densityAdjustedWaterMass;

        assert(mass > 0.0f);

        massBuffer[i] = mass;

        integrationFactorBuffer[i * 2] = integrationFactorTimeCoefficientBuffer[i] / mass;
        integrationFactorBuffer[i * 2 + 1] = integrationFactorTimeCoefficientBuffer[i] / mass;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

vec2f Points::CalculateIdealFlameVector(
    vec2f const & pointVelocity,
    float pointVelocityMagnitudeThreshold)
{
    // Vector Q is the vector describing the ideal, final flame's
    // direction and (unscaled) length.
    //
    // At rest it's (0, 1) - simply, the flame pointing upwards.
    // When the particle has velocity V, it is the interpolation of the rest upward
    // vector (B) with the opposite of the particle's velocity:
    //      Q = (1-a) * B - a * V
    // Where 'a' depends on the magnitude of the particle's velocity.

    // The interpolation factor depends on the magnitude of the particle's velocity,
    // via a magic formula; the more the particle's velocity, the more the resultant
    // vector is aligned with the particle's velocity
    float const interpolationFactor = SmoothStep(0.0f, pointVelocityMagnitudeThreshold, pointVelocity.length());

    vec2f constexpr B = vec2f(0.0f, 1.0f);
    vec2f Q = B * (1.0f - interpolationFactor) - pointVelocity * interpolationFactor;
    float const Ql = Q.length();

    // Qn = normalized Q
    vec2f const Qn = Q.normalise(Ql);

    // Limit length of Q: no more than Qlmax
    float constexpr Qlmax = 1.8f; // Magic number
    Q = Qn * std::min(Ql, Qlmax);

    return Q;
}

ElementIndex Points::FindFreeEphemeralParticle(
    float currentSimulationTime,
    bool doForce)
{
    //
    // Search for the firt free ephemeral particle; if a free one is not found, reuse the
    // oldest particle
    //

    ElementIndex oldestParticle = NoneElementIndex;
    float oldestParticleLifetime = 0.0f;

    assert(mFreeEphemeralParticleSearchStartIndex >= mAlignedShipPointCount
        && mFreeEphemeralParticleSearchStartIndex < mAllPointCount);

    for (ElementIndex p = mFreeEphemeralParticleSearchStartIndex; ; /*incremented in loop*/)
    {
        if (EphemeralType::None == mEphemeralParticleAttributes1Buffer[p].Type)
        {
            // Found!

            // Remember to start after this one next time
            mFreeEphemeralParticleSearchStartIndex = p + 1;
            if (mFreeEphemeralParticleSearchStartIndex >= mAllPointCount)
                mFreeEphemeralParticleSearchStartIndex = mAlignedShipPointCount;

            return p;
        }

        // Check whether it's the oldest
        auto const lifetime = currentSimulationTime - mEphemeralParticleAttributes1Buffer[p].StartSimulationTime;
        if (lifetime >= oldestParticleLifetime)
        {
            oldestParticle = p;
            oldestParticleLifetime = lifetime;
        }

        // Advance
        ++p;
        if (p >= mAllPointCount)
            p = mAlignedShipPointCount;

        if (p == mFreeEphemeralParticleSearchStartIndex)
        {
            // Went around
            break;
        }
    }

    //
    // No luck
    //

    if (!doForce)
        return NoneElementIndex;


    //
    // Steal the oldest
    //

    assert(NoneElementIndex != oldestParticle);

    // Remember to start after this one next time
    mFreeEphemeralParticleSearchStartIndex = oldestParticle + 1;
    if (mFreeEphemeralParticleSearchStartIndex >= mAllPointCount)
        mFreeEphemeralParticleSearchStartIndex = mAlignedShipPointCount;

    return oldestParticle;
}

}