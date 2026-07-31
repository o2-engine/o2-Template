#pragma once

namespace td
{
	// Starts the game by loading the bootstrap scene (Assets/Bootstrap.scn): cameras and
	// the game actor with the controller, HUD and tutorial components come from the scene
	// data. Falls back to building the game actor by code when the scene asset is absent.
	void LaunchTokenDelivery();
}
