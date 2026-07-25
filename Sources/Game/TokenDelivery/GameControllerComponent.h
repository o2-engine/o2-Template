#pragma once

#include "TokenDelivery/CarDrawableComponent.h"
#include "TokenDelivery/CityViewBuilder.h"
#include "TokenDelivery/GameHUD.h"
#include "TokenDelivery/GameSession.h"
#include "o2/Scene/CameraActor.h"

using namespace o2;

// Owns the whole game round: session tick, input, city/car views, traffic, camera follow
// and the HUD. Attached to a single root actor by td::LaunchTokenDelivery().
class GameControllerComponent: public Component
{
public:
	static UInt32 sForcedSeed; // when non-zero, levels generate with this seed (tests)

	GameControllerComponent();
	explicit GameControllerComponent(RefCounter* refCounter);

	const td::GameSession& GetSession() const { return mSession; }
	td::GameSession& GetSessionMutable() { return mSession; }
	int GetLevel() const { return mLevel; }

	void StartLevel(int level);

	SERIALIZABLE(GameControllerComponent);
	CLONEABLE_REF(GameControllerComponent);

private:
	td::GameSession      mSession;
	td::GameHUD          mHUD;
	td::CityViewHandles  mCity;

	Ref<Actor>                 mPlayerActor;
	Ref<CarDrawableComponent>  mPlayerCar;
	Ref<Actor>                 mPlayerGhostActor;
	Ref<CarDrawableComponent>  mPlayerGhost;

	struct TrafficCar
	{
		td::CarSim                sim;
		td::CarInput              input;
		Ref<Actor>                actor;
		Ref<CarDrawableComponent> drawable;
		float                     decisionTimer = 0.0f;

		bool operator==(const TrafficCar& other) const { return this == &other; }
	};
	Vector<TrafficCar> mTraffic;
	td::Rng            mTrafficRng { 1 };

	Ref<CameraActor> mWorldCamera;
	Ref<CameraActor> mUICamera;

	int  mLevel = 1;
	bool mEndShown = false;
	bool mHUDBuilt = false;

	void OnStart() override;
	void OnUpdate(float dt) override;

	void SetupScene();
	void ClearLevel();
	td::GameInput CollectInput() const;
	void SyncCarView(float dt);
	void SyncTraffic(float dt);
	void FollowCamera(float dt);

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(GameControllerComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(GameControllerComponent)
{
    FIELD().PRIVATE().NAME(mSession);
    FIELD().PRIVATE().NAME(mHUD);
    FIELD().PRIVATE().NAME(mCity);
    FIELD().PRIVATE().NAME(mPlayerActor);
    FIELD().PRIVATE().NAME(mPlayerCar);
    FIELD().PRIVATE().NAME(mPlayerGhostActor);
    FIELD().PRIVATE().NAME(mPlayerGhost);
    FIELD().PRIVATE().NAME(mTraffic);
    FIELD().PRIVATE().NAME(mTrafficRng);
    FIELD().PRIVATE().NAME(mWorldCamera);
    FIELD().PRIVATE().NAME(mUICamera);
    FIELD().PRIVATE().DEFAULT_VALUE(1).NAME(mLevel);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mEndShown);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mHUDBuilt);
}
END_META;
CLASS_METHODS_META(GameControllerComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
    FUNCTION().PUBLIC().SIGNATURE(const td::GameSession&, GetSession);
    FUNCTION().PUBLIC().SIGNATURE(td::GameSession&, GetSessionMutable);
    FUNCTION().PUBLIC().SIGNATURE(int, GetLevel);
    FUNCTION().PUBLIC().SIGNATURE(void, StartLevel, int);
    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, SetupScene);
    FUNCTION().PRIVATE().SIGNATURE(void, ClearLevel);
    FUNCTION().PRIVATE().SIGNATURE(td::GameInput, CollectInput);
    FUNCTION().PRIVATE().SIGNATURE(void, SyncCarView, float);
    FUNCTION().PRIVATE().SIGNATURE(void, SyncTraffic, float);
    FUNCTION().PRIVATE().SIGNATURE(void, FollowCamera, float);
}
END_META;
// --- END META ---
