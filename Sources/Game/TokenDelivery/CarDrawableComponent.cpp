#include "o2/stdafx.h"
#include "TokenDelivery/CarDrawableComponent.h"

#include "TokenDelivery/ArtSprites.h"
#include "o2/Render/Render.h"
#include "o2/Utils/Math/Vertex.h"
#include "o2/Scene/Actor.h"

CarDrawableComponent::CarDrawableComponent():
	CarDrawableComponent(nullptr)
{}

CarDrawableComponent::CarDrawableComponent(RefCounter* refCounter):
	Component(refCounter)
{
	mPuffSprite = mmake<Sprite>(String("Game/Props/smoke.png"));
}

Mat4 CarDrawableComponent::IsoMatrix()
{
	// screenX = (x - y)*128; screenY = -(x + y)*64 + z*kCarZScale;
	// depth (written to z) grows negative towards the camera so nearer geometry wins the test.
	// Scales must stay large: the 2D ortho depth half-range is 100000, so sub-0.1 deltas
	// vanish in float precision and the depth test degenerates
	// The 2D ortho camera looks along -z: LARGER world z is closer and wins the depth test
	const float kCarZScale = 110.0f;
	const float kDepthCell = 400.0f; // per (x + y) cell towards the camera
	const float kDepthUp = 120.0f;   // higher points sit closer to the tilted camera

	Mat4 iso;
	iso.At(0, 0) = td::kTileHalfW; iso.At(0, 1) = -td::kTileHalfW; iso.At(0, 2) = 0.0f;
	iso.At(1, 0) = -td::kTileHalfH; iso.At(1, 1) = -td::kTileHalfH; iso.At(1, 2) = kCarZScale;
	iso.At(2, 0) = kDepthCell; iso.At(2, 1) = kDepthCell; iso.At(2, 2) = kDepthUp;
	return iso;
}

void CarDrawableComponent::AddBox(const Vec3F& size, const Vec3F& offset, const Color4& color)
{
	mParts.Add({ Mesh3DPrimitives::BuildBox(size), color, offset });
	mMeshes.Add(mmake<Mesh>(TextureRef(), 128, 128));
}

void CarDrawableComponent::AddWheel(float radius, float width, const Vec3F& offset)
{
	// BuildCylinder axis is Y, which is the car lateral axis — exactly the wheel axle
	mParts.Add({ Mesh3DPrimitives::BuildCylinder(radius, width, 10), Color4(45, 48, 54, 255),
				 offset });
	mMeshes.Add(mmake<Mesh>(TextureRef(), 128, 128));
}

void CarDrawableComponent::SetupCar(CarKind kind)
{
	mParts.Clear();
	mMeshes.Clear();

	const Color4 window(225, 240, 252, 255);
	const Color4 dark(70, 74, 82, 255);

	switch (kind)
	{
		case CarKind::PlayerPickup:
		{
			const Color4 body(235, 76, 52, 255);
			AddBox(Vec3F(0.56f, 0.34f, 0.16f), Vec3F(0.0f, 0.0f, 0.155f), body);    // chassis
			AddBox(Vec3F(0.24f, 0.30f, 0.17f), Vec3F(0.05f, 0.0f, 0.32f), body);    // cabin
			AddBox(Vec3F(0.20f, 0.27f, 0.11f), Vec3F(0.055f, 0.0f, 0.40f), window); // glass
			AddBox(Vec3F(0.20f, 0.30f, 0.08f), Vec3F(-0.16f, 0.0f, 0.27f), dark);   // cargo bed
			AddBox(Vec3F(0.06f, 0.36f, 0.07f), Vec3F(0.29f, 0.0f, 0.12f), dark);    // bumper
			break;
		}
		case CarKind::Van:
		{
			const Color4 body(116, 158, 222, 255);
			AddBox(Vec3F(0.58f, 0.34f, 0.34f), Vec3F(-0.02f, 0.0f, 0.26f), body);
			AddBox(Vec3F(0.14f, 0.32f, 0.16f), Vec3F(0.24f, 0.0f, 0.17f), body);    // hood
			AddBox(Vec3F(0.10f, 0.30f, 0.13f), Vec3F(0.20f, 0.0f, 0.36f), window);
			break;
		}
		case CarKind::Sedan:
		{
			const Color4 body(238, 158, 134, 255);
			AddBox(Vec3F(0.56f, 0.32f, 0.15f), Vec3F(0.0f, 0.0f, 0.15f), body);
			AddBox(Vec3F(0.28f, 0.28f, 0.15f), Vec3F(-0.02f, 0.0f, 0.30f), body);
			AddBox(Vec3F(0.24f, 0.26f, 0.10f), Vec3F(-0.02f, 0.0f, 0.375f), window);
			break;
		}
		case CarKind::Hatchback:
		{
			const Color4 body(128, 198, 122, 255);
			AddBox(Vec3F(0.48f, 0.31f, 0.16f), Vec3F(0.0f, 0.0f, 0.155f), body);
			AddBox(Vec3F(0.26f, 0.28f, 0.16f), Vec3F(-0.06f, 0.0f, 0.315f), body);
			AddBox(Vec3F(0.22f, 0.25f, 0.11f), Vec3F(-0.06f, 0.0f, 0.40f), window);
			break;
		}
	}

	// side windows: thin light panels on both cabin sides
	const Color4 sideWindow(205, 228, 246, 255);
	switch (kind)
	{
		case CarKind::PlayerPickup:
			AddBox(Vec3F(0.16f, 0.315f, 0.09f), Vec3F(0.05f, 0.0f, 0.40f), sideWindow);
			break;
		case CarKind::Van:
			AddBox(Vec3F(0.30f, 0.355f, 0.12f), Vec3F(0.05f, 0.0f, 0.34f), sideWindow);
			break;
		case CarKind::Sedan:
			AddBox(Vec3F(0.20f, 0.295f, 0.08f), Vec3F(-0.02f, 0.0f, 0.375f), sideWindow);
			break;
		case CarKind::Hatchback:
			AddBox(Vec3F(0.18f, 0.295f, 0.09f), Vec3F(-0.06f, 0.0f, 0.40f), sideWindow);
			break;
	}

	// headlights at the front corners
	const Color4 headlight(255, 246, 200, 255);
	float frontX = kind == CarKind::Hatchback ? 0.24f : 0.28f;
	AddBox(Vec3F(0.03f, 0.07f, 0.05f), Vec3F(frontX, 0.10f, 0.17f), headlight);
	AddBox(Vec3F(0.03f, 0.07f, 0.05f), Vec3F(frontX, -0.10f, 0.17f), headlight);

	float wheelX = kind == CarKind::Hatchback ? 0.15f : 0.18f;
	for (float sx : { 1.0f, -1.0f })
	{
		for (float sy : { 1.0f, -1.0f })
			AddWheel(0.055f, 0.04f, Vec3F(wheelX*sx, 0.125f*sy, 0.055f));
	}
}

void CarDrawableComponent::SetPose(const Vec2F& cellPos, float angleDeg, float smokeIntensity)
{
	mCellPos = cellPos;
	mAngle = angleDeg;
	mSmoke = smokeIntensity;
}

void CarDrawableComponent::OnUpdate(float dt)
{
	for (auto& puff : mPuffs)
		puff.age += dt;
	mPuffs.RemoveAll([](const Puff& p) { return p.age >= p.lifetime; });

	if (mSmoke > 0.05f)
	{
		mPuffSpawnAccum += dt*(6.0f + 24.0f*mSmoke);
		while (mPuffSpawnAccum >= 1.0f)
		{
			mPuffSpawnAccum -= 1.0f;
			float rad = Math::Deg2rad(mAngle);
			Vec2F back(-Math::Cos(rad)*0.25f, -Math::Sin(rad)*0.25f);
			Vec2F side(-back.y, back.x);
			float jitter = (Math::Random(0, 100)/100.0f - 0.5f)*0.2f;
			Puff puff;
			puff.cellPos = mCellPos + back + side*jitter;
			mPuffs.Add(puff);
		}
	}
	else
		mPuffSpawnAccum = 0.0f;
}

// Fills the drawable mesh: positions go through the full iso matrix, but normals are
// rotated only by the car heading — the iso projection is non-orthogonal and would
// destroy the baked lighting. Light matches the reference: soft, from the top-left (NW).
static void FillCarPartMesh(Mesh& mesh, const Mesh3DData& data, const Mat4& isoWorld,
							float headingRad, const Vec3F& localOffset, const Color4& color,
							bool flat = false)
{
	UInt vertexCount = data.positions.Count();
	UInt polyCount = data.indices.Count()/3;
	if (vertexCount == 0 || polyCount == 0)
	{
		mesh.vertexCount = 0;
		mesh.polyCount = 0;
		return;
	}

	mesh.Resize(vertexCount, polyCount);

	float cosA = Math::Cos(headingRad), sinA = Math::Sin(headingRad);
	const Vec3F lightDir = Vec3F(-0.35f, -0.25f, -0.9f).Normalized(); // steep, from NW above
	const float ambient = 0.55f;

	Vertex* verts = mesh.GetVertices<Vertex>();
	for (UInt i = 0; i < vertexCount; i++)
	{
		Vec3F local = data.positions[i] + localOffset;
		Vec3F rotated(local.x*cosA - local.y*sinA, local.x*sinA + local.y*cosA, local.z);
		Vec3F pos = isoWorld.TransformPoint(rotated);

		const Vec3F& n = data.normals[i];
		Vec3F normal(n.x*cosA - n.y*sinA, n.x*sinA + n.y*cosA, n.z);

		float intensity = flat ? 1.0f
						 : ambient + (1.0f - ambient)*Math::Max(normal.Dot(lightDir*-1.0f), 0.0f);
		Color4 vertexColor((int)(color.r*intensity), (int)(color.g*intensity),
						   (int)(color.b*intensity), color.a);

		verts[i] = Vertex(pos.x, pos.y, pos.z, vertexColor.ABGR(), 0.5f, 0.5f);
	}

	VertexIndex* indexes = mesh.GetIndexes();
	for (UInt i = 0; i < polyCount*3; i++)
		indexes[i] = data.indices[i];

	mesh.SetTexture(TextureRef());
	mesh.vertexCount = vertexCount;
	mesh.polyCount = polyCount;
}

void CarDrawableComponent::OnDraw()
{
	if (mGhost)
	{
		// translucent silhouette, drawn above everything without the depth test
		Mat4 ghostWorld = IsoMatrix()*Mat4::Translation(Vec3F(mCellPos.x, mCellPos.y, 0.0f));
		float ghostRad = Math::Deg2rad(mAngle);
		for (int i = 0; i < mParts.Count(); i++)
		{
			auto& part = mParts[i];
			FillCarPartMesh(*mMeshes[i], part.data, ghostWorld, ghostRad, part.offset,
							Color4(255, 255, 255, 26), true);
			mMeshes[i]->Draw();
		}
		return;
	}

	// smoke under the car
	for (auto& puff : mPuffs)
	{
		float t = puff.age/puff.lifetime;
		Vec2F screen = td::CellToScreen(puff.cellPos);
		mPuffSprite->SetPosition(screen + Vec2F(0.0f, 10.0f + 30.0f*t));
		mPuffSprite->SetScale(Vec2F(0.6f + t*0.9f, 0.6f + t*0.9f));
		mPuffSprite->SetTransparency(0.55f*(1.0f - t));
		mPuffSprite->Draw();
	}

	if (mParts.IsEmpty())
		return;

	// soft blob shadow under the car, stretched along the heading like the reference cars
	Vec2F carScreen = td::CellToScreen(mCellPos);
	float rad0 = Math::Deg2rad(mAngle);
	Vec2F screenDir((Math::Cos(rad0) - Math::Sin(rad0))*td::kTileHalfW,
					-(Math::Cos(rad0) + Math::Sin(rad0))*td::kTileHalfH);
	mPuffSprite->SetPosition(carScreen + Vec2F(5.0f, -5.0f));
	mPuffSprite->SetScale(Vec2F(2.1f, 1.05f));
	mPuffSprite->SetAngle(Math::Atan2F(screenDir.y, screenDir.x));
	mPuffSprite->SetColor(Color4(20, 22, 30, 255));
	mPuffSprite->SetTransparency(0.26f);
	mPuffSprite->Draw();
	mPuffSprite->SetColor(Color4(255, 255, 255, 255));
	mPuffSprite->SetAngle(0.0f);
	mPuffSprite->SetScale(Vec2F(1.0f, 1.0f));

	Mat4 isoWorld = IsoMatrix()*Mat4::Translation(Vec3F(mCellPos.x, mCellPos.y, 0.0f));
	float rad = Math::Deg2rad(mAngle);

	o2Render.SetDepthTestEnabled(true, true);
	for (int i = 0; i < mParts.Count(); i++)
	{
		auto& part = mParts[i];
		FillCarPartMesh(*mMeshes[i], part.data, isoWorld, rad, part.offset, part.color);
		mMeshes[i]->Draw();
	}
	o2Render.SetDepthTestEnabled(false);
}
// --- META ---

ENUM_META(CarDrawableComponent::CarKind, CarDrawableComponent__CarKind)
{
    ENUM_ENTRY(Hatchback);
    ENUM_ENTRY(PlayerPickup);
    ENUM_ENTRY(Sedan);
    ENUM_ENTRY(Van);
}
END_ENUM_META;

DECLARE_CLASS(CarDrawableComponent, CarDrawableComponent);
// --- END META ---
