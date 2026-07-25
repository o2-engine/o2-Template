#pragma once

#include "o2/Assets/Types/VectorFontAsset.h"

namespace td
{
	// Registers the game widget styles in o2UI: labels, buttons, progress bars.
	// Safe to call repeatedly (styles are replaced).
	void BuildGameUIStyles();

	// The game UI font (Baloo 2 ExtraBold with fallbacks). Widget style cloning resets
	// Text drawables to the engine default font, so widgets reapply it explicitly.
	o2::AssetRef<o2::VectorFontAsset> GameUIFont();
}
