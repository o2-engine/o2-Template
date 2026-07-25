#pragma once

#include "TokenDelivery/CityModel.h"

namespace td
{
	struct CarTuning
	{
		float maxSpeed = 3.0f;        // cells per second
		float accel = 2.5f;           // cells per second^2
		float brakeDecel = 2.5f;      // used when the speed target drops (fuel out)
		float turnSpeedLoss = 0.18f;  // fraction of speed lost on a turn
		float driftAmount = 0.16f;    // outward shift after a turn, cells; stays, no spring back
		float maxLateral = 0.28f;     // road bounds for the lateral shift
		float turnWindow = 0.45f;     // cells past an intersection center where a turn still lands
		float lateralAccel = 8.0f;    // sideways acceleration while a turn key is held, cells/s^2
		float lateralDamping = 6.0f;  // sideways velocity decay, 1/s
		float edgeBounce = 0.45f;     // velocity kept (reversed) when hitting the road edge
		float edgeBounceCooldown = 0.25f; // seconds the held key is ignored after the hit,
										  // letting the rebound play out visibly
		float driftTime = 0.4f;       // smoke burst duration after a turn
		float angleSpeed = 9.0f;      // visual heading exponential lerp rate, 1/sec
	};

	struct CarInput
	{
		bool  turnLeft = false;  // held: turn left of the heading at an intersection
		bool  turnRight = false; // held: turn right of the heading
		bool  turnAuto = false;  // held: turn to the open side, left preferred
		bool  fuelEmpty = false;
	};

	// Rail-bound arcade car: always drives forward along road center lines. Turns are
	// relative to the heading and land at intersection centers (with a forgiving window
	// shortly past the center); dead ends turn the car automatically, right side first.
	// The turn keys also steer the car sideways across the road width — where no turn is
	// possible the car visibly lunges aside and bounces off the road edge. Pure logic,
	// no scene dependencies.
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

		// visual state: center-line position plus the persistent lateral shift, smoothed heading
		Vec2F GetVisualPos() const;
		float GetVisualAngle() const { return mVisualAngle; } // degrees, 0 = E, 90 = S
		float GetDriftIntensity() const;                      // 0..1, for smoke particles

		Vec2I GetCell() const
		{
			return Vec2I(Math::RoundToInt(mPos.x), Math::RoundToInt(mPos.y));
		}

	private:
		enum class TurnCommand { None, Left, Right, Auto };

		CarTuning mTuning;
		Vec2F     mPos;
		Dir       mDir = Dir::E;
		float     mSpeed = 0.0f;

		TurnCommand mCommand = TurnCommand::None;
		Vec2I       mLastTurnCell = Vec2I(-100000, -100000); // one command turn per cell

		float     mLateralOffset = 0.0f; // current shift, + = left of heading, cells
		float     mLateralVel = 0.0f;    // sideways velocity, cells/s
		float     mBounceTimer = 0.0f;   // edge-hit cooldown, see edgeBounceCooldown
		float     mDriftTimer = 0.0f;
		float     mDriftMagnitude = 0.0f;
		float     mVisualAngle = 0.0f;
		bool      mJustTurned = false;

		static float DirAngle(Dir d);
		void ApplyTurn(Dir newDir);
		bool ResolveCommand(int roadMask, Dir& out) const;
		void TryRetroTurn(const CityModel& city);
		bool TryTurnAt(const Vec2I& cell, const CityModel& city);
		void MoveAlong(float distance, const CityModel& city);
	};
}
