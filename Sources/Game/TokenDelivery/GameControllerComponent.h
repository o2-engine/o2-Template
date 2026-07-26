#pragma once

#include "TokenDelivery/CarDrawableComponent.h"
#include "TokenDelivery/CityViewBuilder.h"
#include "TokenDelivery/GameHUDComponent.h"
#include "TokenDelivery/GameSession.h"
#include "TokenDelivery/GameTutorialComponent.h"
#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Scene/ActorLinkRef.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/ComponentLinkRef.h"
#include "o2/Scene/Components/ScriptableComponent.h"

using namespace o2;

namespace td
{
	// -------------------------------------------------------------------------------------
	// Owns the game round: session tick, input, level lifetime, city and car views and the
	// window flow. Lives on the game actor of the bootstrap scene next to the HUD and
	// tutorial components; cameras and entity prototypes are linked through serialized
	// references, so the editor can retarget them without code.
	// -------------------------------------------------------------------------------------
	class GameControllerComponent: public Component
	{
	public:
		static UInt32 sForcedSeed;      // When non-zero, levels generate with this seed (tests)
		static bool   sTutorialEnabled; // The intro tutorial plays once, on the first level

	public:
		// Default constructor
		GameControllerComponent();

		// Constructor with ref counter
		explicit GameControllerComponent(RefCounter* refCounter);

		// Returns the running session state
		const GameSession& GetSession() const { return mSession; }

		// Returns the running session state for direct manipulation (tests)
		GameSession& GetSessionMutable() { return mSession; }

		// Returns the HUD component
		const LinkRef<GameHUDComponent>& GetHUD() const { return mHUD; }

		// Returns the tutorial component
		const LinkRef<GameTutorialComponent>& GetTutorial() const { return mTutorial; }

		// Returns the current level number
		int GetLevel() const { return mLevel; }

		// Returns is the world frozen while the tutorial waits for a tap
		bool IsWorldPaused() const { return mTutorial && mTutorial->IsPausingGame(); }

		// Clears the current level and generates the given one
		void StartLevel(int level);

		// Binds the serialized scene links; used by the assets generator when authoring
		// the bootstrap scene
		void SetSceneLinks(const Ref<CameraActor>& worldCamera, const Ref<CameraActor>& uiCamera,
						   const Ref<GameHUDComponent>& hud, const Ref<GameTutorialComponent>& tutorial,
						   const AssetRef<ActorAsset>& playerCarProto,
						   const AssetRef<ActorAsset>& trafficVanProto,
						   const AssetRef<ActorAsset>& trafficHatchbackProto);

		SERIALIZABLE(GameControllerComponent);
		CLONEABLE_REF(GameControllerComponent);

	private:
		LinkRef<CameraActor> mWorldCamera; // Isometric world camera with the tilt-shift pipeline @SERIALIZABLE @EDITOR_PROPERTY
		LinkRef<CameraActor> mUICamera;    // Sharp UI camera drawn over the world @SERIALIZABLE @EDITOR_PROPERTY

		LinkRef<GameHUDComponent>      mHUD;      // Interface component on the same actor @SERIALIZABLE @EDITOR_PROPERTY
		LinkRef<GameTutorialComponent> mTutorial; // Intro tutorial component on the same actor @SERIALIZABLE @EDITOR_PROPERTY

		AssetRef<ActorAsset> mPlayerCarProto;        // Player car prototype @SERIALIZABLE @EDITOR_PROPERTY
		AssetRef<ActorAsset> mTrafficVanProto;       // Van traffic car prototype @SERIALIZABLE @EDITOR_PROPERTY
		AssetRef<ActorAsset> mTrafficHatchbackProto; // Hatchback traffic car prototype @SERIALIZABLE @EDITOR_PROPERTY

		GameSession     mSession; // Headless round logic: city, car, tokens, orders, fuel
		CityViewHandles mCity;    // Static city view of the current level

		Ref<Actor>                mPlayerActor;      // Player car view actor
		Ref<CarDrawableComponent> mPlayerCar;        // Player car drawable
		Ref<Actor>                mPlayerGhostActor; // Translucent silhouette above buildings
		Ref<CarDrawableComponent> mPlayerGhost;      // Ghost drawable

		Vector<Ref<Actor>> mTrafficActors; // Self-driving traffic cars of the level

		Ref<ScriptableComponent> mCameraFollow; // CameraFollow script on the world camera

		int  mLevel = 1;             // Current level number
		bool mEndShown = false;      // The result window is already up
		bool mHUDBuilt = false;      // Scene setup done, guards the double OnStart
		bool mTutorialShown = false; // The intro tutorial has been started once

	private:
		// Called on first update; builds the scene setup and starts the first level
		void OnStart() override;

		// Ticks the session, syncs the views and drives the HUD, tutorial and windows
		void OnUpdate(float dt) override;

		// Resolves the camera, HUD and tutorial links, creating the missing ones, and
		// builds the UI styles and widgets
		void SetupScene();

		// Removes the city, player and traffic actors of the current level
		void ClearLevel();

		// Creates a car actor from the prototype, or by code when the prototype is not set
		Ref<Actor> MakeCarActor(const AssetRef<ActorAsset>& proto,
								CarDrawableComponent::CarKind kind, const String& name);

		// Collects the player turn input: keyboard and the HUD on-screen buttons
		GameInput CollectInput() const;

		// Pushes the session car pose into the player and ghost drawables
		void SyncCarView(float dt);

		// Feeds the follow target of the current frame into the camera script
		void PushCameraTarget();

		REF_COUNTERABLE_IMPL(Component);
	};
}
// --- META ---

CLASS_BASES_META(td::GameControllerComponent)
{
    BASE_CLASS(o2::Component);
}
END_META;
CLASS_FIELDS_META(td::GameControllerComponent)
{
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mWorldCamera);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mUICamera);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mHUD);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mTutorial);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mPlayerCarProto);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mTrafficVanProto);
    FIELD().PRIVATE().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(mTrafficHatchbackProto);
    FIELD().PRIVATE().NAME(mSession);
    FIELD().PRIVATE().NAME(mCity);
    FIELD().PRIVATE().NAME(mPlayerActor);
    FIELD().PRIVATE().NAME(mPlayerCar);
    FIELD().PRIVATE().NAME(mPlayerGhostActor);
    FIELD().PRIVATE().NAME(mPlayerGhost);
    FIELD().PRIVATE().NAME(mTrafficActors);
    FIELD().PRIVATE().NAME(mCameraFollow);
    FIELD().PRIVATE().DEFAULT_VALUE(1).NAME(mLevel);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mEndShown);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mHUDBuilt);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mTutorialShown);
}
END_META;
CLASS_METHODS_META(td::GameControllerComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
    FUNCTION().PUBLIC().SIGNATURE(const GameSession&, GetSession);
    FUNCTION().PUBLIC().SIGNATURE(GameSession&, GetSessionMutable);
    FUNCTION().PUBLIC().SIGNATURE(const LinkRef<GameHUDComponent>&, GetHUD);
    FUNCTION().PUBLIC().SIGNATURE(const LinkRef<GameTutorialComponent>&, GetTutorial);
    FUNCTION().PUBLIC().SIGNATURE(int, GetLevel);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsWorldPaused);
    FUNCTION().PUBLIC().SIGNATURE(void, StartLevel, int);
    FUNCTION().PUBLIC().SIGNATURE(void, SetSceneLinks, const Ref<CameraActor>&, const Ref<CameraActor>&, const Ref<GameHUDComponent>&, const Ref<GameTutorialComponent>&, const AssetRef<ActorAsset>&, const AssetRef<ActorAsset>&, const AssetRef<ActorAsset>&);
    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, SetupScene);
    FUNCTION().PRIVATE().SIGNATURE(void, ClearLevel);
    FUNCTION().PRIVATE().SIGNATURE(Ref<Actor>, MakeCarActor, const AssetRef<ActorAsset>&, CarDrawableComponent::CarKind, const String&);
    FUNCTION().PRIVATE().SIGNATURE(GameInput, CollectInput);
    FUNCTION().PRIVATE().SIGNATURE(void, SyncCarView, float);
    FUNCTION().PRIVATE().SIGNATURE(void, PushCameraTarget);
}
END_META;
// --- END META ---
