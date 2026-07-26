#include "o2/stdafx.h"
#include "TokenDelivery/GameControllerComponent.h"

#include "TokenDelivery/GameConfigJS.h"
#include "TokenDelivery/GameUIStyle.h"
#include "TokenDelivery/TiltShiftPass.h"
#include "o2/Render/Pipeline/RenderPipeline.h"
#include "o2/Render/Render.h"
#include "o2/Application/Input.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Application/VKCodes.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/System/Time/Time.h"

using namespace td;

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

	mWorldCamera = mmake<CameraActor>();
	mWorldCamera->SetName("world camera");
	mWorldCamera->SetFixedSize(Vec2F(1760.0f, 1100.0f));
	mWorldCamera->drawLayers.SetLayers(Vector<String>{ kWorldLayer });
	mWorldCamera->fillBackground = true;
	mWorldCamera->fillColor = Color4(166, 190, 205, 255);

	// tilt-shift: the world renders through an offscreen pass with edge blur; UI camera
	// draws afterwards and stays sharp
	auto pipeline = mmake<RenderPipeline>();
	pipeline->AddPass(mmake<TiltShiftPass>());
	mWorldCamera->SetRenderPipeline(pipeline);

	mUICamera = mmake<CameraActor>();
	mUICamera->SetName("ui camera");
	mUICamera->SetFittedSize(Vec2F(1280.0f, 800.0f));
	mUICamera->drawLayers.SetLayers(Vector<String>{ kUILayer });
	mUICamera->fillBackground = false;

	BuildGameUIStyles();

	mHUD.onRetry = [this]() { StartLevel(mLevel); };
	mHUD.onNextLevel = [this]() { StartLevel(mLevel + 1); };
	mHUD.Build();
	mHUDBuilt = true;

	// the world camera stretches 1760x1100 over the viewport, the UI camera fits 1280x800
	// into it: both scales collapse into one ratio per axis
	mTutorial.worldToUI = [this](const Vec2F& world)
	{
		Vec2F worldSize = mWorldCamera->GetFittedOrFixedSize();
		Vec2F uiSize = mUICamera->GetRenderCamera().GetSize2D();
		Vec3F cameraPos = mWorldCamera->transform->GetPosition();
		return Vec2F((world.x - cameraPos.x)*uiSize.x/worldSize.x,
					 (world.y - cameraPos.y)*uiSize.y/worldSize.y);
	};
	mTutorial.Build();
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

	for (auto& traffic : mTraffic)
	{
		if (traffic.actor)
			traffic.actor->RemoveFromScene();
	}
	mTraffic.Clear();
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

	// hologram pulsing is scripted in JS (Assets/Scripts/HologramPulse.js)
	if (mCity.hologram)
	{
		auto script = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/HologramPulse.js"));
		if (script)
			mCity.hologram->AddComponent<ScriptableComponent>()->SetScript(script);
	}

	mPlayerActor = mmake<Actor>(ActorCreateMode::InScene);
	mPlayerActor->SetName("player car");
	mPlayerActor->SetLayer(kWorldLayer);
	mPlayerCar = mPlayerActor->AddComponent<CarDrawableComponent>();
	mPlayerCar->SetupCar(CarDrawableComponent::CarKind::PlayerPickup);

	// silhouette above buildings so the car stays trackable when occluded
	mPlayerGhostActor = mmake<Actor>(ActorCreateMode::InScene);
	mPlayerGhostActor->SetName("player car ghost");
	mPlayerGhostActor->SetLayer(kWorldLayer);
	mPlayerGhost = mPlayerGhostActor->AddComponent<CarDrawableComponent>();
	mPlayerGhost->SetupCar(CarDrawableComponent::CarKind::PlayerPickup);
	mPlayerGhost->SetGhostMode(true);
	mPlayerGhostActor->SetDrawingDepth(9000.0f);

	static const CarDrawableComponent::CarKind kTrafficKinds[] = {
		CarDrawableComponent::CarKind::Van, CarDrawableComponent::CarKind::Hatchback
	};

	auto& city = mSession.GetCity();
	CarTuning trafficTuning = mSession.GetTuning().car;
	trafficTuning.maxSpeed *= 0.35f;
	trafficTuning.accel *= 0.5f;

	for (int i = 0; i < city.trafficStarts.Count(); i++)
	{
		TrafficCar traffic;
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
		traffic.sim.Reset(trafficTuning, Vec2F((float)start.x, (float)start.y), dir);
		traffic.actor = mmake<Actor>(ActorCreateMode::InScene);
		traffic.actor->SetName("traffic car");
		traffic.actor->SetLayer(kWorldLayer);
		traffic.drawable = traffic.actor->AddComponent<CarDrawableComponent>();
		traffic.drawable->SetupCar(kTrafficKinds[i%2]);
		traffic.decisionTimer = 1.0f + mTrafficRng.Frand()*2.0f;
		mTraffic.Add(traffic);
	}

	mHUD.BindLevel(&mSession, mCity.officeAnchors, mCity.hologram);
	mHUD.HideWindows();

	// snap the camera to the start
	Vec2F startScreen = CellToScreen(mSession.GetCar().GetPos());
	mWorldCamera->transform->SetPosition(Vec3F(startScreen.x, startScreen.y, 0.0f));

	// the car starts on a token source cell, so the tutorial can show the loading right away
	if (sTutorialEnabled && !mTutorialShown)
	{
		mTutorialShown = true;
		mTutorial.Start(&mSession);
	}
}

GameInput GameControllerComponent::CollectInput() const
{
	GameInput input;
	input.turnLeft = o2Input.IsKeyDown(VK_LEFT) || o2Input.IsKeyDown('A');
	input.turnRight = o2Input.IsKeyDown(VK_RIGHT) || o2Input.IsKeyDown('D');
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

void GameControllerComponent::SyncTraffic(float dt)
{
	auto& city = mSession.GetCity();
	for (auto& traffic : mTraffic)
	{
		traffic.decisionTimer -= dt;
		if (traffic.decisionTimer <= 0.0f)
		{
			traffic.decisionTimer = 1.5f + mTrafficRng.Frand()*2.5f;
			float roll = mTrafficRng.Frand();
			traffic.input.turnLeft = roll < 0.2f;
			traffic.input.turnRight = !traffic.input.turnLeft && roll < 0.4f;
		}

		traffic.sim.Tick(dt, traffic.input, city);
		traffic.input.turnLeft = false;
		traffic.input.turnRight = false;

		traffic.drawable->SetPose(traffic.sim.GetVisualPos(), traffic.sim.GetVisualAngle(),
								  traffic.sim.GetDriftIntensity()*0.5f);
		Vec2F screen = CellToScreen(traffic.sim.GetVisualPos());
		traffic.actor->transform->SetPosition(Vec3F(screen.x, screen.y, 0.0f));
		traffic.actor->SetDrawingDepth(IsoDepth(traffic.sim.GetVisualPos()));
	}
}

void GameControllerComponent::FollowCamera(float dt)
{
	Vec2F target = CellToScreen(mSession.GetCar().GetVisualPos());

	// keep the camera inside the city extents
	float size = (float)mSession.GetCity().size;
	float maxX = (size - 1.0f)*kTileHalfW;
	float minY = -(size - 1.0f)*2.0f*kTileHalfH;
	target.x = Math::Clamp(target.x, -maxX*0.55f, maxX*0.55f);
	target.y = Math::Clamp(target.y, minY + 260.0f, -260.0f);

	Vec3F current = mWorldCamera->transform->GetPosition();
	float lerp = Math::Min(1.0f, dt*4.0f);
	Vec3F next(Math::Lerp(current.x, target.x, lerp), Math::Lerp(current.y, target.y, lerp), 0.0f);
	mWorldCamera->transform->SetPosition(next);
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
	// the whole intro so the run starts with a full tank
	bool paused = mTutorial.IsPausingGame();
	float gameDt = paused ? 0.0f : dt;
	mSession.SetFuelDrain(!mTutorial.IsActive());

	if (!paused && mSession.GetState() == SessionState::Playing)
		mSession.Tick(dt, CollectInput());

	int completedOrder = mSession.ConsumeCompletedOrder();
	if (completedOrder >= 0)
		mHUD.ShowOrderCompleted(completedOrder);

	SyncCarView(gameDt);
	SyncTraffic(gameDt);
	FollowCamera(gameDt);

	mHUD.Update(gameDt);
	mHUD.SetSettingsEnabled(!mTutorial.IsActive());
	mTutorial.Update(dt);

	if (!mEndShown)
	{
		// the win window waits until the last delivered tooltip has played its exit
		if (mSession.GetState() == SessionState::Won && !mHUD.HasVisibleTooltips())
		{
			mEndShown = true;
			mHUD.ShowWin();
		}
		else if (mSession.GetState() == SessionState::Lost)
		{
			mEndShown = true;
			mHUD.ShowLose();
		}
	}
}
// --- META ---

DECLARE_CLASS(GameControllerComponent, GameControllerComponent);
// --- END META ---
