#pragma once

#include "TokenDelivery/IsoMath.h"
#include "o2/Render/Sprite.h"
#include "o2/Scene/Component.h"
#include "o2/Scene/Components/ParticlesEmitterComponent.h"

using namespace o2;

namespace td
{
	// -------------------------------------------------------------------------------------
	// Draws a car in the isometric world as a 2D sprite with four direction variants
	// (ground shadow baked in) switched by heading. The player car has an extra "filled"
	// sprite set shown while it carries tokens. Wheel smoke comes from a sibling particles
	// emitter (world-relative, so the puffs trail behind the moving car); this component
	// only drives its emission rate. The car kind is serialized, so a prototype instance
	// builds its sprites on start.
	// -------------------------------------------------------------------------------------
	class CarDrawableComponent: public Component
	{
	public:
		enum class CarKind { PlayerPickup, Van, Hatchback };

	public:
		// Default constructor
		CarDrawableComponent();

		// Constructor with ref counter
		explicit CarDrawableComponent(RefCounter* refCounter);

		// Sets the car kind and loads its direction sprite sets
		void SetupCar(CarKind kind);

		// Returns the car kind
		CarKind GetCarKind() const { return mKind; }

		// Sets ghost mode: translucent copy drawn above buildings so the player can track
		// the car when it is occluded
		void SetGhostMode(bool ghost) { mGhost = ghost; }

		// Sets is the car filled: filled cars draw the token-loaded sprite set (player only)
		void SetFilled(bool filled) { mFilled = filled; }

		// Sets the drawn pose: cellPos in city cells, angle in degrees (0 = E, growing
		// towards S), smoke 0..1
		void SetPose(const Vec2F& cellPos, float angleDeg, float smokeIntensity);

		// Configures an emitter as the wheel smoke: rising world-relative puffs that grow
		// and fade; shared by the assets generator and the code fallback
		static void SetupSmokeEmitter(const Ref<ParticlesEmitterComponent>& emitter);

		SERIALIZABLE(CarDrawableComponent);
		CLONEABLE_REF(CarDrawableComponent);

	private:
		CarKind mKind = CarKind::PlayerPickup; // Car sprite set @SERIALIZABLE @EDITOR_PROPERTY
		bool    mGhost = false;                // Draws as a translucent silhouette @SERIALIZABLE @EDITOR_PROPERTY

		Vector<Ref<Sprite>> mDirSprites;     // E, S, W, N sprites picked by heading
		Vector<Vec2F>       mDirOffsets;     // Pivot-to-center shift per direction sprite
		Vector<Ref<Sprite>> mDirSpritesFull; // Token-loaded variant, player only
		Vector<Vec2F>       mDirOffsetsFull; // Pivot-to-center shift of the loaded variant

		Vec2F mCellPos;        // Drawn position, city cells
		float mAngle = 0.0f;   // Drawn heading, degrees
		bool  mFilled = false; // Is the token-loaded sprite set drawn

		Ref<ParticlesEmitterComponent> mSmokeEmitter; // Sibling wheel smoke emitter

	private:
		// Called on first update; builds the sprites of the serialized kind for
		// prototype-instantiated cars
		void OnStart() override;

		// Draws the direction sprite of the current pose
		void OnDraw() override;

		REF_COUNTERABLE_IMPL(Component);
	};
}
// --- META ---

PRE_ENUM_META(td::CarDrawableComponent::CarKind);

CLASS_BASES_META(td::CarDrawableComponent)
{
    BASE_CLASS(o2::Component);
}
END_META;
CLASS_FIELDS_META(td::CarDrawableComponent)
{
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(CarKind::PlayerPickup).NAME(mKind);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(false).NAME(mGhost);
    FIELD().PRIVATE().NAME(mDirSprites);
    FIELD().PRIVATE().NAME(mDirOffsets);
    FIELD().PRIVATE().NAME(mDirSpritesFull);
    FIELD().PRIVATE().NAME(mDirOffsetsFull);
    FIELD().PRIVATE().NAME(mCellPos);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mAngle);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mFilled);
    FIELD().PRIVATE().NAME(mSmokeEmitter);
}
END_META;
CLASS_METHODS_META(td::CarDrawableComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
    FUNCTION().PUBLIC().SIGNATURE(void, SetupCar, CarKind);
    FUNCTION().PUBLIC().SIGNATURE(CarKind, GetCarKind);
    FUNCTION().PUBLIC().SIGNATURE(void, SetGhostMode, bool);
    FUNCTION().PUBLIC().SIGNATURE(void, SetFilled, bool);
    FUNCTION().PUBLIC().SIGNATURE(void, SetPose, const Vec2F&, float, float);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(void, SetupSmokeEmitter, const Ref<ParticlesEmitterComponent>&);
    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, OnDraw);
}
END_META;
// --- END META ---
