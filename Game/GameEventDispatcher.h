/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-03-05
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "IGameEventHandlers.h"

#include <GameCore/Log.h>
#include <GameCore/TupleKeys.h>

#include <algorithm>
#include <optional>
#include <vector>

class GameEventDispatcher final
    : public ILifecycleGameEventHandler
    , public IStructuralGameEventHandler
    , public IWavePhenomenaGameEventHandler
    , public ICombustionGameEventHandler
    , public IStatisticsGameEventHandler
    , public IAtmosphereGameEventHandler
    , public IElectricalElementGameEventHandler
    , public IGenericGameEventHandler
{
public:

    GameEventDispatcher()
        : mStressEvents()
        , mBreakEvents()
        , mCombustionExplosionEvents()
        , mLightningHitEvents()
        , mLightFlickerEvents()
        , mSpringRepairedEvents()
        , mTriangleRepairedEvents()
        , mAirBubbleSurfacedEvents(0u)
        , mBombExplosionEvents()
        , mRCBombPingEvents()
        , mTimerBombDefusedEvents()
        , mWatertightDoorOpenedEvents()
        , mWatertightDoorClosedEvents()
        // Sinks
        , mLifecycleSinks()
        , mStructuralSinks()
        , mWavePhenomenaSinks()
        , mCombustionSinks()
        , mStatisticsSinks()
        , mAtmosphereSinks()
        , mElectricalElementSinks()
        , mGenericSinks()
    {
    }

public:

    //
    // Lifecycle
    //

    virtual void OnGameReset() override
    {
        for (auto sink : mLifecycleSinks)
        {
            sink->OnGameReset();
        }
    }

    virtual void OnShipLoaded(
        unsigned int id,
        std::string const & name,
        std::optional<std::string> const & author) override
    {
        for (auto sink : mLifecycleSinks)
        {
            sink->OnShipLoaded(id, name, author);
        }
    }

    virtual void OnSinkingBegin(ShipId shipId) override
    {
        for (auto sink : mLifecycleSinks)
        {
            sink->OnSinkingBegin(shipId);
        }
    }

    virtual void OnSinkingEnd(ShipId shipId) override
    {
        for (auto sink : mLifecycleSinks)
        {
            sink->OnSinkingEnd(shipId);
        }
    }

    virtual void OnShipRepaired(ShipId shipId) override
    {
        for (auto sink : mLifecycleSinks)
        {
            sink->OnShipRepaired(shipId);
        }
    }

    //
    // Structural
    //

    virtual void OnStress(
        StructuralMaterial const & structuralMaterial,
        bool isUnderwater,
        unsigned int size) override
    {
        mStressEvents[std::make_tuple(&structuralMaterial, isUnderwater)] += size;
    }

    virtual void OnBreak(
        StructuralMaterial const & structuralMaterial,
        bool isUnderwater,
        unsigned int size) override
    {
        mBreakEvents[std::make_tuple(&structuralMaterial, isUnderwater)] += size;
    }

    //
    // Wave phenomena
    //

    virtual void OnTsunami(float x) override
    {
        for (auto sink : mWavePhenomenaSinks)
        {
            sink->OnTsunami(x);
        }
    }

    virtual void OnTsunamiNotification(float x) override
    {
        for (auto sink : mWavePhenomenaSinks)
        {
            sink->OnTsunamiNotification(x);
        }
    }

    //
    // Combustion
    //

    virtual void OnPointCombustionBegin() override
    {
        for (auto sink : mCombustionSinks)
        {
            sink->OnPointCombustionBegin();
        }
    }

    virtual void OnPointCombustionEnd() override
    {
        for (auto sink : mCombustionSinks)
        {
            sink->OnPointCombustionEnd();
        }
    }

    virtual void OnCombustionSmothered() override
    {
        for (auto sink : mCombustionSinks)
        {
            sink->OnCombustionSmothered();
        }
    }

    virtual void OnCombustionExplosion(
        bool isUnderwater,
        unsigned int size) override
    {
        mCombustionExplosionEvents[std::make_tuple(isUnderwater)] += size;
    }

    //
    // Statistics
    //

    virtual void OnFrameRateUpdated(
        float immediateFps,
        float averageFps) override
    {
        for (auto sink : mStatisticsSinks)
        {
            sink->OnFrameRateUpdated(
                immediateFps,
                averageFps);
        }
    }

    virtual void OnCurrentUpdateDurationUpdated(float currentUpdateDuration) override
    {
        for (auto sink : mStatisticsSinks)
        {
            sink->OnCurrentUpdateDurationUpdated(currentUpdateDuration);
        }
    }

    //
    // Atmosphere
    //

    virtual void OnStormBegin() override
    {
        for (auto sink : mAtmosphereSinks)
        {
            sink->OnStormBegin();
        }
    }

    virtual void OnStormEnd() override
    {
        for (auto sink : mAtmosphereSinks)
        {
            sink->OnStormEnd();
        }
    }

    virtual void OnWindSpeedUpdated(
        float const zeroSpeedMagnitude,
        float const baseSpeedMagnitude,
        float const baseAndStormSpeedMagnitude,
        float const preMaxSpeedMagnitude,
        float const maxSpeedMagnitude,
        vec2f const& windSpeed) override
    {
        for (auto sink : mAtmosphereSinks)
        {
            sink->OnWindSpeedUpdated(
                zeroSpeedMagnitude,
                baseSpeedMagnitude,
                baseAndStormSpeedMagnitude,
                preMaxSpeedMagnitude,
                maxSpeedMagnitude,
                windSpeed);
        }
    }

    virtual void OnRainUpdated(float const density) override
    {
        for (auto sink : mAtmosphereSinks)
        {
            sink->OnRainUpdated(density);
        }
    }

    virtual void OnThunder() override
    {
        for (auto sink : mAtmosphereSinks)
        {
            sink->OnThunder();
        }
    }

    virtual void OnLightning() override
    {
        for (auto sink : mAtmosphereSinks)
        {
            sink->OnLightning();
        }
    }

    virtual void OnLightningHit(StructuralMaterial const & structuralMaterial) override
    {
        mLightningHitEvents[std::make_tuple(&structuralMaterial)] += 1;
    }

    //
    // Electrical elements
    //

    virtual void OnLightFlicker(
        DurationShortLongType duration,
        bool isUnderwater,
        unsigned int size) override
    {
        mLightFlickerEvents[std::make_tuple(duration, isUnderwater)] += size;
    }

    virtual void OnElectricalElementAnnouncementsBegin() override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnElectricalElementAnnouncementsBegin();
        }
    }

    virtual void OnSwitchCreated(
        ElectricalElementId electricalElementId,
        ElectricalElementInstanceIndex instanceIndex,
        SwitchType type,
        ElectricalState state,
        std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata) override
    {
        LogMessage("OnSwitchCreated(EEID=", electricalElementId, " IID=", int(instanceIndex), "): State=", static_cast<bool>(state));

        for (auto sink : mElectricalElementSinks)
        {
            sink->OnSwitchCreated(electricalElementId, instanceIndex, type, state, panelElementMetadata);
        }
    }

    virtual void OnPowerProbeCreated(
        ElectricalElementId electricalElementId,
        ElectricalElementInstanceIndex instanceIndex,
        PowerProbeType type,
        ElectricalState state,
        std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata) override
    {
        LogMessage("OnPowerProbeCreated(EEID=", electricalElementId, " IID=", int(instanceIndex), "): State=", static_cast<bool>(state));

        for (auto sink : mElectricalElementSinks)
        {
            sink->OnPowerProbeCreated(electricalElementId, instanceIndex, type, state, panelElementMetadata);
        }
    }

    virtual void OnEngineControllerCreated(
        ElectricalElementId electricalElementId,
        ElectricalElementInstanceIndex instanceIndex,
        std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata) override
    {
        LogMessage("OnEngineControllerCreated(EEID=", electricalElementId, " IID=", int(instanceIndex), ")");

        for (auto sink : mElectricalElementSinks)
        {
            sink->OnEngineControllerCreated(electricalElementId, instanceIndex, panelElementMetadata);
        }
    }

    virtual void OnEngineMonitorCreated(
        ElectricalElementId electricalElementId,
        ElectricalElementInstanceIndex instanceIndex,
        ElectricalMaterial const & electricalMaterial,
        float thrustMagnitude,
        float rpm,
        std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata) override
    {
        LogMessage("OnEngineMonitorCreated(EEID=", electricalElementId, " IID=", int(instanceIndex), "): Thrust=", thrustMagnitude, " RPM=", rpm);

        for (auto sink : mElectricalElementSinks)
        {
            sink->OnEngineMonitorCreated(electricalElementId, instanceIndex, electricalMaterial, thrustMagnitude, rpm, panelElementMetadata);
        }
    }

    virtual void OnWaterPumpCreated(
        ElectricalElementId electricalElementId,
        ElectricalElementInstanceIndex instanceIndex,
        ElectricalMaterial const & electricalMaterial,
        float normalizedForce,
        std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata) override
    {
        LogMessage("OnWaterPumpCreated(EEID=", electricalElementId, " IID=", int(instanceIndex), ")");

        for (auto sink : mElectricalElementSinks)
        {
            sink->OnWaterPumpCreated(electricalElementId, instanceIndex, electricalMaterial, normalizedForce, panelElementMetadata);
        }
    }

    virtual void OnWatertightDoorCreated(
        ElectricalElementId electricalElementId,
        ElectricalElementInstanceIndex instanceIndex,
        ElectricalMaterial const & electricalMaterial,
        bool isOpen,
        std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata) override
    {
        LogMessage("OnWatertightDoorCreated(EEID=", electricalElementId, " IID=", int(instanceIndex), ")");

        for (auto sink : mElectricalElementSinks)
        {
            sink->OnWatertightDoorCreated(electricalElementId, instanceIndex, electricalMaterial, isOpen, panelElementMetadata);
        }
    }

    virtual void OnElectricalElementAnnouncementsEnd() override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnElectricalElementAnnouncementsEnd();
        }
    }

    virtual void OnSwitchEnabled(
        ElectricalElementId electricalElementId,
        bool isEnabled) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnSwitchEnabled(electricalElementId, isEnabled);
        }
    }

    virtual void OnSwitchToggled(
        ElectricalElementId electricalElementId,
        ElectricalState newState) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnSwitchToggled(electricalElementId, newState);
        }
    }

    virtual void OnPowerProbeToggled(
        ElectricalElementId electricalElementId,
        ElectricalState newState) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnPowerProbeToggled(electricalElementId, newState);
        }
    }

    virtual void OnEngineControllerEnabled(
        ElectricalElementId electricalElementId,
        bool isEnabled) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnEngineControllerEnabled(electricalElementId, isEnabled);
        }
    }

    virtual void OnEngineControllerUpdated(
        ElectricalElementId electricalElementId,
        int telegraphValue) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnEngineControllerUpdated(electricalElementId, telegraphValue);
        }
    }

    virtual void OnEngineMonitorUpdated(
        ElectricalElementId electricalElementId,
        float thrustMagnitude,
        float rpm) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnEngineMonitorUpdated(electricalElementId, thrustMagnitude, rpm);
        }
    }

    virtual void OnShipSoundUpdated(
        ElectricalElementId electricalElementId,
        ElectricalMaterial const & electricalMaterial,
        bool isPlaying,
        bool isUnderwater) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnShipSoundUpdated(electricalElementId, electricalMaterial, isPlaying, isUnderwater);
        }
    }

    virtual void OnWaterPumpEnabled(
        ElectricalElementId electricalElementId,
        bool isEnabled) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnWaterPumpEnabled(electricalElementId, isEnabled);
        }
    }

    virtual void OnWaterPumpUpdated(
        ElectricalElementId electricalElementId,
        float normalizedForce) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnWaterPumpUpdated(electricalElementId, normalizedForce);
        }
    }

    virtual void OnWatertightDoorEnabled(
        ElectricalElementId electricalElementId,
        bool isEnabled) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnWatertightDoorEnabled(electricalElementId, isEnabled);
        }
    }

    virtual void OnWatertightDoorUpdated(
        ElectricalElementId electricalElementId,
        bool isOpen) override
    {
        for (auto sink : mElectricalElementSinks)
        {
            sink->OnWatertightDoorUpdated(electricalElementId, isOpen);
        }
    }

    //
    // Generic
    //

    virtual void OnDestroy(
        StructuralMaterial const & structuralMaterial,
        bool isUnderwater,
        unsigned int size) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnDestroy(structuralMaterial, isUnderwater, size);
        }
    }

    virtual void OnSpringRepaired(
        StructuralMaterial const & structuralMaterial,
        bool isUnderwater,
        unsigned int size) override
    {
        mSpringRepairedEvents[std::make_tuple(&structuralMaterial, isUnderwater)] += size;
    }

    virtual void OnTriangleRepaired(
        StructuralMaterial const & structuralMaterial,
        bool isUnderwater,
        unsigned int size) override
    {
        mTriangleRepairedEvents[std::make_tuple(&structuralMaterial, isUnderwater)] += size;
    }

    virtual void OnSawed(
        bool isMetal,
        unsigned int size) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnSawed(isMetal, size);
        }
    }

    virtual void OnPinToggled(
        bool isPinned,
        bool isUnderwater) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnPinToggled(isPinned, isUnderwater);
        }
    }

    virtual void OnWaterTaken(float waterTaken) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnWaterTaken(waterTaken);
        }
    }

    virtual void OnWaterSplashed(float waterSplashed) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnWaterSplashed(waterSplashed);
        }
    }

    virtual void OnAirBubbleSurfaced(unsigned int size) override
    {
        mAirBubbleSurfacedEvents += size;
    }

    virtual void OnSilenceStarted() override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnSilenceStarted();
        }
    }

    virtual void OnSilenceLifted() override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnSilenceLifted();
        }
    }

    virtual void OnCustomProbe(
        std::string const & name,
        float value) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnCustomProbe(
                name,
                value);
        }
    }

    virtual void OnBombPlaced(
        BombId bombId,
        BombType bombType,
        bool isUnderwater) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnBombPlaced(
                bombId,
                bombType,
                isUnderwater);
        }
    }

    virtual void OnBombRemoved(
        BombId bombId,
        BombType bombType,
        std::optional<bool> isUnderwater) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnBombRemoved(
                bombId,
                bombType,
                isUnderwater);
        }
    }

    virtual void OnBombExplosion(
        BombType bombType,
        bool isUnderwater,
        unsigned int size) override
    {
        mBombExplosionEvents[std::make_tuple(bombType, isUnderwater)] += size;
    }

    virtual void OnRCBombPing(
        bool isUnderwater,
        unsigned int size) override
    {
        mRCBombPingEvents[std::make_tuple(isUnderwater)] += size;
    }

    virtual void OnTimerBombFuse(
        BombId bombId,
        std::optional<bool> isFast) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnTimerBombFuse(
                bombId,
                isFast);
        }
    }

    virtual void OnTimerBombDefused(
        bool isUnderwater,
        unsigned int size) override
    {
        mTimerBombDefusedEvents[std::make_tuple(isUnderwater)] += size;
    }

    virtual void OnAntiMatterBombContained(
        BombId bombId,
        bool isContained) override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnAntiMatterBombContained(
                bombId,
                isContained);
        }
    }

    virtual void OnAntiMatterBombPreImploding() override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnAntiMatterBombPreImploding();
        }
    }

    virtual void OnAntiMatterBombImploding() override
    {
        for (auto sink : mGenericSinks)
        {
            sink->OnAntiMatterBombImploding();
        }
    }

    virtual void OnWatertightDoorOpened(
        bool isUnderwater,
        unsigned int size) override
    {
        mWatertightDoorOpenedEvents[std::make_tuple(isUnderwater)] += size;
    }

    virtual void OnWatertightDoorClosed(
        bool isUnderwater,
        unsigned int size) override
    {
        mWatertightDoorClosedEvents[std::make_tuple(isUnderwater)] += size;
    }

public:

    /*
     * Flushes all events aggregated so far and clears the state.
     */
    void Flush()
    {
        //
        // Publish aggregations
        //

        for (auto * sink : mStructuralSinks)
        {
            for (auto const & entry : mStressEvents)
            {
                sink->OnStress(*(std::get<0>(entry.first)), std::get<1>(entry.first), entry.second);
            }

            for (auto const & entry : mBreakEvents)
            {
                sink->OnBreak(*(std::get<0>(entry.first)), std::get<1>(entry.first), entry.second);
            }
        }

        mStressEvents.clear();
        mBreakEvents.clear();

        for (auto * sink : mCombustionSinks)
        {
            for (auto const & entry : mCombustionExplosionEvents)
            {
                sink->OnCombustionExplosion(std::get<0>(entry.first), entry.second);
            }
        }

        mCombustionExplosionEvents.clear();

        for (auto * sink : mAtmosphereSinks)
        {
            for (auto const & entry : mLightningHitEvents)
            {
                sink->OnLightningHit(*(std::get<0>(entry.first)));
            }
        }

        mLightningHitEvents.clear();

        for (auto * sink : mElectricalElementSinks)
        {
            for (auto const & entry : mLightFlickerEvents)
            {
                sink->OnLightFlicker(std::get<0>(entry.first), std::get<1>(entry.first), entry.second);
            }
        }

        mLightFlickerEvents.clear();

        for (auto * sink : mGenericSinks)
        {
            for (auto const & entry : mSpringRepairedEvents)
            {
                sink->OnSpringRepaired(*(std::get<0>(entry.first)), std::get<1>(entry.first), entry.second);
            }

            for (auto const & entry : mTriangleRepairedEvents)
            {
                sink->OnTriangleRepaired(*(std::get<0>(entry.first)), std::get<1>(entry.first), entry.second);
            }

            if (mAirBubbleSurfacedEvents > 0)
            {
                sink->OnAirBubbleSurfaced(mAirBubbleSurfacedEvents);
            }

            for (auto const & entry : mBombExplosionEvents)
            {
                sink->OnBombExplosion(std::get<0>(entry.first), std::get<1>(entry.first), entry.second);
            }

            for (auto const & entry : mRCBombPingEvents)
            {
                sink->OnRCBombPing(std::get<0>(entry.first), entry.second);
            }

            for (auto const & entry : mTimerBombDefusedEvents)
            {
                sink->OnTimerBombDefused(std::get<0>(entry.first), entry.second);
            }

            for (auto const & entry : mWatertightDoorOpenedEvents)
            {
                sink->OnWatertightDoorOpened(std::get<0>(entry.first), entry.second);
            }

            for (auto const & entry : mWatertightDoorClosedEvents)
            {
                sink->OnWatertightDoorClosed(std::get<0>(entry.first), entry.second);
            }
        }

        mSpringRepairedEvents.clear();
        mTriangleRepairedEvents.clear();
        mAirBubbleSurfacedEvents = 0u;
        mBombExplosionEvents.clear();
        mRCBombPingEvents.clear();
        mTimerBombDefusedEvents.clear();
        mWatertightDoorOpenedEvents.clear();
        mWatertightDoorClosedEvents.clear();
    }

    void RegisterLifecycleEventHandler(ILifecycleGameEventHandler * sink)
    {
        mLifecycleSinks.push_back(sink);
    }

    void RegisterStructuralEventHandler(IStructuralGameEventHandler * sink)
    {
        mStructuralSinks.push_back(sink);
    }

    void RegisterWavePhenomenaEventHandler(IWavePhenomenaGameEventHandler * sink)
    {
        mWavePhenomenaSinks.push_back(sink);
    }

    void RegisterCombustionEventHandler(ICombustionGameEventHandler * sink)
    {
        mCombustionSinks.push_back(sink);
    }

    void RegisterStatisticsEventHandler(IStatisticsGameEventHandler * sink)
    {
        mStatisticsSinks.push_back(sink);
    }

    void RegisterAtmosphereEventHandler(IAtmosphereGameEventHandler* sink)
    {
        mAtmosphereSinks.push_back(sink);
    }

    void RegisterElectricalElementEventHandler(IElectricalElementGameEventHandler * sink)
    {
        mElectricalElementSinks.push_back(sink);
    }

    void RegisterGenericEventHandler(IGenericGameEventHandler * sink)
    {
        mGenericSinks.push_back(sink);
    }

private:

    // The current events being aggregated
    unordered_tuple_map<std::tuple<StructuralMaterial const *, bool>, unsigned int> mStressEvents;
    unordered_tuple_map<std::tuple<StructuralMaterial const *, bool>, unsigned int> mBreakEvents;
    unordered_tuple_map<std::tuple<bool>, unsigned int> mCombustionExplosionEvents;
    unordered_tuple_map<std::tuple<StructuralMaterial const *>, unsigned int> mLightningHitEvents;
    unordered_tuple_map<std::tuple<DurationShortLongType, bool>, unsigned int> mLightFlickerEvents;
    unordered_tuple_map<std::tuple<StructuralMaterial const *, bool>, unsigned int> mSpringRepairedEvents;
    unordered_tuple_map<std::tuple<StructuralMaterial const *, bool>, unsigned int> mTriangleRepairedEvents;
    unsigned int mAirBubbleSurfacedEvents;
    unordered_tuple_map<std::tuple<BombType, bool>, unsigned int> mBombExplosionEvents;
    unordered_tuple_map<std::tuple<bool>, unsigned int> mRCBombPingEvents;
    unordered_tuple_map<std::tuple<bool>, unsigned int> mTimerBombDefusedEvents;
    unordered_tuple_map<std::tuple<bool>, unsigned int> mWatertightDoorOpenedEvents;
    unordered_tuple_map<std::tuple<bool>, unsigned int> mWatertightDoorClosedEvents;

    // The registered sinks
    std::vector<ILifecycleGameEventHandler *> mLifecycleSinks;
    std::vector<IStructuralGameEventHandler *> mStructuralSinks;
    std::vector<IWavePhenomenaGameEventHandler *> mWavePhenomenaSinks;
    std::vector<ICombustionGameEventHandler *> mCombustionSinks;
    std::vector<IStatisticsGameEventHandler *> mStatisticsSinks;
    std::vector<IAtmosphereGameEventHandler *> mAtmosphereSinks;
    std::vector<IElectricalElementGameEventHandler *> mElectricalElementSinks;
    std::vector<IGenericGameEventHandler *> mGenericSinks;
};
