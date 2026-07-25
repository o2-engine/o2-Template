#include "o2/stdafx.h"
#include "TokenDelivery/GameSession.h"

namespace td
{
	CityGenParams GameSession::ParamsForLevel(int level, const SessionTuning& tuning)
	{
		CityGenParams params;
		params.ordersCount = Math::Min(tuning.baseOrders + (level - 1), tuning.maxOrders);
		params.size = Math::Min(tuning.baseCitySize + (level - 1)/2*2, tuning.maxCitySize);
		params.trafficCars = Math::Min(5 + level, 12);
		return params;
	}

	void GameSession::Start(int level, UInt32 seed, const SessionTuning& tuning)
	{
		mLevel = Math::Max(1, level);
		StartWithCity(GenerateCity(ParamsForLevel(mLevel, tuning), seed), tuning);
	}

	void GameSession::StartWithCity(const CityModel& city, const SessionTuning& tuning)
	{
		mTuning = tuning;
		mCity = city;
		mCar.Reset(tuning.car, Vec2F((float)mCity.playerStart.x, (float)mCity.playerStart.y),
				   mCity.playerStartDir);
		mState = SessionState::Playing;
		mFuel = tuning.fuelTime;
		mTokens = 0.0f;
		mFilling = false;
		mCompleted.Clear();
		mCompleted.resize(mCity.orders.Count(), false);
		mCompletedThisTick = -1;
	}

	int GameSession::GetCompletedCount() const
	{
		int count = 0;
		for (bool completed : mCompleted)
			count += completed ? 1 : 0;
		return count;
	}

	void GameSession::Tick(float dt, const GameInput& input)
	{
		mCompletedThisTick = -1;
		if (mState != SessionState::Playing)
			return;

		mFuel = Math::Max(0.0f, mFuel - dt);

		CarInput carInput;
		carInput.turnLeft = input.turnLeft;
		carInput.turnRight = input.turnRight;
		carInput.turnAuto = input.turnAuto;
		carInput.fuelEmpty = mFuel <= 0.0f;

		mCar.Tick(dt, carInput, mCity);

		// token refill near the fountain source cells
		mFilling = false;
		for (auto& sourceCell : mCity.sourceCells)
		{
			Vec2F cellPos((float)sourceCell.x, (float)sourceCell.y);
			if ((mCar.GetPos() - cellPos).Length() < mTuning.fillRadius)
			{
				mFilling = true;
				mTokens += mTuning.fillRate*dt;
				break;
			}
		}

		// order delivery: driving through a road cell adjacent to an office completes the
		// order when the car carries enough tokens; no need to stop
		Vec2I carCell = mCar.GetCell();
		for (int i = 0; i < mCity.orders.Count(); i++)
		{
			if (mCompleted[i] || !mCity.orders[i].deliveryCells.Contains(carCell))
				continue;
			if ((int)mTokens < mCity.orders[i].amount)
				continue;

			mTokens -= (float)mCity.orders[i].amount;
			mCompleted[i] = true;
			mCompletedThisTick = i;
		}

		if (!mCity.orders.IsEmpty() && GetCompletedCount() == mCity.orders.Count())
		{
			mState = SessionState::Won;
			return;
		}

		if (mFuel <= 0.0f && mCar.IsStopped())
			mState = SessionState::Lost;
	}
}
// --- META ---

ENUM_META(td::SessionState, td__SessionState)
{
    ENUM_ENTRY(Lost);
    ENUM_ENTRY(Playing);
    ENUM_ENTRY(Won);
}
END_ENUM_META;
// --- END META ---
