#pragma once

#include "o2/Render/Material.h"
#include "o2/Render/Pipeline/ScenePasses.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/TextureRef.h"

using namespace o2;

namespace td
{
	// -------------------------------------------------------------------------------------
	// Tilt-shift look for the world camera: renders the 2D scene into an offscreen target,
	// then draws it fullscreen with a shader that blurs the top and bottom screen bands,
	// keeping the center sharp. The UI camera is unaffected (separate camera/pipeline).
	// Serialized inside the world camera pipeline of the bootstrap scene.
	// -------------------------------------------------------------------------------------
	class TiltShiftPass: public Scene2DPass
	{
	public:
		float focusBand = 0.35f; // Vertical half-band (0..1 from center) that stays sharp @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0, 1)
		float blurRadius = 5.0f; // Max blur radius in texels at the screen edges @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0, 16)

	public:
		// Renders the scene into the offscreen target and draws it with the blur shader
		void Execute(RenderPassContext& context) override;

		SERIALIZABLE(TiltShiftPass);
		CLONEABLE_REF(TiltShiftPass);

	private:
		TextureRef    mTarget;     // Offscreen render target, resized with the screen
		Vec2I         mTargetSize; // Current target size
		Ref<Material> mMaterial;   // Blur shader material
		Ref<Sprite>   mScreenQuad; // Fullscreen quad drawing the target

	private:
		// Creates the target, material and quad; returns false when the shader is missing
		bool EnsureResources();

		REF_COUNTERABLE_IMPL(Scene2DPass);
	};
}
// --- META ---

CLASS_BASES_META(td::TiltShiftPass)
{
    BASE_CLASS(o2::Scene2DPass);
}
END_META;
CLASS_FIELDS_META(td::TiltShiftPass)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0, 1).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.35f).NAME(focusBand);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0, 16).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(5.0f).NAME(blurRadius);
    FIELD().PRIVATE().NAME(mTarget);
    FIELD().PRIVATE().NAME(mTargetSize);
    FIELD().PRIVATE().NAME(mMaterial);
    FIELD().PRIVATE().NAME(mScreenQuad);
}
END_META;
CLASS_METHODS_META(td::TiltShiftPass)
{

    FUNCTION().PUBLIC().SIGNATURE(void, Execute, RenderPassContext&);
    FUNCTION().PRIVATE().SIGNATURE(bool, EnsureResources);
}
END_META;
// --- END META ---
