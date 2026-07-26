#include "o2/stdafx.h"
#include "TokenDelivery/TutorialDimDrawable.h"

#include "o2/Render/Sprite.h"

namespace td
{
	TutorialDimDrawable::TutorialDimDrawable()
	{
		Sprite spotlight(String("Game/UI/tut_spotlight.png"));
		auto texture = spotlight.GetTexture();
		mMesh = mmake<Mesh>(texture, 8, 10);

		mUVRect = RectF(0.0f, 0.0f, 1.0f, 1.0f);
		if (texture)
		{
			// half texel inset keeps bilinear sampling inside the atlas region
			Vec2F texSize = (Vec2F)texture->GetSize();
			RectI src = spotlight.GetTextureSrcRect();
			mUVRect.left = ((float)src.left + 0.5f)/texSize.x;
			mUVRect.right = ((float)src.right - 0.5f)/texSize.x;
			mUVRect.top = ((float)src.top + 0.5f)/texSize.y;
			mUVRect.bottom = ((float)src.bottom - 0.5f)/texSize.y;
		}
	}

	void TutorialDimDrawable::SetHole(const Vec2F& center, const Vec2F& radius)
	{
		mHoleCenter = center;
		mHoleRadius = radius;
	}

	void TutorialDimDrawable::Draw()
	{
		if (!mEnabled)
			return;

		BuildMesh();
		mMesh->Draw();
		OnDrawn();
	}

	void TutorialDimDrawable::BuildMesh()
	{
		RectF rect = GetRect();
		RectF outer(rect.left - kOverscan, rect.bottom - kOverscan,
					rect.right + kOverscan, rect.top + kOverscan);

		// a hole thinner than a unit collapses into the rect center: the ring trapezoids
		// then cover the whole dim and the hole quad degenerates to nothing
		Vec2F center = rect.Center() + mHoleCenter;
		Vec2F radius = mHoleRadius.x >= 1.0f && mHoleRadius.y >= 1.0f ? mHoleRadius : Vec2F();

		float x0 = Math::Clamp(center.x - radius.x, outer.left, outer.right);
		float x1 = Math::Clamp(center.x + radius.x, outer.left, outer.right);
		float y0 = Math::Clamp(center.y - radius.y, outer.bottom, outer.top);
		float y1 = Math::Clamp(center.y + radius.y, outer.bottom, outer.top);

		float u0 = mUVRect.left, u1 = mUVRect.right;
		float v0 = mUVRect.bottom, v1 = mUVRect.top;

		UInt32 color = mResultColor.ABGR();

		// outer corners take the UV of the matching hole corner, so every ring triangle
		// interpolates along one opaque border row/column of the spotlight texture
		auto vertices = mMesh->GetVertices<Vertex>();
		vertices[0] = Vertex(outer.left, outer.bottom, 0.0f, color, u0, v0);  // outer LB
		vertices[1] = Vertex(outer.right, outer.bottom, 0.0f, color, u1, v0); // outer RB
		vertices[2] = Vertex(outer.right, outer.top, 0.0f, color, u1, v1);    // outer RT
		vertices[3] = Vertex(outer.left, outer.top, 0.0f, color, u0, v1);     // outer LT
		vertices[4] = Vertex(x0, y0, 0.0f, color, u0, v0);                    // hole LB
		vertices[5] = Vertex(x1, y0, 0.0f, color, u1, v0);                    // hole RB
		vertices[6] = Vertex(x1, y1, 0.0f, color, u1, v1);                    // hole RT
		vertices[7] = Vertex(x0, y1, 0.0f, color, u0, v1);                    // hole LT

		static const VertexIndex kIndexes[] = {
			0, 1, 5,  0, 5, 4, // bottom trapezoid
			1, 2, 6,  1, 6, 5, // right trapezoid
			2, 3, 7,  2, 7, 6, // top trapezoid
			3, 0, 4,  3, 4, 7, // left trapezoid
			4, 5, 6,  4, 6, 7  // spotlight gradient quad in the hole
		};
		auto indexes = mMesh->GetIndexes();
		for (int i = 0; i < 30; i++)
			indexes[i] = kIndexes[i];

		mMesh->vertexCount = 8;
		mMesh->polyCount = 10;
	}
}
// --- META ---

DECLARE_CLASS(td::TutorialDimDrawable, td__TutorialDimDrawable);
// --- END META ---
