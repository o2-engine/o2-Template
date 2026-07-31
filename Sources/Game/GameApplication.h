#pragma once

#include "TokenDelivery/SplashScreen.h"
#include "o2/Application/Application.h"

using namespace o2;

// ---------------------------------------------------------------------------------
// Game application: shows the boot splash for a moment, then loads the game scene.
// ---------------------------------------------------------------------------------
class GameApplication: public Application
{
public:
	// Constructor with ref counter
	GameApplication(RefCounter* refCounter);

protected:
	td::SplashScreen mSplash;             // Boot splash, shown before the game loads
	bool             mGameStarted = false; // The splash is over and the game scene is loaded

protected:
	// Called when application is starting; shows the splash
	void OnStarted() override;

	// Called on updating; drives the splash and starts the game when it finishes
	void OnUpdate(float dt) override;

	// Called on drawing
	void OnDraw() override;
};
