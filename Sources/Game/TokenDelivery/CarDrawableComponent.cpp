#include "o2/stdafx.h"
#include "TokenDelivery/CarDrawableComponent.h"

#include "TokenDelivery/ArtSprites.h"
#include "o2/Render/Render.h"
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
	// depth (written to z) grows negative towards the camera so nearer geometry wins the test
	const float kCarZScale = 90.0f;
	const float kDepthCell = -0.02f;  // per (x + y) cell towards the camera
	const float kDepthUp = 0.016f;    // up moves slightly away along the view axis

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

	const Color4 window(180, 214, 240, 255);
	const Color4 dark(70, 74, 82, 255);

	switch (kind)
	{
		case CarKind::PlayerPickup:
		{
			const Color4 body(214, 70, 52, 255);
			AddBox(Vec3F(0.56f, 0.30f, 0.10f), Vec3F(0.0f, 0.0f, 0.13f), body);     // chassis
			AddBox(Vec3F(0.24f, 0.27f, 0.13f), Vec3F(0.05f, 0.0f, 0.245f), body);   // cabin
			AddBox(Vec3F(0.20f, 0.25f, 0.09f), Vec3F(0.055f, 0.0f, 0.30f), window); // glass
			AddBox(Vec3F(0.20f, 0.26f, 0.05f), Vec3F(-0.16f, 0.0f, 0.20f), dark);   // cargo bed
			AddBox(Vec3F(0.06f, 0.32f, 0.05f), Vec3F(0.29f, 0.0f, 0.10f), dark);    // bumper
			break;
		}
		case CarKind::Van:
		{
			const Color4 body(106, 141, 196, 255);
			AddBox(Vec3F(0.58f, 0.30f, 0.26f), Vec3F(-0.02f, 0.0f, 0.21f), body);
			AddBox(Vec3F(0.14f, 0.28f, 0.12f), Vec3F(0.24f, 0.0f, 0.14f), body);    // hood
			AddBox(Vec3F(0.10f, 0.26f, 0.10f), Vec3F(0.20f, 0.0f, 0.27f), window);
			break;
		}
		case CarKind::Sedan:
		{
			const Color4 body(226, 150, 129, 255);
			AddBox(Vec3F(0.56f, 0.28f, 0.10f), Vec3F(0.0f, 0.0f, 0.13f), body);
			AddBox(Vec3F(0.28f, 0.25f, 0.11f), Vec3F(-0.02f, 0.0f, 0.235f), body);
			AddBox(Vec3F(0.24f, 0.23f, 0.07f), Vec3F(-0.02f, 0.0f, 0.28f), window);
			break;
		}
		case CarKind::Hatchback:
		{
			const Color4 body(120, 184, 116, 255);
			AddBox(Vec3F(0.48f, 0.27f, 0.11f), Vec3F(0.0f, 0.0f, 0.13f), body);
			AddBox(Vec3F(0.26f, 0.24f, 0.12f), Vec3F(-0.06f, 0.0f, 0.245f), body);
			AddBox(Vec3F(0.22f, 0.22f, 0.08f), Vec3F(-0.06f, 0.0f, 0.29f), window);
			break;
		}
	}

	float wheelX = kind == CarKind::Hatchback ? 0.15f : 0.18f;
	for (float sx : { 1.0f, -1.0f })
	{
		for (float sy : { 1.0f, -1.0f })
			AddWheel(0.065f, 0.05f, Vec3F(wheelX*sx, 0.15f*sy, 0.065f));
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

void CarDrawableComponent::OnDraw()
{
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

	float rad = Math::Deg2rad(mAngle);
	Mat4 rot;
	rot.At(0, 0) = Math::Cos(rad); rot.At(0, 1) = -Math::Sin(rad);
	rot.At(1, 0) = Math::Sin(rad); rot.At(1, 1) = Math::Cos(rad);

	Mat4 world = IsoMatrix()*Mat4::Translation(Vec3F(mCellPos.x, mCellPos.y, 0.0f))*rot;

	o2Render.SetDepthTestEnabled(true, true);
	for (int i = 0; i < mParts.Count(); i++)
	{
		auto& part = mParts[i];
		Mat4 partWorld = world*Mat4::Translation(part.offset);
		Mesh3DPrimitives::FillMesh(*mMeshes[i], part.data, partWorld, part.color, TextureSource(),
								   true, 0.72f);
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
