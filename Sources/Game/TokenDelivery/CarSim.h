#pragma once

#include "TokenDelivery/CityModel.h"

namespace td
{
	struct CarTuning
	{
		float maxSpeed = 3.0f;        // cells per second
		float accel = 2.5f;           // cells per second^2
		float brakeDecel = 2.5f;      // used when the speed target drops (boost end, fuel out)
		float turnSpeedLoss = 0.18f;  // fraction of speed lost on a turn
		float boostFactor = 1.5f;     // speed cap multiplier while boosting
		float driftTime = 0.4f;       // seconds of the sideways slide after a turn
		float driftAmount = 0.4f;     // max sideways offset in cells at full speed
		float angleSpeed = 9.0f;      // visual heading exponential lerp rate, 1/sec
	};

	struct CarInput
	{
		bool  hasDesired = false;
		Dir   desired = Dir::N;
		bool  boost = false;
		bool  fuelEmpty = false;
	};

	// Rail-bound arcade car: always drives forward along road center lines, turns at cell
	// centers where the road allows, U-turns anywhere. Pure logic, no scene dependencies.
	class CarSim
	{
	public:
		void Reset(const CarTuning& tuning, const Vec2F& cellPos, Dir dir);
		void Tick(float dt, const CarInput& input, const CityModel& city);

		const Vec2F& GetPos() const { return mPos; }
		Dir   GetDir() const { return mDir; }
		float GetSpeed() const { return mSpeed; }
		bool  IsStopped() const { return mSpeed < 0.001f; }
		bool  JustTurned() const { return mJustTurned; }

		// visual state: logical position plus the decaying drift offset, smoothed heading
		Vec2F GetVisualPos() const;
		float GetVisualAngle() const { return mVisualAngle; } // degrees, 0 = E, CCW in cell space
		float GetDriftIntensity() const;                      // 0..1, for smoke particles

		Vec2I GetCell() const
		{
			return Vec2I(Math::RoundToInt(mPos.x), Math::RoundToInt(mPos.y));
		}

	private:
		CarTuning mTuning;
		Vec2F     mPos;
		Dir       mDir = Dir::E;
		float     mSpeed = 0.0f;

		bool      mHasBufferedTurn = false;
		Dir       mBufferedTurn = Dir::N;

		Vec2F     mDriftDir;
		float     mDriftTimer = 0.0f;
		float     mDriftMagnitude = 0.0f;
		float     mVisualAngle = 0.0f;
		bool      mJustTurned = false;

		static float DirAngle(Dir d);
		void ApplyTurn(Dir newDir);
		bool TryTurnAt(const Vec2I& cell, const CityModel& city);
		void MoveAlong(float distance, const CityModel& city);
	};
}
