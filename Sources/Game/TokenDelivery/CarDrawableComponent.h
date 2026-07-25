#pragma once

#include "TokenDelivery/IsoMath.h"
#include "o2/Render/Mesh.h"
#include "o2/Render/Mesh3DFill.h"
#include "o2/Render/Sprite.h"
#include "o2/Scene/Component.h"

using namespace o2;

// Draws a low-poly 3D car (boxes + cylinder wheels) projected into the isometric world.
// Lives in the 2D scene layer, so it depth-sorts against city sprites by drawDepth; the
// mesh itself is drawn with the depth test on for correct self-occlusion. Also emits
// simple smoke puffs from under the wheels while drifting or accelerating.
class CarDrawableComponent: public Component
{
public:
	enum class CarKind { PlayerPickup, Van, Sedan, Hatchback };

	CarDrawableComponent();
	explicit CarDrawableComponent(RefCounter* refCounter);

	void SetupCar(CarKind kind);

	// ghost mode: flat translucent silhouette drawn above buildings so the player can
	// track the car when it is occluded; no shadow, no smoke, no depth test
	void SetGhostMode(bool ghost) { mGhost = ghost; }

	// cellPos in city cells, angle in degrees (0 = E, growing towards S), smoke 0..1
	void SetPose(const Vec2F& cellPos, float angleDeg, float smokeIntensity);

	// city-space (x = east cells, y = south cells, z = up) to screen projection
	static Mat4 IsoMatrix();

	SERIALIZABLE(CarDrawableComponent);
	CLONEABLE_REF(CarDrawableComponent);

private:
	struct Part
	{
		Mesh3DData data;
		Color4     color;
		Vec3F      offset;

		bool operator==(const Part& other) const { return this == &other; }
	};

	struct Puff
	{
		Vec2F cellPos;
		float age = 0.0f;
		float lifetime = 0.6f;

		bool operator==(const Puff& other) const { return this == &other; }
	};

	Vector<Part>      mParts;
	Vector<Ref<Mesh>> mMeshes;
	Vec2F             mCellPos;
	float             mAngle = 0.0f;
	float             mSmoke = 0.0f;
	bool              mGhost = false;
	Vector<Puff>      mPuffs;
	float             mPuffSpawnAccum = 0.0f;
	Ref<Sprite>       mPuffSprite;

	void OnUpdate(float dt) override;
	void OnDraw() override;
	void AddBox(const Vec3F& size, const Vec3F& offset, const Color4& color);
	void AddWheel(float radius, float width, const Vec3F& offset);

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
    FIELD().PRIVATE().NAME(mParts);
    FIELD().PRIVATE().NAME(mMeshes);
    FIELD().PRIVATE().NAME(mCellPos);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mAngle);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mSmoke);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mGhost);
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
    FUNCTION().PUBLIC().SIGNATURE(void, SetPose, const Vec2F&, float, float);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(Mat4, IsoMatrix);
    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, OnDraw);
    FUNCTION().PRIVATE().SIGNATURE(void, AddBox, const Vec3F&, const Vec3F&, const Color4&);
    FUNCTION().PRIVATE().SIGNATURE(void, AddWheel, float, float, const Vec3F&);
}
END_META;
// --- END META ---
