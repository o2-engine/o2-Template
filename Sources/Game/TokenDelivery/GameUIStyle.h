#pragma once

#include "o2/Assets/Types/VectorFontAsset.h"

namespace o2
{
	class Label;
	class Widget;
}

namespace td
{
	// Registers the game widget styles in o2UI: labels, buttons, progress bars.
	// Safe to call repeatedly (styles are replaced).
	void BuildGameUIStyles();

	// The game UI font (Baloo 2 ExtraBold with fallbacks). Widget style cloning resets
	// Text drawables to the engine default font, so widgets reapply it explicitly.
	o2::AssetRef<o2::VectorFontAsset> GameUIFont();

	// Styled label added to the parent, with the font, color, aligns and font style that
	// widget style cloning drops reapplied
	o2::Ref<o2::Label> MakeGameLabel(const o2::Ref<o2::Widget>& parent, const o2::WString& text,
									 int height, const o2::String& style = "standard");
}
