#pragma once

#include "o2/Render/IRectDrawable.h"
#include "o2/Render/Mesh.h"

using namespace o2;

namespace td
{
	// -------------------------------------------------------------------------------------
	// Full-screen tutorial dim with a soft spotlight hole, drawn as one solid mesh: an
	// eight-triangle ring of trapezoids around the hole rect plus the radial gradient quad
	// inside, all sharing vertices. Separate sprite rects left subpixel seams between their
	// independently rounded layouts; shared mesh vertices are watertight by construction.
	// The spotlight texture is opaque at its borders and carries the dim color, so the ring
	// samples the matching border texels and blends seamlessly into the gradient.
	// -------------------------------------------------------------------------------------
	class TutorialDimDrawable: public IRectDrawable
	{
	public:
		// Default constructor, loads the spotlight texture
		TutorialDimDrawable();

		// Sets the hole rect: center and half-size in local units relative to the drawable
		// rect center; a half-size under one unit closes the hole into a plain full dim
		void SetHole(const Vec2F& center, const Vec2F& radius);

		// Draws the dim mesh
		void Draw() override;

		SERIALIZABLE(TutorialDimDrawable);
		CLONEABLE_REF(TutorialDimDrawable);

	private:
		static constexpr float kOverscan = 2000.0f; // Dim runs past the rect into the window overscan

		Vec2F mHoleCenter; // Hole center, local units from the rect center
		Vec2F mHoleRadius; // Hole half-size; zero closes the hole

		Ref<Mesh> mMesh;   // Shared vertex mesh: ring trapezoids + hole quad
		RectF     mUVRect; // Spotlight rect in the texture, normalized with a half-texel inset

	private:
		// Fills the mesh vertices and triangles for the current rect and hole
		void BuildMesh();
	};
}
// --- META ---

CLASS_BASES_META(td::TutorialDimDrawable)
{
    BASE_CLASS(o2::IRectDrawable);
}
END_META;
CLASS_FIELDS_META(td::TutorialDimDrawable)
{
    FIELD().PRIVATE().NAME(mHoleCenter);
    FIELD().PRIVATE().NAME(mHoleRadius);
    FIELD().PRIVATE().NAME(mMesh);
    FIELD().PRIVATE().NAME(mUVRect);
}
END_META;
CLASS_METHODS_META(td::TutorialDimDrawable)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().SIGNATURE(void, SetHole, const Vec2F&, const Vec2F&);
    FUNCTION().PUBLIC().SIGNATURE(void, Draw);
    FUNCTION().PRIVATE().SIGNATURE(void, BuildMesh);
}
END_META;
// --- END META ---
