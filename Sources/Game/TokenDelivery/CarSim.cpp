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
		mHasBufferedTurn = false;
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
		mDriftDir = Vec2F((float)DirVec(mDir).x, (float)DirVec(mDir).y);
		mDriftMagnitude = mTuning.driftAmount*(mSpeed/mTuning.maxSpeed);
		mDriftTimer = mTuning.driftTime;
		mSpeed *= 1.0f - mTuning.turnSpeedLoss;
		mDir = newDir;
		mJustTurned = true;
	}

	bool CarSim::TryTurnAt(const Vec2I& cell, const CityModel& city)
	{
		int mask = city.RoadMask(cell);

		if (mHasBufferedTurn && (mask & DirBit(mBufferedTurn)) != 0)
		{
			ApplyTurn(mBufferedTurn);
			mHasBufferedTurn = false;
			return true;
		}

		if ((mask & DirBit(mDir)) != 0)
			return false;

		// road ends ahead: forced turn, preferring right, then left, then back
		for (Dir candidate : { RightOf(mDir), LeftOf(mDir), Opposite(mDir) })
		{
			if ((mask & DirBit(candidate)) != 0)
			{
				ApplyTurn(candidate);
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

		if (input.hasDesired)
		{
			if (input.desired == mDir)
				mHasBufferedTurn = false;
			else if (input.desired == Opposite(mDir))
			{
				ApplyTurn(input.desired); // U-turn is allowed anywhere on the road
				mHasBufferedTurn = false;
			}
			else
			{
				mHasBufferedTurn = true;
				mBufferedTurn = input.desired;
			}
		}

		float target = input.fuelEmpty ? 0.0f
					 : mTuning.maxSpeed*(input.boost ? mTuning.boostFactor : 1.0f);
		if (mSpeed < target)
			mSpeed = Math::Min(target, mSpeed + mTuning.accel*dt);
		else
			mSpeed = Math::Max(target, mSpeed - mTuning.brakeDecel*dt);

		if (mSpeed > 0.0f)
			MoveAlong(mSpeed*dt, city);

		mDriftTimer = Math::Max(0.0f, mDriftTimer - dt);

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
		if (mDriftTimer <= 0.0f || mTuning.driftTime <= 0.0f)
			return mPos;

		float u = 1.0f - mDriftTimer/mTuning.driftTime;
		return mPos + mDriftDir*(mDriftMagnitude*DriftCurve(u));
	}

	float CarSim::GetDriftIntensity() const
	{
		if (mDriftTimer <= 0.0f)
			return 0.0f;

		float u = 1.0f - mDriftTimer/mTuning.driftTime;
		return DriftCurve(u)*Math::Clamp01(mDriftMagnitude/Math::Max(0.001f, mTuning.driftAmount));
	}
}
