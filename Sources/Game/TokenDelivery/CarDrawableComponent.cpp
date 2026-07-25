#include "o2/stdafx.h"
#include "TokenDelivery/CarDrawableComponent.h"

#include "TokenDelivery/ArtSprites.h"
#include "o2/Scene/Actor.h"

CarDrawableComponent::CarDrawableComponent():
	CarDrawableComponent(nullptr)
{}

CarDrawableComponent::CarDrawableComponent(RefCounter* refCounter):
	Component(refCounter)
{
	mPuffSprite = mmake<Sprite>(String("Game/Props/smoke.png"));
}

static void LoadDirSprites(const char* prefix, Vector<Ref<Sprite>>& sprites,
						   Vector<Vec2F>& offsets)
{
	static const char* kDirs[] = { "e", "s", "w", "n" }; // indexed by angle/90 (0 = E)
	for (auto dir : kDirs)
	{
		String path = String("Game/Cars/") + prefix + "_" + dir + ".png";
		auto sprite = mmake<Sprite>(path);
		Vec2F offset;
		if (auto meta = td::art::Find(path.Data()))
		{
			sprite->SetSize(Vec2F((float)meta->w, (float)meta->h));
			offset = Vec2F(meta->w*0.5f - meta->px, meta->py - meta->h*0.5f);
		}
		sprites.Add(sprite);
		offsets.Add(offset);
	}
}

void CarDrawableComponent::SetupCar(CarKind kind)
{
	mDirSprites.Clear();
	mDirOffsets.Clear();
	mDirSpritesFull.Clear();
	mDirOffsetsFull.Clear();

	switch (kind)
	{
		case CarKind::PlayerPickup:
			LoadDirSprites("player", mDirSprites, mDirOffsets);
			LoadDirSprites("player_full", mDirSpritesFull, mDirOffsetsFull);
			break;

		case CarKind::Van:       LoadDirSprites("traffic_blue", mDirSprites, mDirOffsets); break;
		case CarKind::Sedan:     LoadDirSprites("player_full", mDirSprites, mDirOffsets); break;
		case CarKind::Hatchback: LoadDirSprites("traffic_sport", mDirSprites, mDirOffsets); break;
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
	if (mDirSprites.IsEmpty())
		return;

	// pick the direction sprite set by the heading quadrant (0=E, 90=S, ...)
	int quadrant = (int)Math::Round(mAngle/90.0f) % 4;
	if (quadrant < 0)
		quadrant += 4;

	bool full = mFilled && !mDirSpritesFull.IsEmpty();
	auto& sprite = full ? mDirSpritesFull[quadrant] : mDirSprites[quadrant];
	Vec2F carScreen = td::CellToScreen(mCellPos)
					+ (full ? mDirOffsetsFull[quadrant] : mDirOffsets[quadrant]);

	if (mGhost)
	{
		// translucent copy above buildings so the occluded car stays trackable
		sprite->SetTransparency(0.25f);
		sprite->SetPosition(carScreen);
		sprite->Draw();
		sprite->SetTransparency(1.0f);
		return;
	}

	for (auto& puff : mPuffs)
	{
		float t = puff.age/puff.lifetime;
		Vec2F screen = td::CellToScreen(puff.cellPos);
		mPuffSprite->SetPosition(screen + Vec2F(0.0f, 10.0f + 30.0f*t));
		mPuffSprite->SetScale(Vec2F(0.6f + t*0.9f, 0.6f + t*0.9f));
		mPuffSprite->SetTransparency(0.55f*(1.0f - t));
		mPuffSprite->Draw();
	}

	// the ground shadow is baked into the car sprites
	sprite->SetPosition(carScreen);
	sprite->Draw();
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
