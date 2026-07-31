#include "o2/stdafx.h"
#include "TokenDelivery/CarSim.h"

namespace td
{
	void CarSim::Reset(const CarTuning& tuning, const Vec2F& cellPos, Dir dir)
	{
		mTuning = tuning;
		mPos = cellPos;
		mDir = dir;
		mSpeed = 0.0f;
		mCommand = TurnCommand::None;
		mLastTurnCell = Vec2I(-100000, -100000);
		mLateralOffset = 0.0f;
		mLateralVel = 0.0f;
		mBounceTimer = 0.0f;
		mDriftTimer = 0.0f;
		mDriftMagnitude = 0.0f;
		mVisualAngle = DirAngle(dir);
		mJustTurned = false;
	}

	float CarSim::DirAngle(Dir d)
	{
		switch (d)
		{
			case Dir::E: return 0.0f;
			case Dir::S: return 90.0f;
			case Dir::W: return 180.0f;
			default:     return 270.0f;
		}
	}

	void CarSim::ApplyTurn(Dir newDir)
	{
		if (newDir == Opposite(mDir))
		{
			// the lane side flips together with the car
			mLateralOffset = -mLateralOffset;
			mLateralVel = -mLateralVel;
		}
		else
		{
			// momentum keeps pushing along the old heading: an outward velocity impulse
			// that travels ~driftAmount cells and stays there (no spring back)
			Vec2I oldHeading = DirVec(mDir);
			Vec2I newLeft = DirVec(LeftOf(newDir));
			float side = (float)(oldHeading.x*newLeft.x + oldHeading.y*newLeft.y);
			mLateralVel = side*mTuning.driftAmount*mTuning.lateralDamping*(mSpeed/mTuning.maxSpeed);
			mLateralOffset = 0.0f; // the old lateral becomes along-track at the pivot
		}

		mDriftMagnitude = mSpeed/mTuning.maxSpeed;
		mDriftTimer = mTuning.driftTime;
		mSpeed *= 1.0f - mTuning.turnSpeedLoss;
		mDir = newDir;
		mJustTurned = true;
	}

	bool CarSim::ResolveCommand(int roadMask, Dir& out) const
	{
		Dir left = LeftOf(mDir), right = RightOf(mDir);
		switch (mCommand)
		{
			case TurnCommand::Left:
				if (roadMask & DirBit(left)) { out = left; return true; }
				return false;

			case TurnCommand::Right:
				if (roadMask & DirBit(right)) { out = right; return true; }
				return false;

			case TurnCommand::Auto:
				if (roadMask & DirBit(left)) { out = left; return true; }
				if (roadMask & DirBit(right)) { out = right; return true; }
				return false;

			default:
				return false;
		}
	}

	void CarSim::TryRetroTurn(const CityModel& city)
	{
		// forgiving window: the command lands even shortly past the intersection center —
		// the car pivots back at the center and carries the traveled bit onto the new axis
		Vec2I cell = GetCell();
		if (cell == mLastTurnCell)
			return;

		bool alongX = mDir == Dir::E || mDir == Dir::W;
		float sign = (mDir == Dir::E || mDir == Dir::S) ? 1.0f : -1.0f;
		float ahead = (alongX ? mPos.x - (float)cell.x : mPos.y - (float)cell.y)*sign;
		if (ahead <= 0.0f || ahead > mTuning.turnWindow)
			return;

		Dir desired;
		if (!ResolveCommand(city.RoadMask(cell), desired))
			return;

		mPos = Vec2F((float)cell.x, (float)cell.y);
		ApplyTurn(desired);
		mLastTurnCell = cell;
		Vec2I v = DirVec(mDir);
		mPos += Vec2F((float)v.x, (float)v.y)*ahead;
	}

	bool CarSim::TryTurnAt(const Vec2I& cell, const CityModel& city)
	{
		int mask = city.RoadMask(cell);

		if (mCommand != TurnCommand::None && cell != mLastTurnCell)
		{
			Dir desired;
			if (ResolveCommand(mask, desired))
			{
				ApplyTurn(desired);
				mLastTurnCell = cell;
				return true;
			}
		}

		if ((mask & DirBit(mDir)) != 0)
			return false;

		// road ends ahead: forced turn, preferring right, then left, then back
		for (Dir candidate : { RightOf(mDir), LeftOf(mDir), Opposite(mDir) })
		{
			if ((mask & DirBit(candidate)) != 0)
			{
				ApplyTurn(candidate);
				mLastTurnCell = cell;
				return true;
			}
		}

		mSpeed = 0.0f;
		return false;
	}

	void CarSim::MoveAlong(float distance, const CityModel& city)
	{
		const float eps = 0.0001f;
		int guard = 0;
		while (distance > eps && guard++ < 64)
		{
			bool alongX = mDir == Dir::E || mDir == Dir::W;
			float coord = alongX ? mPos.x : mPos.y;
			float sign = (mDir == Dir::E || mDir == Dir::S) ? 1.0f : -1.0f;

			float nextCenter = sign > 0.0f ? Math::Floor(coord + eps) + 1.0f
										   : Math::Ceil(coord - eps) - 1.0f;
			float distToNext = (nextCenter - coord)*sign;

			if (distance < distToNext - eps)
			{
				coord += sign*distance;
				(alongX ? mPos.x : mPos.y) = coord;
				return;
			}

			(alongX ? mPos.x : mPos.y) = nextCenter;
			distance -= distToNext;
			TryTurnAt(GetCell(), city);
			if (mSpeed <= 0.0f)
				return;
		}
	}

	void CarSim::Tick(float dt, const CarInput& input, const CityModel& city)
	{
		mJustTurned = false;

		mCommand = input.turnLeft ? TurnCommand::Left
				 : input.turnRight ? TurnCommand::Right
				 : input.turnAuto ? TurnCommand::Auto
				 : TurnCommand::None;

		if (mCommand != TurnCommand::None)
			TryRetroTurn(city);

		float target = input.fuelEmpty ? 0.0f : mTuning.maxSpeed;
		if (mSpeed < target)
			mSpeed = Math::Min(target, mSpeed + mTuning.accel*dt);
		else
			mSpeed = Math::Max(target, mSpeed - mTuning.brakeDecel*dt);

		if (mSpeed > 0.0f)
			MoveAlong(mSpeed*dt, city);

		mDriftTimer = Math::Max(0.0f, mDriftTimer - dt);

		// free sideways steering across the road width: the held key accelerates the car
		// aside; the offset stays where it ends up (no centering), the road edge bounces
		// the car back and briefly ignores the key so the rebound reads as a failed move
		mBounceTimer = Math::Max(0.0f, mBounceTimer - dt);
		float keyDir = (input.turnLeft ? 1.0f : 0.0f) - (input.turnRight ? 1.0f : 0.0f);
		if (mBounceTimer > 0.0f)
			keyDir = 0.0f;
		mLateralVel += keyDir*mTuning.lateralAccel*dt;
		mLateralVel -= mLateralVel*Math::Min(1.0f, mTuning.lateralDamping*dt);
		mLateralOffset += mLateralVel*dt;
		if (mLateralOffset > mTuning.maxLateral)
		{
			mLateralOffset = mTuning.maxLateral;
			if (mLateralVel > 0.0f)
			{
				mLateralVel = -mLateralVel*mTuning.edgeBounce;
				mBounceTimer = mTuning.edgeBounceCooldown;
			}
		}
		else if (mLateralOffset < -mTuning.maxLateral)
		{
			mLateralOffset = -mTuning.maxLateral;
			if (mLateralVel < 0.0f)
			{
				mLateralVel = -mLateralVel*mTuning.edgeBounce;
				mBounceTimer = mTuning.edgeBounceCooldown;
			}
		}

		float targetAngle = DirAngle(mDir);
		float delta = targetAngle - mVisualAngle;
		while (delta > 180.0f) delta -= 360.0f;
		while (delta < -180.0f) delta += 360.0f;
		mVisualAngle += delta*Math::Min(1.0f, mTuning.angleSpeed*dt);
	}

	float DriftCurve(float elapsedFraction)
	{
		// 0 at the turn moment, quick rise, smooth fade — a short sideways skid
		return Math::Sin(Math::PI()*Math::Pow(elapsedFraction, 0.75f));
	}

	Vec2F CarSim::GetVisualPos() const
	{
		Vec2I left = DirVec(LeftOf(mDir));
		return mPos + Vec2F((float)left.x, (float)left.y)*mLateralOffset;
	}

	float CarSim::GetDriftIntensity() const
	{
		if (mDriftTimer <= 0.0f)
			return 0.0f;

		float u = 1.0f - mDriftTimer/mTuning.driftTime;
		return DriftCurve(u)*Math::Clamp01(mDriftMagnitude);
	}
}
