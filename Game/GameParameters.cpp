/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-04-13
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "GameParameters.h"

GameParameters::GameParameters()
    // Dynamics
    : NumMechanicalDynamicsIterationsAdjustment(1.0f)
    , SpringStiffnessAdjustment(1.0f)
    , SpringDampingAdjustment(1.0f)
    , SpringStrengthAdjustment(1.0f)
    , RotAcceler8r(1.0f)
    // Water
    , WaterDensityAdjustment(1.0f)
    , WaterDragAdjustment(1.0f)
    , WaterIntakeAdjustment(1.0f)
    , WaterDiffusionSpeedAdjustment(1.0f)
    , WaterCrazyness(1.0f)
    // Ephemeral particles
    , DoGenerateDebris(true)
    , DoGenerateSparklesForCuts(true)
    , DoGenerateAirBubbles(true)
    , CumulatedIntakenWaterThresholdForAirBubbles(8.0f)
    // Wind
    , DoModulateWind(true)
    , WindSpeedBase(-20.0f)
    , WindSpeedMaxFactor(2.5f)
    , WindGustFrequencyAdjustment(1.0f)
    // Waves
    , BasalWaveHeightAdjustment(1.0f)
    , BasalWaveLengthAdjustment(1.0f)
    , BasalWaveSpeedAdjustment(4.0f)
    , TsunamiRate(25.0f)
    , RogueWaveRate(2.0f)
    // Storm
	, StormRate(30)
    , StormDuration(60 * 4) // 4 minutes
	, StormStrengthAdjustment(1.0f)
	, LightningBlastRadius(8.0f)
	, LightningBlastHeat(4000.0f)
	, DoRainWithStorm(true)
    // Heat and combustion
    , AirTemperature(298.15f) // 25C
    , WaterTemperature(288.15f) // 15C
    , MaxBurningParticles(112)
    , ThermalConductivityAdjustment(1.0f)
    , HeatDissipationAdjustment(1.0f)
    , IgnitionTemperatureAdjustment(1.0f)
    , MeltingTemperatureAdjustment(1.0f)
    , CombustionSpeedAdjustment(1.0f)
    , CombustionHeatAdjustment(1.0f)
    , HeatBlasterHeatFlow(2000.0f) // 900KJ: 80kg of iron (~=1 particle) get dT=1500 in 60 seconds
    , HeatBlasterRadius(8.0f)
    , ElectricalElementHeatProducedAdjustment(1.0f)
    // Misc
    , SeaDepth(300.0f)
    , OceanFloorBumpiness(1.0f)
    , OceanFloorDetailAmplification(10.0f)
    , LuminiscenceAdjustment(1.0f)
    , LightSpreadAdjustment(1.0f)
    , NumberOfStars(1536)
    , NumberOfClouds(48)
    // Interactions
    , ToolSearchRadius(2.0f)
    , DestroyRadius(8.0f)
    , RepairRadius(2.0f)
    , RepairSpeedAdjustment(1.0f)
    , BombBlastRadius(2.0f)
    , BombBlastHeat(40000.0f)
    , AntiMatterBombImplosionStrength(3.0f)
    , TimerBombInterval(10)
    , BombMass(5000.0f)
    , FloodRadius(0.75f)
    , FloodQuantity(1.0f)
    , FireExtinguisherRadius(5.0f)
    , ScrubRadius(7.0f)
    , IsUltraViolentMode(false)
    , MoveToolInertia(3.0f)
{
}