/***************************************************************************************
 * Original Author:     Gabriele Giuseppini
 * Created:             2018-01-21
 * Copyright:           Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
 ***************************************************************************************/
#pragma once

#include "GameEventDispatcher.h"
#include "GameParameters.h"
#include "MaterialDatabase.h"
#include "PerfStats.h"
#include "Physics.h"
#include "RenderContext.h"
#include "ShipDefinition.h"
#include "ShipTexturizer.h"

#include <GameCore/AABB.h>
#include <GameCore/GameChronometer.h>
#include <GameCore/ImageData.h>
#include <GameCore/TaskThreadPool.h>
#include <GameCore/Vectors.h>

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace Physics
{

class World
{
public:

    World(
        OceanFloorTerrain && oceanFloorTerrain,
        std::shared_ptr<GameEventDispatcher> gameEventDispatcher,
        std::shared_ptr<TaskThreadPool> taskThreadPool,
        GameParameters const & gameParameters);

    std::tuple<ShipId, RgbaImageData> AddShip(
        ShipDefinition && shipDefinition,
        MaterialDatabase const & materialDatabase,
        ShipTexturizer const & shipTexturizer,
        GameParameters const & gameParameters);

    void Announce();

    float GetCurrentSimulationTime() const
    {
        return mCurrentSimulationTime;
    }

    size_t GetShipCount() const;

    size_t GetShipPointCount(ShipId shipId) const;

    vec2f GetShipSize(ShipId shipId) const;

    inline float GetOceanSurfaceHeightAt(float x) const
    {
        return mOceanSurface.GetHeightAt(x);
    }

    inline void DisplaceOceanSurfaceAt(
        float x,
        float yOffset)
    {
        mOceanSurface.DisplaceAt(x, yOffset);
    }

    inline bool IsUnderwater(vec2f const & position) const
    {
        return position.y < GetOceanSurfaceHeightAt(position.x);
    }

    bool IsUnderwater(ElementId elementId) const;

    inline float GetOceanFloorHeightAt(float x) const
    {
        return mOceanFloor.GetHeightAt(x);
    }

    inline void DisplaceOceanFloorAt(
        float x,
        float yOffset)
    {
        mOceanFloor.DisplaceAt(x, yOffset);
    }

    inline vec2f const & GetCurrentWindSpeed() const
    {
        return mWind.GetCurrentWindSpeed();
    }

    inline void SetOceanFloorTerrain(OceanFloorTerrain const & oceanFloorTerrain)
    {
        mOceanFloor.SetTerrain(oceanFloorTerrain);
    }

    inline OceanFloorTerrain const & GetOceanFloorTerrain() const
    {
        return mOceanFloor.GetTerrain();
    }


    //
    // Interactions
    //

    void PickPointToMove(
        vec2f const & pickPosition,
        std::optional<ElementId> & elementId,
        GameParameters const & gameParameters) const;

    void MoveBy(
        ElementId elementId,
        vec2f const & offset,
        vec2f const & inertialVelocity,
        GameParameters const & gameParameters);

    void MoveBy(
        ShipId shipId,
        vec2f const & offset,
        vec2f const & inertialVelocity,
        GameParameters const & gameParameters);

    void RotateBy(
        ElementId elementId,
        float angle,
        vec2f const & center,
        float inertialAngle,
        GameParameters const & gameParameters);

    void RotateBy(
        ShipId shipId,
        float angle,
        vec2f const & center,
        float inertialAngle,
        GameParameters const & gameParameters);

    std::optional<ElementId> PickObjectForPickAndPull(
        vec2f const & pickPosition,
        GameParameters const & gameParameters);

    void Pull(
        ElementId elementId,
        vec2f const & target,
        GameParameters const & gameParameters);

    void DestroyAt(
        vec2f const & targetPos,
        float radiusFraction,
        GameParameters const & gameParameters);

    void RepairAt(
        vec2f const & targetPos,
        float radiusMultiplier,
        RepairSessionId sessionId,
        RepairSessionStepId sessionStepId,
        GameParameters const & gameParameters);

    void SawThrough(
        vec2f const & startPos,
        vec2f const & endPos,
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

    void TogglePinAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    bool InjectBubblesAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    bool FloodAt(
        vec2f const & targetPos,
        float waterQuantityMultiplier,
        GameParameters const & gameParameters);

    void ToggleAntiMatterBombAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    void ToggleImpactBombAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    void ToggleRCBombAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    void ToggleTimerBombAt(
        vec2f const & targetPos,
        GameParameters const & gameParameters);

    void DetonateRCBombs();

    void DetonateAntiMatterBombs();

    void AdjustOceanSurfaceTo(std::optional<vec2f> const & worldCoordinates);

    std::optional<bool> AdjustOceanFloorTo(
        float x1,
        float targetY1,
        float x2,
        float targetY2);

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

    std::optional<ElementId> GetNearestPointAt(
        vec2f const & targetPos,
        float radius) const;

    void QueryNearestPointAt(
        vec2f const & targetPos,
        float radius) const;

    std::optional<vec2f> FindSuitableLightningTarget() const;

    void ApplyLightning(
        vec2f const & targetPos,
        float currentSimulationTime,
        GameParameters const & gameParameters);

    void TriggerTsunami();

    void TriggerRogueWave();

    void TriggerStorm();

    void TriggerLightning();

    void HighlightElectricalElement(ElectricalElementId electricalElementId);

    void SetSwitchState(
        ElectricalElementId electricalElementId,
        ElectricalState switchState,
        GameParameters const & gameParameters);

    void SetEngineControllerState(
        ElectricalElementId electricalElementId,
        int telegraphValue,
        GameParameters const & gameParameters);

    void SetSilence(float silenceAmount);

public:

    void Update(
        GameParameters const & gameParameters,
        Render::RenderContext & renderContext,
        PerfStats & perfStats);

    void RenderUpload(
        GameParameters const & gameParameters,
        Render::RenderContext & renderContext,
        PerfStats & perfStats);

private:

    // The current simulation time
    float mCurrentSimulationTime;

    // Repository
    std::vector<std::unique_ptr<Ship>> mAllShips;
    Stars mStars;
    Storm mStorm;
    Wind mWind;
    Clouds mClouds;
    OceanSurface mOceanSurface;
    OceanFloor mOceanFloor;

    // The game event handler
    std::shared_ptr<GameEventDispatcher> mGameEventHandler;

    // The task thread pool that we use for concurrency
    std::shared_ptr<TaskThreadPool> mTaskThreadPool;
};

}
