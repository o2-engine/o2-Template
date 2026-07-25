#pragma once

#include "o2/Render/Material.h"
#include "o2/Render/Pipeline/ScenePasses.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/TextureRef.h"

using namespace o2;

// Tilt-shift look for the world camera: renders the 2D scene into an offscreen target,
// then draws it fullscreen with a shader that blurs the top and bottom screen bands,
// keeping the center sharp. The UI camera is unaffected (separate camera/pipeline).
class TiltShiftPass: public Scene2DPass
{
public:
	float focusBand = 0.35f; // vertical half-band (0..1 from center) that stays sharp
	float blurRadius = 5.0f; // max blur radius in texels at the screen edges

	void Execute(RenderPassContext& context) override;

	SERIALIZABLE(TiltShiftPass);
	CLONEABLE_REF(TiltShiftPass);

private:
	TextureRef    mTarget;
	Vec2I         mTargetSize;
	Ref<Material> mMaterial;
	Ref<Sprite>   mScreenQuad;

	bool EnsureResources();

	REF_COUNTERABLE_IMPL(Scene2DPass);
};
// --- META ---

CLASS_BASES_META(TiltShiftPass)
{
    BASE_CLASS(Scene2DPass);
}
END_META;
CLASS_FIELDS_META(TiltShiftPass)
{
    FIELD().PUBLIC().DEFAULT_VALUE(0.35f).NAME(focusBand);
    FIELD().PUBLIC().DEFAULT_VALUE(5.0f).NAME(blurRadius);
    FIELD().PRIVATE().NAME(mTarget);
    FIELD().PRIVATE().NAME(mTargetSize);
    FIELD().PRIVATE().NAME(mMaterial);
    FIELD().PRIVATE().NAME(mScreenQuad);
}
END_META;
CLASS_METHODS_META(TiltShiftPass)
{

    FUNCTION().PUBLIC().SIGNATURE(void, Execute, RenderPassContext&);
    FUNCTION().PRIVATE().SIGNATURE(bool, EnsureResources);
}
END_META;
// --- END META ---
