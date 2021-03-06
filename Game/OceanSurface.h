/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-04-14
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "GameEventDispatcher.h"
#include "GameParameters.h"

#include <GameCore/GameMath.h>
#include <GameCore/PrecalculatedFunction.h>
#include <GameCore/RunningAverage.h>

#include <memory>
#include <optional>

namespace Physics
{

class OceanSurface
{
public:

    OceanSurface(std::shared_ptr<GameEventDispatcher> gameEventDispatcher);

    void Update(
        float currentSimulationTime,
        Wind const & wind,
        GameParameters const & gameParameters);

    void Upload(
        GameParameters const & gameParameters,
        Render::RenderContext & renderContext) const;

private:

    static inline auto ToSampleIndex(float x)
    {
        // Calculate sample index, minimizing error
        float const sampleIndexF = (x + GameParameters::HalfMaxWorldWidth) / Dx;
        auto const sampleIndexI = FastTruncateToArchInt(sampleIndexF + 0.5f);
        assert(sampleIndexI >= 0 && sampleIndexI <= SamplesCount);

        return sampleIndexI;
    }

public:

    /*
     * Assumption: x is in world boundaries.
     */
    float GetHeightAt(float x) const noexcept
    {
        assert(x >= -GameParameters::HalfMaxWorldWidth
            && x <= GameParameters::HalfMaxWorldWidth + 0.01f); // Allow for derivative taking

        //
        // Find sample index and interpolate in-between that sample and the next
        //

        // Fractional index in the sample array
        float const sampleIndexF = (x + GameParameters::HalfMaxWorldWidth) / Dx;

        // Integral part
        auto const sampleIndexI = FastTruncateToArchInt(sampleIndexF);

        // Fractional part within sample index and the next sample index
        float const sampleIndexDx = sampleIndexF - sampleIndexI;

        assert(sampleIndexI >= 0 && sampleIndexI <= SamplesCount);
        assert(sampleIndexDx >= 0.0f && sampleIndexDx <= 1.0f);

        return mSamples[sampleIndexI].SampleValue
            + mSamples[sampleIndexI].SampleValuePlusOneMinusSampleValue * sampleIndexDx;
    }

    void AdjustTo(
        std::optional<vec2f> const & worldCoordinates,
        float currentSimulationTime);

    inline void DisplaceAt(
        float const x,
        float const yOffset)
    {
        assert(x >= -GameParameters::HalfMaxWorldWidth
            && x <= GameParameters::HalfMaxWorldWidth + 0.01f); // Allow for derivative taking

        //
        // Find sample index and interpolate in-between that sample and the next
        //

        // Fractional index in the sample array
        float const sampleIndexF = (x + GameParameters::HalfMaxWorldWidth) / Dx;

        // Integral part
        auto const sampleIndexI = FastTruncateToArchInt(sampleIndexF);

        // Fractional part within sample index and the next sample index
        float const sampleIndexDx = sampleIndexF - sampleIndexI;

        assert(sampleIndexI >= 0 && sampleIndexI <= SamplesCount);
        assert(sampleIndexDx >= 0.0f && sampleIndexDx <= 1.0f);

        // Distribute among the two samples
        mHeightField[SWEOuterLayerSamples + sampleIndexI] += (1.0f - sampleIndexDx) * yOffset / SWEHeightFieldAmplification;
        mHeightField[SWEOuterLayerSamples + sampleIndexI + 1] += sampleIndexDx * yOffset / SWEHeightFieldAmplification;
    }

    void ApplyThanosSnap(
        float leftFrontX,
        float rightFrontX);

    void TriggerTsunami(float currentSimulationTime);

    void TriggerRogueWave(
        float currentSimulationTime,
        Wind const & wind);

public:

    // The number of samples for the entire world width;
    // a higher value means more resolution at the expense of Update() and of cache misses
    static size_t constexpr SamplesCount = 8192;

    // The x step of the samples
    static float constexpr Dx = GameParameters::MaxWorldWidth / static_cast<float>(SamplesCount);

private:

    void SetSWEWaveHeight(
        size_t centerIndex,
        float height);

    void RecalculateCoefficients(
        Wind const & wind,
        GameParameters const & gameParameters);

    template<typename TDuration>
    static GameWallClock::time_point CalculateNextAbnormalWaveTimestamp(
        GameWallClock::time_point lastTimestamp,
        TDuration rate);

    void ApplyDampingBoundaryConditions();

    void UpdateFields();

    void GenerateSamples(
        float currentSimulationTime,
        Wind const & wind,
        GameParameters const & gameParameters);

private:

    std::shared_ptr<GameEventDispatcher> mGameEventHandler;

    // What we store for each sample
    struct Sample
    {
        float SampleValue; // Value of this sample
        float SampleValuePlusOneMinusSampleValue; // Delta between next sample and this sample
    };

    // The samples (plus 1 to account for x==MaxWorldWidth)
    std::unique_ptr<Sample[]> mSamples;

    // Smoothing of wind incisiveness
    RunningAverage<15> mWindIncisivenessRunningAverage;

    //
    // SWE Layer constants
    //

    // The rest height of the height field - indirectly determines velocity
    // of waves (via dv/dt <= dh/dx, with dh/dt <= h*dv/dx).
    // Sensitive to Dx - With Dx=1.22, a good offset is 100; with dx=0.61, a good offset is 50
    static float constexpr SWEHeightFieldOffset = 50.0f;

    // The factor by which we amplify the height field perturbations;
    // higher values allow for smaller height field variations with the same visual height,
    // and smaller height field variations allow for greater stability
    static float constexpr SWEHeightFieldAmplification = 50.0f;

    // The number of samples we raise with a state machine
    static size_t constexpr SWEWaveStateMachinePerturbedSamplesCount = 3;

    // The number of samples we set apart in the SWE buffers for wave generation at each end of a buffer
    static size_t constexpr SWEWaveGenerationSamples = 1;

    // The number of samples we set apart in the SWE buffers for boundary conditions at each end of a buffer
    static size_t constexpr SWEBoundaryConditionsSamples = 3;

    static size_t constexpr SWEOuterLayerSamples =
        SWEWaveGenerationSamples
        + SWEBoundaryConditionsSamples;

    // The total number of samples in the SWE buffers
    static size_t constexpr SWETotalSamples =
        SWEOuterLayerSamples
        + SamplesCount
        + SWEOuterLayerSamples;

    //
    // Calculated coefficients
    //

    // Calculated values
    float mBasalWaveAmplitude1;
    float mBasalWaveAmplitude2;
    float mBasalWaveNumber1;
    float mBasalWaveNumber2;
    float mBasalWaveAngularVelocity1;
    float mBasalWaveAngularVelocity2;
    PrecalculatedFunction<SamplesCount> mBasalWaveSin1;
    GameWallClock::time_point mNextTsunamiTimestamp;
    GameWallClock::time_point mNextRogueWaveTimestamp;

    // Parameters that the calculated values are current with
    float mWindBaseAndStormSpeedMagnitude;
    float mBasalWaveHeightAdjustment;
    float mBasalWaveLengthAdjustment;
    float mBasalWaveSpeedAdjustment;
    std::chrono::minutes mTsunamiRate;
    std::chrono::minutes mRogueWaveRate;


    //
    // Shallow water equations
    //

    // Height field
    // - Height values are at the center of the staggered grid cells
    std::unique_ptr<float[]> mHeightField;

    // Velocity field
    // - Velocity values are at the edges of the staggered grid cells
    std::unique_ptr<float[]> mVelocityField;

private:

    //
    // Interactive waves
    //

    class SWEInteractiveWaveStateMachine
    {
    public:

        SWEInteractiveWaveStateMachine(
            size_t centerIndex,
            float lowHeight,
            float highHeight,
            float currentSimulationTime);

        // Absolute coordinate, not sample coordinate
        auto GetCenterIndex() const
        {
            return mCenterIndex;
        }

        void Restart(
            float newTargetHeight,
            float currentSimulationTime);

        void Release(float currentSimulationTime);

        /*
         * Returns none when it may be retired.
         */
        std::optional<float> Update(
            float currentSimulationTime);

    private:

        float CalculateSmoothingDelay();

        enum class WavePhaseType
        {
            Rise,
            Fall
        };

        size_t const mCenterIndex;
        float const mLowHeight;
        float mCurrentPhaseStartHeight;
        float mCurrentPhaseTargetHeight;
        float mCurrentHeight;
        float mCurrentProgress; // Between 0 and 1, regardless of direction
        float mStartSimulationTime;
        WavePhaseType mCurrentWavePhase;
        float mCurrentSmoothingDelay;
    };

    std::optional<SWEInteractiveWaveStateMachine> mSWEInteractiveWaveStateMachine;


    //
    // Abnormal waves
    //

    class SWEAbnormalWaveStateMachine
    {
    public:

        SWEAbnormalWaveStateMachine(
            size_t centerIndex,
            float lowHeight,
            float highHeight,
            float riseDelay, // sec
            float fallDelay, // sec
            float currentSimulationTime);

        // Absolute coordinate, not sample coordinate
        auto GetCenterIndex() const
        {
            return mCenterIndex;
        }

        /*
         * Returns none when it may be retired.
         */
        std::optional<float> Update(
            float currentSimulationTime);

    private:

        enum class WavePhaseType
        {
            Rise,
            Fall
        };

        size_t const mCenterIndex;
        float const mLowHeight;
        float const mHighHeight;
        float const mFallDelay; // sec
        float mCurrentProgress; // Between 0 and 1, regardless of direction
        float mCurrentPhaseStartSimulationTime;
        float mCurrentPhaseDelay;
        WavePhaseType mCurrentWavePhase;
    };

    std::optional<SWEAbnormalWaveStateMachine> mSWETsunamiWaveStateMachine;
    std::optional<SWEAbnormalWaveStateMachine> mSWERogueWaveWaveStateMachine;

    GameWallClock::time_point mLastTsunamiTimestamp;
    GameWallClock::time_point mLastRogueWaveTimestamp;
};

}
