#pragma once

#include "TokenDelivery/CarSim.h"
#include "TokenDelivery/CityModel.h"

namespace td
{
	struct SessionTuning
	{
		CarTuning car;
		float fuelTime = 60.0f;       // seconds of fuel, constant drain
		float boostReserve = 5.0f;    // seconds of boost hold
		float fillRate = 60.0f;       // tokens per second near the source
		float fillRadius = 1.6f;      // cells from the fountain-adjacent road cells
		int   baseOrders = 3;
		int   maxOrders = 8;
		int   baseCitySize = 15;
		int   maxCitySize = 19;
	};

	enum class SessionState { Playing, Won, Lost };

	struct GameInput
	{
		bool up = false, down = false, left = false, right = false;
		bool boost = false;
	};

	// Full game round: generated city + player car + tokens + orders + fuel/boost economy.
	// Pure logic, drives the view layer; fully testable headless.
	class GameSession
	{
	public:
		void Start(int level, UInt32 seed, const SessionTuning& tuning = SessionTuning());
		void StartWithCity(const CityModel& city, const SessionTuning& tuning = SessionTuning());
		void Tick(float dt, const GameInput& input);

		const CityModel& GetCity() const { return mCity; }
		const CarSim& GetCar() const { return mCar; }
		const SessionTuning& GetTuning() const { return mTuning; }

		SessionState GetState() const { return mState; }
		int   GetLevel() const { return mLevel; }
		int   GetTokens() const { return (int)mTokens; }
		float GetFuel() const { return mFuel; }              // seconds left
		float GetFuelFraction() const { return mFuel/mTuning.fuelTime; }
		float GetBoostLeft() const { return mBoostLeft; }
		float GetBoostFraction() const { return mBoostLeft/mTuning.boostReserve; }
		bool  IsBoosting() const { return mBoosting; }
		bool  IsFilling() const { return mFilling; }

		void  DebugSetFuel(float seconds) { mFuel = seconds; } // test hook

		bool  IsOrderCompleted(int index) const { return mCompleted[index]; }
		int   GetCompletedCount() const;
		int   GetOrderCompletedThisTick() const { return mCompletedThisTick; } // -1 if none

		static CityGenParams ParamsForLevel(int level, const SessionTuning& tuning);

	private:
		SessionTuning mTuning;
		CityModel     mCity;
		CarSim        mCar;
		SessionState  mState = SessionState::Playing;
		int           mLevel = 1;
		float         mFuel = 0.0f;
		float         mBoostLeft = 0.0f;
		float         mTokens = 0.0f;
		bool          mBoosting = false;
		bool          mFilling = false;
		Vector<bool>  mCompleted;
		int           mCompletedThisTick = -1;
	};
}
// --- META ---

PRE_ENUM_META(td::SessionState);
// --- END META ---
