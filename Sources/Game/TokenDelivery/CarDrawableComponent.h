#pragma once

#include "TokenDelivery/IsoMath.h"
#include "o2/Render/Sprite.h"
#include "o2/Scene/Component.h"

using namespace o2;

// Draws a car in the isometric world as a 2D sprite with four direction variants
// (ground shadow baked in) switched by heading. The player car has an extra "filled"
// sprite set shown while it carries tokens. Also emits simple smoke puffs from under
// the wheels while drifting or accelerating.
class CarDrawableComponent: public Component
{
public:
	enum class CarKind { PlayerPickup, Van, Sedan, Hatchback };

	CarDrawableComponent();
	explicit CarDrawableComponent(RefCounter* refCounter);

	void SetupCar(CarKind kind);

	// ghost mode: translucent copy drawn above buildings so the player can track the
	// car when it is occluded
	void SetGhostMode(bool ghost) { mGhost = ghost; }

	// filled cars draw the token-loaded sprite set (player only)
	void SetFilled(bool filled) { mFilled = filled; }

	// cellPos in city cells, angle in degrees (0 = E, growing towards S), smoke 0..1
	void SetPose(const Vec2F& cellPos, float angleDeg, float smokeIntensity);

	SERIALIZABLE(CarDrawableComponent);
	CLONEABLE_REF(CarDrawableComponent);

private:
	struct Puff
	{
		Vec2F cellPos;
		float age = 0.0f;
		float lifetime = 0.6f;

		bool operator==(const Puff& other) const { return this == &other; }
	};

	Vector<Ref<Sprite>> mDirSprites;     // E, S, W, N sprites picked by heading
	Vector<Vec2F>       mDirOffsets;     // pivot-to-center shift per direction sprite
	Vector<Ref<Sprite>> mDirSpritesFull; // token-loaded variant, player only
	Vector<Vec2F>       mDirOffsetsFull;
	Vec2F             mCellPos;
	float             mAngle = 0.0f;
	float             mSmoke = 0.0f;
	bool              mGhost = false;
	bool              mFilled = false;
	Vector<Puff>      mPuffs;
	float             mPuffSpawnAccum = 0.0f;
	Ref<Sprite>       mPuffSprite;

	void OnUpdate(float dt) override;
	void OnDraw() override;

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

PRE_ENUM_META(CarDrawableComponent::CarKind);

CLASS_BASES_META(CarDrawableComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(CarDrawableComponent)
{
    FIELD().PRIVATE().NAME(mDirSprites);
    FIELD().PRIVATE().NAME(mDirOffsets);
    FIELD().PRIVATE().NAME(mDirSpritesFull);
    FIELD().PRIVATE().NAME(mDirOffsetsFull);
    FIELD().PRIVATE().NAME(mCellPos);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mAngle);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mSmoke);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mGhost);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mFilled);
    FIELD().PRIVATE().NAME(mPuffs);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mPuffSpawnAccum);
    FIELD().PRIVATE().NAME(mPuffSprite);
}
END_META;
CLASS_METHODS_META(CarDrawableComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
    FUNCTION().PUBLIC().SIGNATURE(void, SetupCar, CarKind);
    FUNCTION().PUBLIC().SIGNATURE(void, SetGhostMode, bool);
    FUNCTION().PUBLIC().SIGNATURE(void, SetFilled, bool);
    FUNCTION().PUBLIC().SIGNATURE(void, SetPose, const Vec2F&, float, float);
    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, OnDraw);
}
END_META;
// --- END META ---
