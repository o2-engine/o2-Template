#pragma once

namespace td
{
	// Creates the game root actor with GameControllerComponent; everything else (layers,
	// cameras, city, HUD) is built by the controller on start
	void LaunchTokenDelivery();
}
