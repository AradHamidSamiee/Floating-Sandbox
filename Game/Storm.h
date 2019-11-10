/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2019-10-15
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "GameEventDispatcher.h"
#include "GameParameters.h"
#include "RenderContext.h"

#include <GameCore/GameWallClock.h>
#include <GameCore/Vectors.h>

#include <memory>
#include <list>
#include <vector>

namespace Physics
{

class Storm
{

public:

	Storm(
		World & parentWorld,
		std::shared_ptr<GameEventDispatcher> gameEventDispatcher);

    void Update(
		float currentSimulationTime,
		GameParameters const & gameParameters);

    void Upload(Render::RenderContext & renderContext) const;

public:

    struct Parameters
    {
        float WindSpeed; // Km/h, absolute (on top of current direction)
        unsigned int NumberOfClouds;
        float CloudsSize; // [0.0f = initial size, 1.0 = full size]
        float CloudDarkening; // [0.0f = full darkness, 1.0 = no darkening]
        float AmbientDarkening; // [0.0f = full darkness, 1.0 = no darkening]
		float RainDensity; // [0.0f = no rain, 1.0f = full rain]

        Parameters()
        {
            Reset();
        }

        void Reset()
        {
            WindSpeed = 0.0f;
            NumberOfClouds = 0;
            CloudsSize = 0.0f;
            CloudDarkening = 1.0f;
            AmbientDarkening = 1.0f;
			RainDensity = 0.0f;
        }
    };

    Parameters const & GetParameters() const
    {
        return mParameters;
    }

    void TriggerStorm();

	void TriggerLightning();

private:

    void TurnStormOn(GameWallClock::time_point now);
    void TurnStormOff();

	void DoTriggerBackgroundLightning(GameWallClock::time_point now);
	void DoTriggerForegroundLightning(
		GameWallClock::time_point now,
		vec2f const & targetWorldPosition);
	void UpdateLightnings(
		GameWallClock::time_point now,
		float currentSimulationTime,
		GameParameters const & gameParameters);
	void UploadLightnings(Render::RenderContext & renderContext) const;

private:

	World & mParentWorld;
	std::shared_ptr<GameEventDispatcher> mGameEventHandler;

	//
	// Lightning state machine
	//

	class LightningStateMachine
	{
	public:

		enum class LightningType
		{
			Background,
			Foreground
		};

		LightningType const Type;
		float const PersonalitySeed;
		GameWallClock::time_point const StartTimestamp;

		std::optional<float> const NdcX;
		std::optional<vec2f> const TargetWorldPosition;
		float Progress;
		float RenderProgress;
		bool HasNotifiedTouchdown;

		LightningStateMachine(
			LightningType type,
			float personalitySeed,
			GameWallClock::time_point startTimestamp,
			std::optional<float> ndcX,
			std::optional<vec2f> targetWorldPosition)
			: Type(type)
			, PersonalitySeed(personalitySeed)
			, StartTimestamp(startTimestamp)
			, NdcX(ndcX)
			, TargetWorldPosition(targetWorldPosition)
			, Progress(0.0f)
			, RenderProgress(0.0f)
			, HasNotifiedTouchdown(false)
		{}
	};

    //
    // Storm state machine
    //

	// The storm output
	Parameters mParameters;

    // Flag indicating whether we are in a storm or waiting for one
    bool mIsInStorm;

    // The current progress of the storm, when in a storm: [0.0, 1.0]
    float mCurrentStormProgress;

	// The timestamp at which we last did a storm update
	GameWallClock::time_point mLastStormUpdateTimestamp;

	// The CDF's for thunders
	float const mMinThunderCdf;
	float const mOneThunderCdf;
	float const mMaxThunderCdf;

	// The CDF's for lightnings
	float const mMinLightningCdf;
	float const mOneLightningCdf;
	float const mMaxLightningCdf;

	// The next timestamp at which to sample the Poisson distribution for deciding various things
	GameWallClock::time_point mNextThunderPoissonSampleTimestamp;
	GameWallClock::time_point mNextBackgroundLightningPoissonSampleTimestamp;
	GameWallClock::time_point mNextForegroundLightningPoissonSampleTimestamp;

	// The current lightnings' state machines
	std::list<LightningStateMachine> mLightnings;
};

}
