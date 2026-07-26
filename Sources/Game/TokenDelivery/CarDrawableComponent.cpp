#include "o2/stdafx.h"
#include "TokenDelivery/CarDrawableComponent.h"

#include "TokenDelivery/ArtSprites.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/ImageAsset.h"
#include "o2/Render/Particles/ParticlesEffects.h"
#include "o2/Render/Particles/ParticlesEmitterShapes.h"
#include "o2/Scene/Actor.h"
#include "o2/Utils/Math/ColorGradient.h"

namespace td
{
	CarDrawableComponent::CarDrawableComponent():
		CarDrawableComponent(nullptr)
	{}

	CarDrawableComponent::CarDrawableComponent(RefCounter* refCounter):
		Component(refCounter)
	{}

	static void LoadDirSprites(const char* prefix, Vector<Ref<Sprite>>& sprites,
							   Vector<Vec2F>& offsets)
	{
		static const char* kDirs[] = { "e", "s", "w", "n" }; // indexed by angle/90 (0 = E)
		for (auto dir : kDirs)
		{
			String path = String("Game/Cars/") + prefix + "_" + dir + ".png";
			auto sprite = mmake<Sprite>(path);
			Vec2F offset;
			if (auto meta = art::Find(path.Data()))
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
		mKind = kind;

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
			case CarKind::Hatchback: LoadDirSprites("traffic_sport", mDirSprites, mDirOffsets); break;
		}

		// the smoke child sorts independently just under the car; created here when the
		// actor is not a prototype instance that already carries it
		if (auto owner = GetActor())
		{
			auto smoke = owner->FindChild("smoke");
			if (!smoke)
			{
				smoke = mmake<Actor>(owner->IsOnScene() ? ActorCreateMode::InScene
													    : ActorCreateMode::NotInScene);
				smoke->SetName("smoke");
				owner->AddChild(smoke, false);
				SetupSmokeEmitter(smoke->AddComponent<ParticlesEmitterComponent>());
			}
			smoke->SetDrawingDepthInheritFromParent(false);
			mSmokeEmitter = smoke->GetComponent<ParticlesEmitterComponent>();
		}
	}

	void CarDrawableComponent::SetupSmokeEmitter(const Ref<ParticlesEmitterComponent>& emitter)
	{
		auto source = mmake<SingleSpriteParticleSource>();
		source->image = o2Assets.GetAssetRefByType<ImageAsset>(String("Game/Props/smoke.png"));
		emitter->SetParticlesSource(source);

		emitter->SetShape(mmake<CircleParticlesEmitterShape>());

		// world-relative, so the emitted puffs stay behind while the car drives away
		emitter->SetParticlesRelativity(false);
		emitter->SetMaxParticles(32);
		emitter->SetParticlesPerSecond(0.0f);
		emitter->SetParticlesLifetime(0.6f);
		emitter->SetInitialSize(0.6f);
		emitter->SetInitialSizeRange(0.1f);
		emitter->SetInitialSpeed(45.0f);
		emitter->SetInitialSpeedRange(15.0f);
		emitter->SetEmitParticlesMoveDirection(90.0f); // drifts upwards
		emitter->SetEmitParticlesMoveDirectionRange(25.0f);

		auto grow = mmake<ParticlesSizeEffect>();
		grow->curve = mmake<Curve>(Vector<Vec2F>{ Vec2F(0.0f, 1.0f), Vec2F(1.0f, 2.4f) });
		emitter->AddEffect(grow);

		auto fade = mmake<ParticlesColorEffect>();
		fade->colorGradient = mmake<ColorGradient>(Color4(255, 255, 255, 140),
												   Color4(255, 255, 255, 0));
		emitter->AddEffect(fade);
	}

	void CarDrawableComponent::SetPose(const Vec2F& cellPos, float angleDeg, float smokeIntensity)
	{
		mCellPos = cellPos;
		mAngle = angleDeg;

		if (mSmokeEmitter)
		{
			// the ghost silhouette must not double the smoke of the real car
			mSmokeEmitter->SetParticlesPerSecond(
				!mGhost && smokeIntensity > 0.05f ? 6.0f + 24.0f*smokeIntensity : 0.0f);
			mSmokeEmitter->GetActor()->SetDrawingDepth(IsoDepth(mCellPos) - 0.02f);
		}
	}

	void CarDrawableComponent::OnStart()
	{
		if (mDirSprites.IsEmpty())
			SetupCar(mKind);
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
		Vec2F carScreen = CellToScreen(mCellPos)
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

		// the ground shadow is baked into the car sprites
		sprite->SetPosition(carScreen);
		sprite->Draw();
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<td::CarDrawableComponent>);
// --- META ---

ENUM_META(td::CarDrawableComponent::CarKind, td__CarDrawableComponent__CarKind)
{
    ENUM_ENTRY(Hatchback);
    ENUM_ENTRY(PlayerPickup);
    ENUM_ENTRY(Van);
}
END_ENUM_META;

DECLARE_CLASS(td::CarDrawableComponent, td__CarDrawableComponent);
// --- END META ---
