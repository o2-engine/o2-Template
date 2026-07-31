#pragma once

#include "TokenDelivery/CityModel.h"

namespace td
{
	// ---------------------------------------------------------
	// Car handling tuning: speed, turns, lateral steering, drift
	// ---------------------------------------------------------
	struct CarTuning
	{
		float maxSpeed = 3.0f;   // Cells per second
		float accel = 2.5f;      // Cells per second^2
		float brakeDecel = 2.5f; // Used when the speed target drops (fuel out)

		float turnSpeedLoss = 0.18f; // Fraction of speed lost on a turn
		float driftAmount = 0.16f;   // Outward shift after a turn, cells; stays, no spring back
		float maxLateral = 0.28f;    // Road bounds for the lateral shift
		float turnWindow = 0.45f;    // Cells past an intersection center where a turn still lands

		float lateralAccel = 8.0f;   // Sideways acceleration while a turn key is held, cells/s^2
		float lateralDamping = 6.0f; // Sideways velocity decay, 1/s

		float edgeBounce = 0.45f;         // Velocity kept (reversed) when hitting the road edge
		float edgeBounceCooldown = 0.25f; // Seconds the held key is ignored after the hit,
										  // letting the rebound play out visibly

		float driftTime = 0.4f;  // Smoke burst duration after a turn
		float angleSpeed = 9.0f; // Visual heading exponential lerp rate, 1/sec
	};

	// ------------------------------------
	// Car turn input of one simulation tick
	// ------------------------------------
	struct CarInput
	{
		bool turnLeft = false;  // Held: turn left of the heading at an intersection
		bool turnRight = false; // Held: turn right of the heading
		bool turnAuto = false;  // Held: turn to the open side, left preferred
		bool fuelEmpty = false; // Drops the speed target to zero
	};

	// ---------------------------------------------------------------------------------
	// Rail-bound arcade car: always drives forward along road center lines. Turns are
	// relative to the heading and land at intersection centers (with a forgiving window
	// shortly past the center); dead ends turn the car automatically, right side first.
	// The turn keys also steer the car sideways across the road width — where no turn is
	// possible the car visibly lunges aside and bounces off the road edge. Pure logic,
	// no scene dependencies.
	// ---------------------------------------------------------------------------------
	class CarSim
	{
	public:
		// Places the car on the road cell heading the given way
		void Reset(const CarTuning& tuning, const Vec2F& cellPos, Dir dir);

		// Advances the car: turn commands, speed, movement and the lateral steering
		void Tick(float dt, const CarInput& input, const CityModel& city);

		// Returns the center-line position, cells
		const Vec2F& GetPos() const { return mPos; }

		// Returns the heading
		Dir GetDir() const { return mDir; }

		// Returns the speed, cells per second
		float GetSpeed() const { return mSpeed; }

		// Returns is the car stopped
		bool IsStopped() const { return mSpeed < 0.001f; }

		// Returns has the car turned this tick
		bool JustTurned() const { return mJustTurned; }

		// Returns the visual position: center line plus the persistent lateral shift
		Vec2F GetVisualPos() const;

		// Returns the smoothed visual heading in degrees, 0 = E, 90 = S
		float GetVisualAngle() const { return mVisualAngle; }

		// Returns the drift smoke intensity 0..1
		float GetDriftIntensity() const;

		// Returns the occupied cell
		Vec2I GetCell() const
		{
			return Vec2I(Math::RoundToInt(mPos.x), Math::RoundToInt(mPos.y));
		}

	private:
		enum class TurnCommand { None, Left, Right, Auto };

	private:
		CarTuning mTuning; // Handling tuning

		Vec2F mPos;            // Center-line position, cells
		Dir   mDir = Dir::E;   // Heading
		float mSpeed = 0.0f;   // Speed, cells per second

		TurnCommand mCommand = TurnCommand::None;               // Turn command of the tick
		Vec2I       mLastTurnCell = Vec2I(-100000, -100000);    // One command turn per cell

		float mLateralOffset = 0.0f; // Current shift, + = left of heading, cells
		float mLateralVel = 0.0f;    // Sideways velocity, cells/s
		float mBounceTimer = 0.0f;   // Edge-hit cooldown, see edgeBounceCooldown

		float mDriftTimer = 0.0f;     // Smoke burst time left
		float mDriftMagnitude = 0.0f; // Smoke burst strength
		float mVisualAngle = 0.0f;    // Smoothed heading, degrees
		bool  mJustTurned = false;    // The car turned this tick

	private:
		// Returns the heading angle in degrees
		static float DirAngle(Dir d);

		// Applies the turn: heading, speed loss, drift shift
		void ApplyTurn(Dir newDir);

		// Resolves the current command against the open roads; false when no turn fits
		bool ResolveCommand(int roadMask, Dir& out) const;

		// Lands a turn shortly past the passed intersection center (forgiving window)
		void TryRetroTurn(const CityModel& city);

		// Turns at the cell when the command matches an open road
		bool TryTurnAt(const Vec2I& cell, const CityModel& city);

		// Moves the car along the roads cell by cell, turning at dead ends
		void MoveAlong(float distance, const CityModel& city);
	};
}
