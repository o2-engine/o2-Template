#include "o2/stdafx.h"
#include "TokenDelivery/GameControllerComponent.h"

#include "TokenDelivery/GameConfigJS.h"
#include "TokenDelivery/GameUIStyle.h"
#include "TokenDelivery/TiltShiftPass.h"
#include "TokenDelivery/TrafficCarComponent.h"
#include "o2/Application/Input.h"
#include "o2/Application/VKCodes.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Render/Pipeline/RenderPipeline.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/System/Time/Time.h"

namespace td
{
	UInt32 GameControllerComponent::sForcedSeed = 0;
	bool GameControllerComponent::sTutorialEnabled = true;

	GameControllerComponent::GameControllerComponent():
		GameControllerComponent(nullptr)
	{}

	GameControllerComponent::GameControllerComponent(RefCounter* refCounter):
		Component(refCounter)
	{}

	void GameControllerComponent::OnStart()
	{
		// the scene starts a component of a starting actor twice (actor list and component
		// list both hold it), and a second setup would rebuild the HUD over a running level
		if (mHUDBuilt)
			return;

		SetupScene();
		StartLevel(1);
	}

	void GameControllerComponent::SetupScene()
	{
		o2Scene.AddLayer(kWorldLayer);
		o2Scene.AddLayer(kUILayer);

		if (!mWorldCamera)
		{
			auto camera = mmake<CameraActor>();
			camera->SetName("world camera");
			// fitted, not fixed: a fixed size stretches its rect onto any window shape and
			// squashes the isometry. Fitting keeps the tile scale and shows more city on a
			// wider window
			camera->SetFittedSize(Vec2F(1760.0f, 1100.0f));
			camera->drawLayers.SetLayers(Vector<String>{ kWorldLayer });
			camera->fillBackground = true;
			camera->fillColor = Color4(166, 190, 205, 255);

			// tilt-shift: the world renders through an offscreen pass with edge blur; UI
			// camera draws afterwards and stays sharp
			auto pipeline = mmake<RenderPipeline>();
			pipeline->AddPass(mmake<TiltShiftPass>());
			camera->SetRenderPipeline(pipeline);

			auto followScript = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/CameraFollow.js"));
			if (followScript)
				camera->AddComponent<ScriptableComponent>()->SetScript(followScript);
			mWorldCamera = camera;
		}

		if (!mUICamera)
		{
			auto camera = mmake<CameraActor>();
			camera->SetName("ui camera");
			camera->SetFittedSize(Vec2F(1280.0f, 800.0f));
			camera->drawLayers.SetLayers(Vector<String>{ kUILayer });
			camera->fillBackground = false;
			mUICamera = camera;
		}

		// camera follow is scripted in JS (Assets/Scripts/CameraFollow.js); the controller
		// only feeds the target point and the city bounds
		mCameraFollow = mWorldCamera->GetComponent<ScriptableComponent>();

		BuildGameUIStyles();

		mAudio = mmake<GameAudio>();
		mAudio->Load();

		auto owner = GetActor();
		if (!mHUD)
			mHUD = owner->GetComponent<GameHUDComponent>();
		if (!mHUD)
			mHUD = owner->AddComponent<GameHUDComponent>();

		mHUD->onRetry = [this]() { StartLevel(mLevel); };
		mHUD->onNextLevel = [this]() { StartLevel(mLevel + 1); };
		mHUD->SetAudio(mAudio);
		mHUD->Build();
		mHUDBuilt = true;

		if (!mTutorial)
			mTutorial = owner->GetComponent<GameTutorialComponent>();
		if (!mTutorial)
			mTutorial = owner->AddComponent<GameTutorialComponent>();

		// both cameras fit their rect into the same window, so the scale ratio between them
		// is constant whatever the window shape is
		mTutorial->worldToUI = [this](const Vec2F& world)
		{
			Vec2F worldSize = mWorldCamera->GetRenderCamera().GetSize2D();
			Vec2F uiSize = mUICamera->GetRenderCamera().GetSize2D();
			Vec3F cameraPos = mWorldCamera->transform->GetPosition();
			return Vec2F((world.x - cameraPos.x)*uiSize.x/worldSize.x,
						 (world.y - cameraPos.y)*uiSize.y/worldSize.y);
		};
		mHUD->worldToUI = mTutorial->worldToUI; // the nav arrow maps through the same cameras
		mTutorial->Build();
	}

	void GameControllerComponent::ClearLevel()
	{
		if (mCity.root)
			mCity.root->RemoveFromScene();
		mCity = CityViewHandles();

		if (mPlayerActor)
			mPlayerActor->RemoveFromScene();
		mPlayerActor = nullptr;
		mPlayerCar = nullptr;

		if (mPlayerGhostActor)
			mPlayerGhostActor->RemoveFromScene();
		mPlayerGhostActor = nullptr;
		mPlayerGhost = nullptr;

		for (auto& actor : mTrafficActors)
		{
			if (actor)
				actor->RemoveFromScene();
		}
		mTrafficActors.Clear();
	}

	Ref<Actor> GameControllerComponent::MakeCarActor(const AssetRef<ActorAsset>& proto,
													 CarDrawableComponent::CarKind kind,
													 const String& name)
	{
		Ref<Actor> actor = proto ? mmake<Actor>(proto, ActorCreateMode::InScene)
								 : mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(name);
		actor->SetLayer(kWorldLayer);

		auto drawable = actor->GetComponent<CarDrawableComponent>();
		if (!drawable)
			drawable = actor->AddComponent<CarDrawableComponent>();
		drawable->SetupCar(kind);
		return actor;
	}

	void GameControllerComponent::StartLevel(int level)
	{
		ClearLevel();

		mLevel = Math::Max(1, level);
		mEndShown = false;

		UInt32 seed = sForcedSeed != 0 ? sForcedSeed
					: (UInt32)(o2Time.GetApplicationTime()*1000.0f) + (UInt32)mLevel*977u;
		mSession.Start(mLevel, seed, LoadTuningFromJS());

		mCity = BuildCityView(mSession.GetCity());

		mPlayerActor = MakeCarActor(mPlayerCarProto, CarDrawableComponent::CarKind::PlayerPickup,
									"player car");
		mPlayerCar = mPlayerActor->GetComponent<CarDrawableComponent>();

		// silhouette above buildings so the car stays trackable when occluded
		mPlayerGhostActor = MakeCarActor(mPlayerCarProto, CarDrawableComponent::CarKind::PlayerPickup,
										 "player car ghost");
		mPlayerGhost = mPlayerGhostActor->GetComponent<CarDrawableComponent>();
		mPlayerGhost->SetGhostMode(true);
		mPlayerGhostActor->SetDrawingDepth(9000.0f);

		auto& city = mSession.GetCity();
		auto trafficScript = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/TrafficAI.js"));
		for (int i = 0; i < city.trafficStarts.Count(); i++)
		{
			Vec2I start = city.trafficStarts[i];
			Dir dir = Dir::E;
			for (int d = 0; d < 4; d++)
			{
				if (city.IsRoad(start + DirVec((Dir)d)))
				{
					dir = (Dir)d;
					break;
				}
			}

			bool van = i%2 == 0;
			auto actor = MakeCarActor(van ? mTrafficVanProto : mTrafficHatchbackProto,
									  van ? CarDrawableComponent::CarKind::Van
										  : CarDrawableComponent::CarKind::Hatchback,
									  "traffic car");

			// turn decisions are scripted in JS (Assets/Scripts/TrafficAI.js)
			if (!actor->GetComponent<ScriptableComponent>() && trafficScript)
				actor->AddComponent<ScriptableComponent>()->SetScript(trafficScript);

			auto traffic = actor->GetComponent<TrafficCarComponent>();
			if (!traffic)
				traffic = actor->AddComponent<TrafficCarComponent>();
			traffic->Spawn(Ref(this), mSession.GetTuning().car,
						   Vec2F((float)start.x, (float)start.y), dir);
			mTrafficActors.Add(actor);
		}

		mHUD->BindLevel(&mSession, mCity.officeAnchors, mCity.hologram);
		mHUD->HideWindows();

		// snap the camera to the start; the follow script takes over from there
		Vec2F startScreen = CellToScreen(mSession.GetCar().GetPos());
		mWorldCamera->transform->SetPosition(Vec3F(startScreen.x, startScreen.y, 0.0f));

		if (mCameraFollow)
		{
			// city extents for the follow clamp: the script keeps the camera inside them
			float size = (float)mSession.GetCity().size;
			float maxX = (size - 1.0f)*kTileHalfW;
			float minY = -(size - 1.0f)*2.0f*kTileHalfH;

			auto instance = mCameraFollow->GetInstance();
			if (instance.IsObject())
			{
				instance.SetProperty("_minX", -maxX*0.55f);
				instance.SetProperty("_maxX", maxX*0.55f);
				instance.SetProperty("_minY", minY + 260.0f);
				instance.SetProperty("_maxY", -260.0f);
			}
			PushCameraTarget();
		}

		// the car starts on a token source cell, so the tutorial can show the loading right away
		if (sTutorialEnabled && !mTutorialShown)
		{
			mTutorialShown = true;
			mTutorial->Start(&mSession);
		}
	}

	GameInput GameControllerComponent::CollectInput() const
	{
		GameInput input;
		input.turnLeft = o2Input.IsKeyDown(VK_LEFT) || o2Input.IsKeyDown('A') || mHUD->IsTurningLeft();
		input.turnRight = o2Input.IsKeyDown(VK_RIGHT) || o2Input.IsKeyDown('D') || mHUD->IsTurningRight();
		input.turnAuto = o2Input.IsKeyDown(VK_SPACE);
		return input;
	}

	void GameControllerComponent::SyncCarView(float dt)
	{
		auto& car = mSession.GetCar();
		bool loaded = mSession.GetTokens() > 0;
		mPlayerCar->SetFilled(loaded);
		mPlayerGhost->SetFilled(loaded);
		mPlayerCar->SetPose(car.GetVisualPos(), car.GetVisualAngle(), car.GetDriftIntensity());
		Vec2F screen = CellToScreen(car.GetVisualPos());
		mPlayerActor->transform->SetPosition(Vec3F(screen.x, screen.y, 0.0f));
		mPlayerActor->SetDrawingDepth(IsoDepth(car.GetVisualPos()));

		mPlayerGhost->SetPose(car.GetVisualPos(), car.GetVisualAngle(), 0.0f);
		mPlayerGhostActor->transform->SetPosition(Vec3F(screen.x, screen.y, 0.0f));
	}

	void GameControllerComponent::PushCameraTarget()
	{
		if (!mCameraFollow)
			return;

		auto instance = mCameraFollow->GetInstance();
		if (!instance.IsObject())
			return;

		Vec2F target = CellToScreen(mSession.GetCar().GetVisualPos());
		instance.SetProperty("_targetX", target.x);
		instance.SetProperty("_targetY", target.y);
	}

	void GameControllerComponent::SetSceneLinks(const Ref<CameraActor>& worldCamera,
												const Ref<CameraActor>& uiCamera,
												const Ref<GameHUDComponent>& hud,
												const Ref<GameTutorialComponent>& tutorial,
												const AssetRef<ActorAsset>& playerCarProto,
												const AssetRef<ActorAsset>& trafficVanProto,
												const AssetRef<ActorAsset>& trafficHatchbackProto)
	{
		mWorldCamera = worldCamera;
		mUICamera = uiCamera;
		mHUD = hud;
		mTutorial = tutorial;
		mPlayerCarProto = playerCarProto;
		mTrafficVanProto = trafficVanProto;
		mTrafficHatchbackProto = trafficHatchbackProto;
	}

	void GameControllerComponent::OnUpdate(float dt)
	{
		if (!mHUDBuilt)
			return;

		dt = Math::Min(dt, 1.0f/20.0f);

		if (o2Input.IsKeyPressed('R'))
		{
			StartLevel(mLevel);
			return;
		}

		// the tutorial freezes the world while a step is read, and holds the fuel timer for
		// the whole intro so the run starts with a full tank; the settings window pauses
		// the same way — the car stands and the fuel holds
		bool paused = mTutorial->IsPausingGame() || mHUD->IsSettingsOpen();
		float gameDt = paused ? 0.0f : dt;
		mSession.SetFuelDrain(!mTutorial->IsActive());

		if (!paused && mSession.GetState() == SessionState::Playing)
			mSession.Tick(dt, CollectInput());

		int completedOrder = mSession.ConsumeCompletedOrder();
		if (completedOrder >= 0)
			mHUD->ShowOrderCompleted(completedOrder);

		SyncCarView(gameDt);
		PushCameraTarget();

		mHUD->Update(gameDt);
		mHUD->SetSettingsEnabled(!mTutorial->IsActive());
		mTutorial->Update(dt);

		if (!mEndShown)
		{
			// the win window waits until the last delivered tooltip has played its exit
			if (mSession.GetState() == SessionState::Won && !mHUD->HasVisibleTooltips())
			{
				mEndShown = true;
				mAudio->PlayWin();
				mHUD->ShowWin();
			}
			else if (mSession.GetState() == SessionState::Lost)
			{
				mEndShown = true;
				mAudio->PlayLose();
				mHUD->ShowLose();
			}
		}

		auto& car = mSession.GetCar();
		bool driving = !paused && mSession.GetState() == SessionState::Playing;
		mAudio->SetDriving(driving ? car.GetSpeed()/mSession.GetTuning().car.maxSpeed : 0.0f);
		mAudio->SetDrift(driving ? car.GetDriftIntensity() : 0.0f);
		mAudio->SetMusicActive(!mEndShown);
		mAudio->Update(dt);
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<td::GameControllerComponent>);
// --- META ---

DECLARE_CLASS(td::GameControllerComponent, td__GameControllerComponent);
// --- END META ---
