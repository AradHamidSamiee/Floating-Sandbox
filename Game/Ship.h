/***************************************************************************************
 * Original Author:     Gabriele Giuseppini
 * Created:             2018-01-21
 * Copyright:           Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
 ***************************************************************************************/
#pragma once

#include "GameEventDispatcher.h"
#include "GameParameters.h"
#include "MaterialDatabase.h"
#include "Physics.h"
#include "RenderContext.h"
#include "ShipDefinition.h"

#include <GameCore/GameTypes.h>
#include <GameCore/RunningAverage.h>
#include <GameCore/TaskThreadPool.h>
#include <GameCore/Vectors.h>

#include <list>
#include <memory>
#include <optional>
#include <vector>

namespace Physics
{

class Ship final : public IShipPhysicsHandler
{
public:

    Ship(
        ShipId id,
        World & parentWorld,
        MaterialDatabase const & materialDatabase,
        std::shared_ptr<GameEventDispatcher> gameEventDispatcher,
        std::shared_ptr<TaskThreadPool> taskThreadPool,
        Points && points,
        Springs && springs,
        Triangles && triangles,
        ElectricalElements && electricalElements);

    void Announce();

    inline ShipId GetId() const { return mId; }

    inline World const & GetParentWorld() const { return mParentWorld; }
    inline World & GetParentWorld() { return mParentWorld; }

    inline size_t GetPointCount() const { return mPoints.GetElementCount(); }

    inline vec2f GetSize() const { return mSize; }

    inline auto const & GetPoints() const { return mPoints; }
    inline auto & GetPoints() { return mPoints; }

    bool IsUnderwater(ElementIndex pointElementIndex) const;

    void Update(
        float currentSimulationTime,
		Storm::Parameters const & stormParameters,
        GameParameters const & gameParameters,
        Render::RenderContext const & renderContext);

    void RenderUpload(
        GameParameters const & gameParameters,
        Render::RenderContext & renderContext);

public:

    void Finalize();

    ///////////////////////////////////////////////////////////////
    // Interactions
    ///////////////////////////////////////////////////////////////

    std::optional<ElementIndex> PickPointToMove(
        vec2f const & pickPosition,
        GameParameters const & gameParameters) const;

    void MoveBy(
        ElementIndex pointElementIndex,
        vec2f const & offset,
        vec2f const & inertialVelocity,
        GameParameters const & gameParameters);

    void MoveBy(
        vec2f const & offset,
        vec2f const & inertialVelocity,
        GameParameters const & gameParameters);

    void RotateBy(
        ElementIndex pointElementIndex,
        float angle,
        vec2f const & center,
        float inertialAngle,
        GameParameters const & gameParameters);

    void RotateBy(
        float angle,
        vec2f const & center,
        float inertialAngle,
        GameParameters const & gameParameters);

    std::optional<ElementIndex> PickObjectForPickAndPull(
        vec2f const & pickPosition,
        GameParameters const & gameParameters);

    void Pull(
        ElementIndex pointElementIndex,
        vec2f const & target,
        GameParameters const & gameParameters);

    void DestroyAt(
        vec2f const & targetPos,
        float radiusFraction,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void RepairAt(
        vec2f const & targetPos,
        float radiusMultiplier,
        RepairSessionId sessionId,
        RepairSessionStepId sessionStepId,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void SawThrough(
        vec2f const & startPos,
        vec2f const & endPos,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    bool ApplyHeatBlasterAt(
        vec2f const & targetPos,
        HeatBlasterActionType action,
        float radius,
        GameParameters const & gameParameters);

    bool ExtinguishFireAt(
        vec2f const & targetPos,
        float radius,
        GameParameters const & gameParameters);

    void DrawTo(
        vec2f const & targetPos,
        float strengthFraction,
        GameParameters const & gameParameters);

    void SwirlAt(
        vec2f const & targetPos,
        float strengthFraction,
        GameParameters const & gameParameters);

    bool TogglePinAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    bool InjectBubblesAt(
        vec2f const & targetPos,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    bool FloodAt(
        vec2f const & targetPos,
        float waterQuantityMultiplier,
        GameParameters const & gameParameters);

    bool ToggleAntiMatterBombAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    bool ToggleImpactBombAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    bool ToggleRCBombAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    bool ToggleTimerBombAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    void DetonateRCBombs();

    void DetonateAntiMatterBombs();

    bool ScrubThrough(
        vec2f const & startPos,
        vec2f const & endPos,
        GameParameters const & gameParameters);

    void ApplyThanosSnap(
        float centerX,
        float radius,
        float leftFrontX,
        float rightFrontX,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    ElementIndex GetNearestPointAt(
        vec2f const & targetPos,
        float radius) const;

    bool QueryNearestPointAt(
        vec2f const & targetPos,
        float radius) const;

	std::optional<vec2f> FindSuitableLightningTarget() const;

	void ApplyLightning(
		vec2f const & targetPos,
		float currentSimulationTime,
		GameParameters const & gameParameters);

    void HighlightElectricalElement(ElectricalElementId electricalElementId);

    void SetSwitchState(
        ElectricalElementId electricalElementId,
        ElectricalState switchState,
        GameParameters const & gameParameters);

    void SetEngineControllerState(
        ElectricalElementId electricalElementId,
        int telegraphValue,
        GameParameters const & gameParameters);

private:

    /////////////////////////////////////////////////////////////////////////
    // Dynamics
    /////////////////////////////////////////////////////////////////////////

    // Mechanical

    void ApplyWorldForces(
        Storm::Parameters const & stormParameters,
        GameParameters const & gameParameters);

    void ApplySpringsForces_ByPoints(GameParameters const & gameParameters);

    void ApplySpringsForces_BySprings(GameParameters const & gameParameters);

    void IntegrateAndResetSpringForces(GameParameters const & gameParameters);

    void HandleCollisionsWithSeaFloor(GameParameters const & gameParameters);

    void TrimForWorldBounds(GameParameters const & gameParameters);

    // Water

    void UpdateWaterInflow(
        float currentSimulationTime,
        Storm::Parameters const & stormParameters,
        GameParameters const & gameParameters,
        float & waterTakenInStep);

    void UpdateWaterVelocities(
        GameParameters const & gameParameters,
        float & waterSplashed);

    void UpdateSinking();

    // Electrical

    void DiffuseLight(GameParameters const & gameParameters);

    // Heat

    void PropagateHeat(
        float currentSimulationTime,
        float dt,
		Storm::Parameters const & stormParameters,
        GameParameters const & gameParameters);

    // Misc

    void RotPoints(
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void UpdateStateMachines(
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void UploadStateMachines(Render::RenderContext & renderContext);

private:

    /////////////////////////////////////////////////////////////////////////
    // Force Fields
    /////////////////////////////////////////////////////////////////////////

    void ApplyDrawForceField(
        vec2f const & centerPosition,
        float strength);

    void ApplySwirlForceField(
        vec2f const & centerPosition,
        float strength);

    void ApplyBlastForceField(
        vec2f const & centerPosition,
        float blastRadius,
        float strength,
        bool doDetachPoint,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void ApplyRadialSpaceWarpForceField(
        vec2f const & centerPosition,
        float radius,
        float radiusThickness,
        float strength);

    void ApplyImplosionForceField(
        vec2f const & centerPosition,
        float strength);

    void ApplyRadialExplosionForceField(
        vec2f const & centerPosition,
        float strength);

private:

    void RunConnectivityVisit();

    inline void SetAndPropagateResultantPointHullness(
        ElementIndex pointElementIndex,
        bool isHull);

    void DestroyConnectedTriangles(ElementIndex pointElementIndex);

    void DestroyConnectedTriangles(
        ElementIndex pointAElementIndex,
        ElementIndex pointBElementIndex);

    void AttemptPointRestore(ElementIndex pointElementIndex);

    void GenerateAirBubbles(
        vec2f const & position,
        float temperature,
        float currentSimulationTime,
        PlaneId planeId,
        GameParameters const & gameParameters);

    void GenerateDebris(
        ElementIndex pointElementIndex,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void GenerateSparklesForCut(
        ElementIndex springElementIndex,
        vec2f const & cutDirectionStartPos,
        vec2f const & cutDirectionEndPos,
        float currentSimulationTime,
        GameParameters const & gameParameters);

	void GenerateSparklesForLightning(
		ElementIndex pointElementIndex,
		float currentSimulationTime,
		GameParameters const & gameParameters);

    inline size_t GetPointConnectedComponentSize(ElementIndex pointIndex) const noexcept
    {
        auto const connCompId = mPoints.GetConnectedComponentId(pointIndex);
        if (NoneConnectedComponentId == connCompId)
            return 0;

        return mConnectedComponentSizes[static_cast<size_t>(connCompId)];
    }

private:

    /////////////////////////////////////////////////////////////////////////
    // IShipPhysicsHandler
    /////////////////////////////////////////////////////////////////////////

    virtual void HandlePointDetach(
        ElementIndex pointElementIndex,
        bool generateDebris,
        bool fireDestroyEvent,
        float currentSimulationTime,
        GameParameters const & gameParameters) override;

    virtual void HandlePointDamaged(ElementIndex pointElementIndex) override;

    virtual void HandleEphemeralParticleDestroy(
        ElementIndex pointElementIndex) override;

    virtual void HandlePointRestore(ElementIndex pointElementIndex) override;

    virtual void HandleSpringDestroy(
        ElementIndex springElementIndex,
        bool destroyAllTriangles,
        GameParameters const & gameParameters) override;

    virtual void HandleSpringRestore(
        ElementIndex springElementIndex,
        GameParameters const & gameParameters) override;

    virtual void HandleTriangleDestroy(ElementIndex triangleElementIndex) override;

    virtual void HandleTriangleRestore(ElementIndex triangleElementIndex) override;

    virtual void HandleElectricalElementDestroy(ElementIndex electricalElementIndex) override;

    virtual void HandleElectricalElementRestore(ElementIndex electricalElementIndex) override;

    virtual void StartExplosion(
        float currentSimulationTime,
        PlaneId planeId,
        vec2f const & centerPosition,
        float blastRadius,
        float blastStrength,
        float blastHeat,
        ExplosionType explosionType,
        GameParameters const & gameParameters) override;

    virtual void DoAntiMatterBombPreimplosion(
        vec2f const & centerPosition,
        float sequenceProgress,
        float radius,
        GameParameters const & gameParameters) override;

    virtual void DoAntiMatterBombImplosion(
        vec2f const & centerPosition,
        float sequenceProgress,
        GameParameters const & gameParameters) override;

    virtual void DoAntiMatterBombExplosion(
        vec2f const & centerPosition,
        float sequenceProgress,
        GameParameters const & gameParameters) override;

    virtual void HandleWatertightDoorUpdated(
        ElementIndex pointElementIndex,
        bool isOpen) override;

private:

#ifdef _DEBUG
    void VerifyInvariants();
#endif

private:

    /////////////////////////////////////////////////////////////////////////
    // State machines
    /////////////////////////////////////////////////////////////////////////

    enum class StateMachineType
    {
        Explosion
    };

    struct StateMachine
    {
    public:

        StateMachineType const Type;

        StateMachine(StateMachineType type)
            : Type(type)
        {}

        virtual ~StateMachine()
        {}
    };

    struct ExplosionStateMachine;

    inline bool UpdateExplosionStateMachine(
        ExplosionStateMachine & explosionStateMachine,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    inline void UploadExplosionStateMachine(
        ExplosionStateMachine const & explosionStateMachine,
        Render::RenderContext & renderContext);

    std::list<std::unique_ptr<StateMachine>> mStateMachines;

private:

    ShipId const mId;
    World & mParentWorld;
    MaterialDatabase const & mMaterialDatabase;
    std::shared_ptr<GameEventDispatcher> mGameEventHandler;
    std::shared_ptr<TaskThreadPool> mTaskThreadPool;

    // The (initial) world size of  the ship
    vec2f const mSize;

    // All the ship elements - never removed, the repositories maintain their own size forever
    Points mPoints;
    Springs mSprings;
    Triangles mTriangles;
    ElectricalElements mElectricalElements;

    // Pinned points
    PinnedPoints mPinnedPoints;

    // Bombs
    Bombs mBombs;

    // The current simulation sequence number
    SequenceNumber mCurrentSimulationSequenceNumber;

    // The current connectivity visit sequence number
    SequenceNumber mCurrentConnectivityVisitSequenceNumber;

    // The max plane ID we have seen - ever
    PlaneId mMaxMaxPlaneId;

    // The current electrical connectivity visit sequence number
    SequenceNumber mCurrentElectricalVisitSequenceNumber;

    // The number of points in each connected component
    std::vector<size_t> mConnectedComponentSizes;

    // Flag remembering whether the structure of the ship (i.e. the connectivity between elements)
    // has changed since the last step.
    // When this flag is set, we'll re-detect connected components and planes, and re-upload elements
    // to the rendering context
    bool mIsStructureDirty;

    // Counts of elements currently broken - update each time an element is broken
    // or restored
    ElementCount mDamagedPointsCount;
    ElementCount mBrokenSpringsCount;
    ElementCount mBrokenTrianglesCount;

    // Sinking detection
    bool mIsSinking;

    // Water splashes
    RunningAverage<30> mWaterSplashedRunningAverage;

    // Last luminiscence adjustment that we've run the light diffusion algorithm with;
    // used to avoid running diffusion when luminiscence adjustment is zero and we've
    // already ran once with zero (so to zero out buffer)
    float mLastLuminiscenceAdjustmentDiffused;

    //
    // Render members
    //

    // The debug ship render mode that was in effect the last time we've uploaded elements;
    // used to detect changes and eventually re-upload
    std::optional<DebugShipRenderModeType> mLastUploadedDebugShipRenderMode;

    // Initial indices of the triangles for each plane ID;
    // last extra element contains total number of triangles
    std::vector<size_t> mPlaneTriangleIndicesToRender;

    // The wind speed magnitude to use for rendering
    float mWindSpeedMagnitudeToRender;
};

}