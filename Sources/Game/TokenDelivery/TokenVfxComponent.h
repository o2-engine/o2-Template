#pragma once

#include "o2/Scene/Component.h"
#include "o2/Utils/Function/Function.h"

using namespace o2;

namespace td
{
	// -------------------------------------------------------------------------------------
	// Manual-draw host for the flying tokens: forwards the actor draw hook into the HUD.
	// The flyers are plain data owned by the HUD component — actors must not be spawned
	// mid scene update, so the tokens are not scene objects themselves.
	// -------------------------------------------------------------------------------------
	class TokenVfxComponent: public Component
	{
	public:
		Function<void()> onDraw; // Draw callback, set by the HUD

	public:
		// Default constructor
		TokenVfxComponent();

		// Constructor with ref counter
		explicit TokenVfxComponent(RefCounter* refCounter);

		SERIALIZABLE(TokenVfxComponent);
		CLONEABLE_REF(TokenVfxComponent);

	private:
		// Called when the actor draws; runs the HUD callback
		void OnDraw() override;

		REF_COUNTERABLE_IMPL(Component);
	};
}
// --- META ---

CLASS_BASES_META(td::TokenVfxComponent)
{
    BASE_CLASS(o2::Component);
}
END_META;
CLASS_FIELDS_META(td::TokenVfxComponent)
{
    FIELD().PUBLIC().NAME(onDraw);
}
END_META;
CLASS_METHODS_META(td::TokenVfxComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
    FUNCTION().PRIVATE().SIGNATURE(void, OnDraw);
}
END_META;
// --- END META ---
