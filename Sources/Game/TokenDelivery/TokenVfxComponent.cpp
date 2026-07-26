#include "o2/stdafx.h"
#include "TokenDelivery/TokenVfxComponent.h"

namespace td
{
	TokenVfxComponent::TokenVfxComponent():
		TokenVfxComponent(nullptr)
	{}

	TokenVfxComponent::TokenVfxComponent(RefCounter* refCounter):
		Component(refCounter)
	{}

	void TokenVfxComponent::OnDraw()
	{
		onDraw();
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<td::TokenVfxComponent>);
// --- META ---

DECLARE_CLASS(td::TokenVfxComponent, td__TokenVfxComponent);
// --- END META ---
