#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "TokenDelivery/GameControllerComponent.h"
#include "TokenDelivery/TokenDeliveryGame.h"
#include "o2/Application/Input.h"
#include "o2/Application/VKCodes.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;
using namespace td;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";

	int CountDistinctColors(const Ref<Bitmap>& bitmap)
	{
		std::set<UInt32> colors;
		auto data = (const UInt32*)bitmap->GetData();
		int pixels = bitmap->GetSize().x*bitmap->GetSize().y;
		for (int i = 0; i < pixels; i += 16)
			colors.insert(data[i]);
		return (int)colors.size();
	}

	Ref<GameControllerComponent> FindController()
	{
		auto root = o2Scene.FindActor("token delivery");
		return root ? root->GetComponent<GameControllerComponent>() : nullptr;
	}

	// UI camera is FittedSize(1280x800): converts a UI-space point to window screen space
	Vec2F UIToScreen(const Vec2F& uiPos)
	{
		Vec2I resolution = o2Render.GetResolution();
		float scale = Math::Min(resolution.x/1280.0f, resolution.y/800.0f);
		return uiPos*scale;
	}
}

// Renders the four 3D car kinds close-up at different headings — a pure visual check of
// the projected mesh, shading and depth sorting between overlapping cars
TEST(TokenDeliveryCars, CloseupRendersAllKinds)
{
	o2Scene.AddLayer(td::kWorldLayer);

	auto camera = mmake<CameraActor>();
	camera->SetFixedSize(Vec2F(880.0f, 550.0f));
	camera->drawLayers.SetLayers(Vector<String>{ td::kWorldLayer });
	camera->fillBackground = true;
	camera->fillColor = Color4(160, 165, 175, 255);

	struct Def { CarDrawableComponent::CarKind kind; Vec2F cell; float angle; };
	const Def defs[] = {
		{ CarDrawableComponent::CarKind::PlayerPickup, Vec2F(0.0f, 0.0f), 0.0f },
		{ CarDrawableComponent::CarKind::Van, Vec2F(1.0f, 0.0f), 90.0f },
		{ CarDrawableComponent::CarKind::Sedan, Vec2F(0.0f, 1.0f), 180.0f },
		{ CarDrawableComponent::CarKind::Hatchback, Vec2F(1.0f, 1.0f), 45.0f },
		// deliberately overlapping pair to verify car-vs-car depth
		{ CarDrawableComponent::CarKind::Sedan, Vec2F(2.0f, 1.85f), 90.0f },
		{ CarDrawableComponent::CarKind::Van, Vec2F(2.0f, 2.0f), 90.0f },
	};
	for (auto& def : defs)
	{
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetLayer(td::kWorldLayer);
		auto car = actor->AddComponent<CarDrawableComponent>();
		car->SetupCar(def.kind);
		car->SetPose(def.cell, def.angle, 0.0f);
		Vec2F screen = td::CellToScreen(def.cell);
		actor->transform->SetPosition(Vec3F(screen.x, screen.y, 0.0f));
		actor->SetDrawingDepth(td::IsoDepth(def.cell));
	}

	camera->transform->SetPosition(Vec3F(td::CellToScreen(Vec2F(1.0f, 1.0f)).x,
										 td::CellToScreen(Vec2F(1.0f, 1.0f)).y, 0.0f));
	AppTestDriver::PumpFrames(5);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_cars.png");

	o2Scene.Clear(true);
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(2);
}

// City showcase: fixed-seed generated city rendered without UI, cars or tooltips — the
// map art comparison shots (wide and close-up)
TEST(TokenDeliveryCity, ShowcaseScreenshots)
{
	GameControllerComponent::sForcedSeed = 424242u;
	td::LaunchTokenDelivery();
	AppTestDriver::PumpFrames(8);

	// freeze the game first (its HUD update re-enables tooltips), then hide everything
	// that is not the map: HUD, cars and any world-space widgets (tooltips)
	if (auto controller = o2Scene.FindActor("token delivery"))
		controller->SetEnabled(false);
	auto rootActors = o2Scene.GetRootActors();
	for (auto& actor : rootActors)
	{
		String name = actor->GetName();
		bool worldWidget = DynamicCast<Widget>(actor) != nullptr && name != "hud";
		if (worldWidget)
			actor->RemoveFromScene(); // widget enable states resist plain SetEnabled
		else if (name == "hud" || name == "player car" || name == "player car ghost" ||
				 name == "traffic car")
		{
			actor->SetEnabled(false);
		}
	}
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(3);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_city_wide.png");

	if (auto cameraActor = o2Scene.FindActor("world camera"))
	{
		if (auto camera = DynamicCast<CameraActor>(cameraActor))
			camera->SetFixedSize(Vec2F(1100.0f, 688.0f));
	}
	AppTestDriver::PumpFrames(3);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_city_close.png");

	// ground only: hide buildings and props, keep roads/sidewalks/tiles
	if (auto city = o2Scene.FindActor("city"))
	{
		for (auto& child : city->GetChildren())
		{
			String name = child->GetName();
			if (name.StartsWith("Game/Buildings") || name.StartsWith("Game/Props"))
				child->SetEnabled(false);
		}
	}
	if (auto cameraActor = o2Scene.FindActor("world camera"))
	{
		if (auto camera = DynamicCast<CameraActor>(cameraActor))
			camera->SetFixedSize(Vec2F(1760.0f, 1100.0f));
	}
	AppTestDriver::PumpFrames(3);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_city_ground.png");

	GameControllerComponent::sForcedSeed = 0;
	o2Scene.Clear(true);
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(2);
}

// Boots the full game with a fixed seed and drives it through the real frame loop
class TokenDeliveryApp: public ::testing::Test
{
protected:
	void SetUp() override
	{
		GameControllerComponent::sForcedSeed = 1234567u;
		td::LaunchTokenDelivery();
		AppTestDriver::PumpFrames(8); // controller OnStart builds city, cameras and HUD
	}

	void TearDown() override
	{
		GameControllerComponent::sForcedSeed = 0;
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}
};

TEST_F(TokenDeliveryApp, CityBuildsAndRenders)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);
	ASSERT_TRUE(o2Scene.FindActor("city"));
	ASSERT_TRUE(o2Scene.FindActor("player car"));
	ASSERT_TRUE(o2Scene.FindActor("hud"));

	auto& session = controller->GetSession();
	EXPECT_EQ(session.GetState(), td::SessionState::Playing);
	EXPECT_EQ(session.GetCity().orders.Count(), 3);

	AppTestDriver::Wait(0.5f);
	auto screenshot = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(screenshot);
	EXPECT_GT(CountDistinctColors(screenshot), 30); // the composed city is not a blank frame

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_city.png");
}

TEST_F(TokenDeliveryApp, CarDrivesAndArrowKeysSteer)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);
	auto& car = controller->GetSession().GetCar();

	AppTestDriver::Wait(1.0f);
	EXPECT_GT(car.GetSpeed(), 0.5f); // accelerates on its own, never stops

	// steer with a perpendicular arrow; the sim turns at the nearest allowed intersection
	bool beforeHorizontal = car.GetDir() == Dir::E || car.GetDir() == Dir::W;
	int turnKey = beforeHorizontal ? VK_DOWN : VK_RIGHT;

	o2Input.OnKeyPressed(turnKey);
	bool axisChanged = false;
	for (int i = 0; i < 240 && !axisChanged; i++)
	{
		AppTestDriver::PumpFrames(1);
		bool horizontal = car.GetDir() == Dir::E || car.GetDir() == Dir::W;
		axisChanged = horizontal != beforeHorizontal;
	}
	o2Input.OnKeyReleased(turnKey);
	EXPECT_TRUE(axisChanged);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_turn.png");
}

TEST_F(TokenDeliveryApp, SpaceBoostsAndDrainsReserve)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);
	auto& session = controller->GetSession();

	float reserveBefore = session.GetBoostLeft();

	o2Input.OnKeyPressed(VK_SPACE);
	AppTestDriver::Wait(1.0f);
	EXPECT_TRUE(session.IsBoosting());
	o2Input.OnKeyReleased(VK_SPACE);
	AppTestDriver::PumpFrames(2);

	EXPECT_LT(session.GetBoostLeft(), reserveBefore - 0.5f);
	EXPECT_FALSE(session.IsBoosting());
}

TEST_F(TokenDeliveryApp, FuelDrainsOverTime)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	float fuelBefore = controller->GetSession().GetFuel();
	AppTestDriver::Wait(1.5f);
	EXPECT_LT(controller->GetSession().GetFuel(), fuelBefore - 1.0f);
}

TEST_F(TokenDeliveryApp, TokensFillNearSourceOnStart)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	// the player always starts on a token source cell; a few moments there fill the trunk
	AppTestDriver::Wait(0.4f);
	EXPECT_GT(controller->GetSession().GetTokens(), 0);
}

TEST_F(TokenDeliveryApp, FuelRunOutShowsLoseWindowAndRetryRestarts)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	controller->GetSessionMutable().DebugSetFuel(0.3f);
	AppTestDriver::Wait(2.5f); // fuel dies, the car rolls to a stop, the lose window pops
	ASSERT_EQ(controller->GetSession().GetState(), td::SessionState::Lost);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_lose.png");

	// the retry button of the lose window sits at the window bottom center
	AppTestDriver::Click(UIToScreen(Vec2F(0.0f, -115.0f)));
	AppTestDriver::PumpFrames(3);
	EXPECT_EQ(controller->GetSession().GetState(), td::SessionState::Playing);
	EXPECT_GT(controller->GetSession().GetFuel(), 30.0f);
}

TEST_F(TokenDeliveryApp, BoostButtonPressWorksWithCursor)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	// boost button rect center in UI camera space (FittedSize 1280x800, origin center):
	// anchored to the bottom-right corner, offsets (-190,60)..(-30,220)
	Vec2F buttonCenter = UIToScreen(Vec2F(640.0f - 110.0f, -400.0f + 140.0f));

	AppTestDriver::PressCursor(buttonCenter);
	AppTestDriver::PumpFrames(5);
	EXPECT_TRUE(controller->GetSession().IsBoosting());

	AppTestDriver::ReleaseCursor();
	AppTestDriver::PumpFrames(2);
	EXPECT_FALSE(controller->GetSession().IsBoosting());
}
