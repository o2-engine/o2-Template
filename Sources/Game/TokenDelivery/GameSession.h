#pragma once

#include "TokenDelivery/CarSim.h"
#include "TokenDelivery/CityModel.h"

namespace td
{
	// ------------------------------------------------------------------------------
	// Session tuning: car handling plus the token/fuel economy and the level scaling
	// ------------------------------------------------------------------------------
	struct SessionTuning
	{
		CarTuning car; // Car handling tuning

		float fuelTime = 60.0f;  // Seconds of fuel, constant drain
		float fillRate = 60.0f;  // Tokens per second near the source
		float fillRadius = 1.6f; // Cells from the fountain-adjacent road cells

		int baseOrders = 3;    // Orders on the first level
		int maxOrders = 8;     // Orders cap for the late levels
		int baseCitySize = 15; // City size on the first level
		int maxCitySize = 19;  // City size cap for the late levels
	};

	enum class SessionState { Playing, Won, Lost };

	// -------------------------------------
	// Player input of one simulation tick
	// -------------------------------------
	struct GameInput
	{
		bool turnLeft = false;  // Turn left at the next intersection
		bool turnRight = false; // Turn right at the next intersection
		bool turnAuto = false;  // Turn to any open side
	};

	// ---------------------------------------------------------------------------------
	// Full game round: generated city + player car + tokens + orders + fuel economy.
	// Pure logic, drives the view layer; fully testable headless.
	// ---------------------------------------------------------------------------------
	class GameSession
	{
	public:
		// Generates the level city from the seed and resets the round
		void Start(int level, UInt32 seed, const SessionTuning& tuning = SessionTuning());

		// Resets the round over an externally built city (tests)
		void StartWithCity(const CityModel& city, const SessionTuning& tuning = SessionTuning());

		// Advances the round: fuel, car, token filling, deliveries and the end states
		void Tick(float dt, const GameInput& input);

		// Returns the generated city
		const CityModel& GetCity() const { return mCity; }

		// Returns the player car simulation
		const CarSim& GetCar() const { return mCar; }

		// Returns the running tuning
		const SessionTuning& GetTuning() const { return mTuning; }

		// Returns the round state
		SessionState GetState() const { return mState; }

		// Returns the level number
		int GetLevel() const { return mLevel; }

		// Returns the collected tokens
		int GetTokens() const { return (int)mTokens; }

		// Returns the fuel left, seconds
		float GetFuel() const { return mFuel; }

		// Returns the fuel left as a 0..1 fraction of the full tank
		float GetFuelFraction() const { return mFuel/mTuning.fuelTime; }

		// Returns is the car inside the token source radius
		bool IsFilling() const { return mFilling; }

		// Enables or disables the fuel drain; off while the tutorial runs
		void SetFuelDrain(bool enabled) { mFuelDrain = enabled; }

		// Sets the fuel directly (tests)
		void DebugSetFuel(float seconds) { mFuel = seconds; }

		// Completes the order without driving to it (tests)
		void DebugCompleteOrder(int index);

		// Returns is the order delivered
		bool IsOrderCompleted(int index) const { return mCompleted[index]; }

		// Returns the delivered orders count
		int GetCompletedCount() const;

		// Returns the just-delivered order index, -1 if none; reading takes the event,
		// so it can't repeat once the session stops ticking
		int ConsumeCompletedOrder();

		// Returns the city generation params of the level under the tuning curve
		static CityGenParams ParamsForLevel(int level, const SessionTuning& tuning);

	private:
		SessionTuning mTuning; // Running tuning
		CityModel     mCity;   // Generated level city
		CarSim        mCar;    // Player car simulation

		SessionState mState = SessionState::Playing; // Round state

		int   mLevel = 1;        // Level number
		float mFuel = 0.0f;      // Fuel left, seconds
		float mTokens = 0.0f;    // Collected tokens, fractional while filling
		bool  mFilling = false;  // Is the car inside the source radius
		bool  mFuelDrain = true; // Is the fuel draining; off while the tutorial runs

		Vector<bool> mCompleted;                // Delivered flag per order
		int          mPendingCompletedOrder = -1; // One-shot just-delivered event, -1 = none
	};
}
// --- META ---

PRE_ENUM_META(td::SessionState);
// --- END META ---
