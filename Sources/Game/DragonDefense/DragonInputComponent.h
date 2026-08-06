#pragma once

#include "o2/Scene/Component.h"

using namespace o2;

// Bridges engine input into game scripts: clicks and cursor position in world
// space of the fitted UI camera. Scripts poll it every update.
class DragonInputComponent: public Component
{
public:
	// Returns true if the primary cursor was pressed during this frame @SCRIPTABLE
	bool IsClicked() const;

	// Returns true if the right mouse button was pressed during this frame @SCRIPTABLE
	bool IsRightClicked() const;

	// Returns cursor position in world coordinates of the fitted camera @SCRIPTABLE
	Vec2F GetCursorWorld() const;

	// Converts a screen space point (window center origin, y up) to world space
	static Vec2F ScreenToWorld(const Vec2F& screen);

	// Converts a world space point to screen space; used by UI tests to aim clicks
	static Vec2F WorldToScreen(const Vec2F& world);

	SERIALIZABLE(DragonInputComponent);
	CLONEABLE_REF(DragonInputComponent);

private:
	static float WorldPerScreenPixel();

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(DragonInputComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(DragonInputComponent)
{
}
END_META;
CLASS_METHODS_META(DragonInputComponent)
{

    FUNCTION().PUBLIC().SCRIPTABLE_ATTRIBUTE().SIGNATURE(bool, IsClicked);
    FUNCTION().PUBLIC().SCRIPTABLE_ATTRIBUTE().SIGNATURE(bool, IsRightClicked);
    FUNCTION().PUBLIC().SCRIPTABLE_ATTRIBUTE().SIGNATURE(Vec2F, GetCursorWorld);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Vec2F, ScreenToWorld, const Vec2F&);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Vec2F, WorldToScreen, const Vec2F&);
    FUNCTION().PRIVATE().SIGNATURE_STATIC(float, WorldPerScreenPixel);
}
END_META;
// --- END META ---
