#pragma once

#include "TokenDelivery/CarDrawableComponent.h"
#include "TokenDelivery/CarSim.h"
#include "TokenDelivery/GameControllerComponent.h"
#include "o2/Scene/Component.h"
#include "o2/Scene/ComponentLinkRef.h"
#include "o2/Scene/Components/ScriptableComponent.h"

using namespace o2;

namespace td
{
	// -------------------------------------------------------------------------------------
	// Self-driving traffic car: owns its car simulation and ticks it against the city of
	// the linked game controller. Turn decisions come from the TrafficAI script on the
	// same actor when it is present; the sibling CarDrawableComponent draws the car.
	// The component freezes together with the game world (tutorial pause).
	// -------------------------------------------------------------------------------------
	class TrafficCarComponent: public Component
	{
	public:
		float speedScale = 0.35f; // Player max speed fraction of this car @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0.1, 1)
		float accelScale = 0.5f;  // Player acceleration fraction of this car @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0.1, 1)

	public:
		// Default constructor
		TrafficCarComponent();

		// Constructor with ref counter
		explicit TrafficCarComponent(RefCounter* refCounter);

		// Places the car on the road cell heading the given way and binds it to the game
		void Spawn(const Ref<GameControllerComponent>& controller, const CarTuning& playerTuning,
				   const Vec2F& cell, Dir dir);

		SERIALIZABLE(TrafficCarComponent);
		CLONEABLE_REF(TrafficCarComponent);

	private:
		LinkRef<GameControllerComponent> mController; // Pause state and city roads source @SERIALIZABLE

		CarSim   mSim;   // Rail-bound car simulation
		CarInput mInput; // Turn command of the current frame, pulsed by the AI script

		Ref<CarDrawableComponent> mDrawable; // Sibling drawable, receives the pose
		Ref<ScriptableComponent>  mAIScript; // Sibling TrafficAI scriptable component, optional

	private:
		// Ticks the simulation and pushes the pose into the drawable
		void OnUpdate(float dt) override;

		// Reads a fresh turn decision from the TrafficAI script instance, if any
		void ReadAIDecision();

		REF_COUNTERABLE_IMPL(Component);
	};
}
// --- META ---

CLASS_BASES_META(td::TrafficCarComponent)
{
    BASE_CLASS(o2::Component);
}
END_META;
CLASS_FIELDS_META(td::TrafficCarComponent)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0.1, 1).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.35f).NAME(speedScale);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0.1, 1).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.5f).NAME(accelScale);
    FIELD().PRIVATE().SERIALIZABLE_ATTRIBUTE().NAME(mController);
    FIELD().PRIVATE().NAME(mSim);
    FIELD().PRIVATE().NAME(mInput);
    FIELD().PRIVATE().NAME(mDrawable);
    FIELD().PRIVATE().NAME(mAIScript);
}
END_META;
CLASS_METHODS_META(td::TrafficCarComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
    FUNCTION().PUBLIC().SIGNATURE(void, Spawn, const Ref<GameControllerComponent>&, const CarTuning&, const Vec2F&, Dir);
    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, ReadAIDecision);
}
END_META;
// --- END META ---
