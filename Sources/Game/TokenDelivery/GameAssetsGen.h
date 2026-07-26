#pragma once

using namespace o2;

namespace td
{
	// Builds the shipped game assets by code, as a separate pass: entity prototypes
	// (player car, traffic cars, buildings, hologram, sparks) and the bootstrap scene
	// with cameras and the game actor. The runtime only loads the results; run the
	// GameAssetsGen tool after changing this construction code.
	void GenerateGameAssets();
}
