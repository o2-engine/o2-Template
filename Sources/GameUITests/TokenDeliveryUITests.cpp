#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "TokenDelivery/GameControllerComponent.h"
#include "TokenDelivery/SplashScreen.h"
#include "TokenDelivery/TokenDeliveryGame.h"
#include "TokenDelivery/TrafficCarComponent.h"
#include "TokenDelivery/TutorialDimDrawable.h"
#include "o2/Application/Application.h"
#include "o2/Application/Input.h"
#include "o2/Application/VKCodes.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Scene/UI/Widgets/Toggle.h"
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

// Renders every car kind close-up at different headings — a pure visual check of the
// direction sprites and of depth sorting between overlapping cars
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
		// player sprite car in all four heading quadrants
		{ CarDrawableComponent::CarKind::PlayerPickup, Vec2F(0.0f, 0.0f), 0.0f },
		{ CarDrawableComponent::CarKind::PlayerPickup, Vec2F(2.0f, 0.0f), 90.0f },
		{ CarDrawableComponent::CarKind::PlayerPickup, Vec2F(0.0f, 2.0f), 180.0f },
		{ CarDrawableComponent::CarKind::PlayerPickup, Vec2F(1.5f, -1.0f), 270.0f },
		{ CarDrawableComponent::CarKind::Van, Vec2F(1.0f, 0.0f), 90.0f },
		{ CarDrawableComponent::CarKind::Hatchback, Vec2F(0.0f, 1.0f), 180.0f },
		{ CarDrawableComponent::CarKind::Hatchback, Vec2F(1.0f, 1.0f), 45.0f },
		// deliberately overlapping pair to verify car-vs-car depth
		{ CarDrawableComponent::CarKind::Van, Vec2F(2.0f, 1.85f), 90.0f },
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

// Boot splash: a white screen with the large dark engine caption that slowly grows and
// hands over to the game after about a second
TEST(Splash, ShowsGrowingCaptionAndFinishesInAboutASecond)
{
	td::SplashScreen splash;
	splash.Show();
	AppTestDriver::PumpFrames(3);

	auto caption = o2Scene.FindActor("splash caption");
	ASSERT_TRUE(caption);
	float scaleBefore = caption->transform->GetScale().x;

	// half a second in: still showing, the caption has grown
	for (float waited = 0.0f; waited < 0.5f; waited += 0.05f)
	{
		AppTestDriver::Wait(0.05f);
		splash.Update(0.05f);
	}
	EXPECT_FALSE(splash.IsFinished());
	EXPECT_GT(caption->transform->GetScale().x, scaleBefore);

	auto screenshot = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(screenshot);
	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "splash.png");

	auto data = (const UInt32*)screenshot->GetData();
	Vec2I size = screenshot->GetSize();
	auto pixelSum = [&](int px, int py)
	{
		UInt32 pixel = data[py*size.x + px];
		return (int)(pixel & 0xFF) + (int)((pixel >> 8) & 0xFF) + (int)((pixel >> 16) & 0xFF);
	};

	// white background in the corner, dark caption pixels along the center line
	EXPECT_GT(pixelSum(size.x/10, size.y/10), 700);
	int darkOnCenterLine = 0;
	for (int px = 0; px < size.x; px++)
	{
		if (pixelSum(px, size.y/2) < 400)
			darkOnCenterLine++;
	}
	EXPECT_GT(darkOnCenterLine, 20) << "the caption must be visible";

	// the rest of the second passes and the splash reports done
	for (float waited = 0.0f; waited < 0.7f; waited += 0.05f)
		splash.Update(0.05f);
	EXPECT_TRUE(splash.IsFinished());

	splash.Clear();
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(2);
	EXPECT_FALSE(o2Scene.FindActor("splash caption"));

	o2Scene.Clear(true);
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(2);
}

// The tutorial dim is one solid mesh: the old four-sprite composition left subpixel seams
// (bright stripes of the game showing through) between the independently rounded widget
// rects at fractional hole coordinates
TEST(TutorialDim, FractionalHoleLeavesNoSeams)
{
	o2Scene.AddLayer(td::kUILayer);

	auto camera = mmake<CameraActor>();
	camera->SetName("dim test camera");
	camera->SetFixedSize(Vec2F(1280.0f, 800.0f));
	camera->drawLayers.SetLayers(Vector<String>{ td::kUILayer });
	camera->fillBackground = true;
	camera->fillColor = Color4(255, 255, 255, 255);

	auto overlay = mmake<Widget>();
	overlay->SetName("dim overlay");
	overlay->SetLayer(td::kUILayer);
	overlay->layout->anchorMin = Vec2F(0.0f, 0.0f);
	overlay->layout->anchorMax = Vec2F(0.0f, 0.0f);
	overlay->layout->offsetMin = Vec2F(-640.0f, -400.0f);
	overlay->layout->offsetMax = Vec2F(640.0f, 400.0f);

	auto dim = mmake<TutorialDimDrawable>();
	overlay->AddLayer("dim", dim);

	// adversarial fractional edges, like a frame of the spotlight flight after the car
	const Vec2F holeCenter(0.437f, -0.618f);
	const Vec2F holeRadius(235.37f, 235.37f);
	dim->SetHole(holeCenter, holeRadius);

	AppTestDriver::PumpFrames(5);
	auto screenshot = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(screenshot);

	auto data = (const UInt32*)screenshot->GetData();
	Vec2I size = screenshot->GetSize();
	auto pixelSum = [&](int px, int py)
	{
		UInt32 pixel = data[py*size.x + px];
		return (int)(pixel & 0xFF) + (int)((pixel >> 8) & 0xFF) + (int)((pixel >> 16) & 0xFF);
	};

	// every pixel outside the hole (with a soft-edge margin, symmetric so the bitmap row
	// order doesn't matter) must be dimmed; a seam lets the white background bleed through
	const float kMarginX = holeRadius.x + 42.0f;
	const float kMarginY = holeRadius.y + 42.0f;
	int brightOutside = 0;
	for (int py = 0; py < size.y; py++)
	{
		for (int px = 0; px < size.x; px++)
		{
			float ux = ((float)px/(float)size.x - 0.5f)*1280.0f;
			float uy = ((float)py/(float)size.y - 0.5f)*800.0f;
			if (Math::Abs(ux) < kMarginX && Math::Abs(uy) < kMarginY)
				continue;

			if (pixelSum(px, py) > 600)
				brightOutside++;
		}
	}
	EXPECT_EQ(brightOutside, 0) << "bright seams in the dim";

	// the hole itself stays open: the background shows through its center
	EXPECT_GT(pixelSum(size.x/2, size.y/2), 600);

	o2Scene.Clear(true);
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(2);
}

// City showcase: fixed-seed generated city rendered without UI, cars or tooltips — the
// map art comparison shots (wide and close-up)
TEST(TokenDeliveryCity, ShowcaseScreenshots)
{
	GameControllerComponent::sForcedSeed = 424242u;
	GameControllerComponent::sTutorialEnabled = false;
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
			camera->SetFittedSize(Vec2F(1100.0f, 688.0f));
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
			camera->SetFittedSize(Vec2F(1760.0f, 1100.0f)); // back to the game framing
	}
	AppTestDriver::PumpFrames(3);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_city_ground.png");

	GameControllerComponent::sForcedSeed = 0;
	GameControllerComponent::sTutorialEnabled = true;
	o2Scene.Clear(true);
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(2);
}

// Boots the full game with a fixed seed and drives it through the real frame loop, with
// the intro tutorial off — it freezes the session and every step waits for a tap
class TokenDeliveryApp: public ::testing::Test
{
protected:
	void SetUp() override
	{
		GameControllerComponent::sForcedSeed = 1234567u;
		GameControllerComponent::sTutorialEnabled = false;
		td::LaunchTokenDelivery();
		AppTestDriver::PumpFrames(8); // controller OnStart builds city, cameras and HUD
	}

	void TearDown() override
	{
		GameControllerComponent::sForcedSeed = 0;
		GameControllerComponent::sTutorialEnabled = true;
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}
};

// The game boots by loading Bootstrap.scn: cameras and the game actor with its three
// components come from the scene data, wired through serialized LinkRefs, and the level
// entities are instantiated from the generated .proto assets
TEST_F(TokenDeliveryApp, BootstrapSceneProvidesCamerasAndLinks)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	// cameras are scene actors from the asset, not code-built
	auto worldCamera = DynamicCast<CameraActor>(o2Scene.FindActor("world camera"));
	auto uiCamera = DynamicCast<CameraActor>(o2Scene.FindActor("ui camera"));
	ASSERT_TRUE(worldCamera);
	ASSERT_TRUE(uiCamera);
	EXPECT_TRUE(worldCamera->GetRenderPipeline()); // tilt-shift pipeline came deserialized
	EXPECT_TRUE(worldCamera->GetComponent<ScriptableComponent>()); // CameraFollow.js

	// the component links resolved onto the game actor
	ASSERT_TRUE(controller->GetHUD());
	ASSERT_TRUE(controller->GetTutorial());
	EXPECT_EQ(controller->GetHUD()->GetActor()->GetName(), "token delivery");

	// the player car is a prototype instance
	auto playerCar = o2Scene.FindActor("player car");
	ASSERT_TRUE(playerCar);
	EXPECT_TRUE(playerCar->GetPrototype());

	// traffic cars carry the drawable, the AI script and the traffic component
	auto trafficCar = o2Scene.FindActor("traffic car");
	ASSERT_TRUE(trafficCar);
	EXPECT_TRUE(trafficCar->GetPrototype());
	EXPECT_TRUE(trafficCar->GetComponent<CarDrawableComponent>());
	EXPECT_TRUE(trafficCar->GetComponent<ScriptableComponent>());
	EXPECT_TRUE(trafficCar->GetComponent<TrafficCarComponent>());
}

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

	// hold the right-turn key; the sim turns at the nearest intersection open to the right
	bool beforeHorizontal = car.GetDir() == Dir::E || car.GetDir() == Dir::W;

	o2Input.OnKeyPressed(VK_RIGHT);
	bool axisChanged = false;
	for (int i = 0; i < 240 && !axisChanged; i++)
	{
		AppTestDriver::PumpFrames(1);
		bool horizontal = car.GetDir() == Dir::E || car.GetDir() == Dir::W;
		axisChanged = horizontal != beforeHorizontal;
	}
	o2Input.OnKeyReleased(VK_RIGHT);
	EXPECT_TRUE(axisChanged);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_turn.png");
}

// The corner arrows steer like the keys: the sim reads a held command, so the button has to
// report its pressed state and a plain tap has to keep steering long enough to reach a turn
TEST_F(TokenDeliveryApp, CornerArrowButtonSteersTheCar)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);
	auto hud = o2Scene.FindActor("hud");
	ASSERT_TRUE(hud);
	ASSERT_TRUE(hud->FindChild("turn left"));
	ASSERT_TRUE(hud->FindChild("turn right"));

	auto& car = controller->GetSession().GetCar();
	AppTestDriver::Wait(0.5f);
	bool beforeHorizontal = car.GetDir() == Dir::E || car.GetDir() == Dir::W;

	// hold the right arrow button, bottom-right corner of the 1280x800 UI rect
	AppTestDriver::PressCursor(UIToScreen(Vec2F(552.0f, -318.0f)));
	AppTestDriver::PumpFrames(2);
	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_turn_buttons.png");

	bool axisChanged = false;
	for (float waited = 0.0f; waited < 8.0f && !axisChanged; waited += 0.05f)
	{
		AppTestDriver::Wait(0.05f);
		axisChanged = (car.GetDir() == Dir::E || car.GetDir() == Dir::W) != beforeHorizontal;
	}
	AppTestDriver::ReleaseCursor();
	EXPECT_TRUE(axisChanged);
}

TEST_F(TokenDeliveryApp, CompletedTaskPanelSlidesInAndOut)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	auto hud = o2Scene.FindActor("hud");
	ASSERT_TRUE(hud);
	auto panel = hud->FindChild("task panel");
	ASSERT_TRUE(panel);
	EXPECT_FALSE(panel->IsEnabled());

	controller->GetHUD()->ShowOrderCompleted(0);
	AppTestDriver::Wait(0.6f); // fully slid in from the left
	EXPECT_TRUE(panel->IsEnabled());

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_task.png");

	// holds, then slides back out; a real in-game delivery may retrigger the panel,
	// so poll until the animation finishes instead of asserting at a fixed time
	bool hidden = false;
	for (int i = 0; i < 600 && !hidden; i++)
	{
		AppTestDriver::PumpFrames(1);
		hidden = !panel->IsEnabled();
	}
	EXPECT_TRUE(hidden);
}

TEST_F(TokenDeliveryApp, SettingsWindowOpensTogglesAndCloses)
{
	auto hud = o2Scene.FindActor("hud");
	ASSERT_TRUE(hud);
	auto settingsWindow = hud->FindChild("settings window");
	ASSERT_TRUE(settingsWindow);
	EXPECT_FALSE(settingsWindow->IsEnabled());

	// settings button center, top-right corner of the 1280x800 UI rect
	AppTestDriver::Click(UIToScreen(Vec2F(584.0f, 344.0f)));
	AppTestDriver::PumpFrames(3);
	ASSERT_TRUE(settingsWindow->IsEnabled());

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_settings.png");

	// sound switch: window (-210,-170)..(210,169), toggle rect (241,216)-(364,275)
	auto toggle = DynamicCast<Toggle>(settingsWindow->FindChild("switch"));
	ASSERT_TRUE(toggle);
	EXPECT_TRUE(toggle->GetValue());
	AppTestDriver::Click(UIToScreen(Vec2F(92.5f, 75.5f)));
	AppTestDriver::PumpFrames(3);
	EXPECT_FALSE(toggle->GetValue());

	// the switch drives the sound bank: effects muted, the music channel untouched
	auto audio = FindController()->GetAudio();
	ASSERT_TRUE(audio);
	EXPECT_FALSE(audio->IsSoundEnabled());
	EXPECT_TRUE(audio->IsMusicEnabled());

	AppTestDriver::Wait(0.3f); // knob slides to the off side
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_settings_off.png");

	// accept on the window bottom edge: hold to capture the press-down state, then
	// release — the click closes the settings
	AppTestDriver::PressCursor(UIToScreen(Vec2F(0.0f, -172.0f)));
	AppTestDriver::Wait(0.15f);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_btn_pressed.png");
	AppTestDriver::ReleaseCursor();
	AppTestDriver::PumpFrames(3);
	EXPECT_FALSE(settingsWindow->IsEnabled());
}

// The settings window pauses the round: the car stands still and the fuel timer holds
// until the window is accepted away
TEST_F(TokenDeliveryApp, SettingsWindowPausesTheGame)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);
	auto hud = o2Scene.FindActor("hud");
	ASSERT_TRUE(hud);
	auto settingsWindow = hud->FindChild("settings window");
	ASSERT_TRUE(settingsWindow);

	auto& session = controller->GetSession();
	AppTestDriver::Wait(0.5f); // the car gets rolling
	EXPECT_GT(session.GetCar().GetSpeed(), 0.1f);

	AppTestDriver::Click(UIToScreen(Vec2F(584.0f, 344.0f))); // settings gear
	AppTestDriver::PumpFrames(3);
	ASSERT_TRUE(settingsWindow->IsEnabled());
	EXPECT_TRUE(controller->IsWorldPaused());

	Vec2F carBefore = session.GetCar().GetPos();
	float fuelBefore = session.GetFuel();
	AppTestDriver::Wait(0.7f);
	EXPECT_EQ(session.GetCar().GetPos(), carBefore);
	EXPECT_FLOAT_EQ(session.GetFuel(), fuelBefore);

	// accept closes the window; the round resumes: the car drives and the fuel burns
	AppTestDriver::Click(UIToScreen(Vec2F(0.0f, -172.0f)));
	AppTestDriver::PumpFrames(3);
	EXPECT_FALSE(settingsWindow->IsEnabled());
	EXPECT_FALSE(controller->IsWorldPaused());

	AppTestDriver::Wait(0.7f);
	EXPECT_NE(session.GetCar().GetPos(), carBefore);
	EXPECT_LT(session.GetFuel(), fuelBefore - 0.3f);
}

TEST_F(TokenDeliveryApp, WinWindowWaitsForTheLastTooltipExit)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);
	auto hud = o2Scene.FindActor("hud");
	ASSERT_TRUE(hud);
	auto winWindow = hud->FindChild("win window");
	ASSERT_TRUE(winWindow);

	auto& session = controller->GetSessionMutable();
	for (int i = 0; i < session.GetCity().orders.Count(); i++)
	{
		session.DebugCompleteOrder(i);
		AppTestDriver::PumpFrames(2); // the controller picks the delivery up and starts the flight
	}
	ASSERT_EQ(session.GetState(), td::SessionState::Won);

	// the last tooltip keeps the tokens flight and its exit animation on screen
	EXPECT_TRUE(controller->GetHUD()->HasVisibleTooltips());
	EXPECT_FALSE(winWindow->IsEnabled());

	// the tooltip lingers 1.3s for the token flight and then plays a 0.44s exit; poll by
	// game time, the frame rate here is far above 60
	bool shown = false;
	for (float waited = 0.0f; waited < 6.0f && !shown; waited += 0.05f)
	{
		AppTestDriver::Wait(0.05f);
		shown = winWindow->IsEnabled();
	}
	EXPECT_TRUE(shown);
	EXPECT_FALSE(controller->GetHUD()->HasVisibleTooltips());

	// the win window swaps the gameplay music for the win jingle; the music bed never
	// stops, it fades to zero
	auto audio = controller->GetAudio();
	ASSERT_TRUE(audio);
	EXPECT_TRUE(audio->GetWinPlayer()->IsPlaying());
	AppTestDriver::Wait(0.4f); // the fade-out ramp settles
	EXPECT_NEAR(audio->GetMusicPlayer()->GetVolume(), 0.0f, 0.001f);
}

// paying for the order drops the balance below its price — the delivered bubble must not
// flash red while it lingers and fades out
TEST_F(TokenDeliveryApp, DeliveredTooltipTextStaysDarkWhileFadingOut)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	auto& session = controller->GetSessionMutable();
	WString amount = WString((String)session.GetCity().orders[0].amount);

	Ref<Label> label;
	for (auto& actor : o2Scene.GetRootActors())
	{
		auto tooltip = DynamicCast<Widget>(actor);
		if (!tooltip || actor->GetName() != "order tooltip")
			continue;
		if (auto found = tooltip->FindChildByType<Label>(); found && found->GetText() == amount)
			label = found;
	}
	ASSERT_TRUE(label);

	session.DebugCompleteOrder(0);
	for (int i = 0; i < 90; i++) // linger with the tokens flight, then the exit animation
	{
		AppTestDriver::PumpFrames(1);
		ASSERT_NE(label->GetColor(), Color4(226, 44, 44, 255)) << "frame " << i;
	}
}

// The window aspect must not squash the isometric city: both cameras have to keep the
// render aspect, and the world has to stay locked to the UI scale
TEST_F(TokenDeliveryApp, IsoScaleSurvivesWindowResize)
{
	auto worldCamera = DynamicCast<CameraActor>(o2Scene.FindActor("world camera"));
	auto uiCamera = DynamicCast<CameraActor>(o2Scene.FindActor("ui camera"));
	ASSERT_TRUE(worldCamera);
	ASSERT_TRUE(uiCamera);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	Vec2I windowSize = o2Application.GetWindowSize();
	for (const Vec2I& size : { Vec2I(1000, 800), Vec2I(1280, 560), windowSize })
	{
		o2Application.SetWindowSize(size);
		AppTestDriver::PumpFrames(3);

		Vec2F resolution = (Vec2F)o2Render.GetResolution();
		Vec2F world = worldCamera->GetRenderCamera().GetSize2D();
		Vec2F ui = uiCamera->GetRenderCamera().GetSize2D();
		String at = (String)" at " + (String)size.x + "x" + (String)size.y;

		// units per pixel equal on both axes, otherwise the tiles stretch
		EXPECT_NEAR(world.x/resolution.x, world.y/resolution.y, 0.001f) << at;
		EXPECT_NEAR(ui.x/resolution.x, ui.y/resolution.y, 0.001f) << at;
		EXPECT_NEAR(world.x/ui.x, 1760.0f/1280.0f, 0.001f) << at;

		AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_resize_" +
									  (String)size.x + "x" + (String)size.y + ".png");
	}
}

TEST_F(TokenDeliveryApp, WinWindowRenders)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	controller->GetHUD()->ShowWin();
	AppTestDriver::Wait(0.3f);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_win.png");
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

// Boots the game with the intro tutorial on, exactly like the shipping first launch
class TokenDeliveryTutorial: public ::testing::Test
{
protected:
	void SetUp() override
	{
		GameControllerComponent::sForcedSeed = 1234567u;
		GameControllerComponent::sTutorialEnabled = true;
		td::LaunchTokenDelivery();
		AppTestDriver::PumpFrames(8);
		o2FileSystem.FolderCreate(kScreenshotsDir, true);
	}

	void TearDown() override
	{
		GameControllerComponent::sForcedSeed = 0;
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}

	// a step freezes the game when its live segment is over: that frozen frame carries the
	// step text and is the one worth a screenshot
	bool WaitForStepText(const Ref<GameControllerComponent>& controller, GameTutorialComponent::Step step)
	{
		auto& tutorial = controller->GetTutorial();
		for (int i = 0; i < 900; i++)
		{
			if (tutorial->GetStep() == step && tutorial->IsPausingGame())
			{
				AppTestDriver::Wait(0.4f); // overlay fade and spotlight settle
				return true;
			}
			AppTestDriver::PumpFrames(1);
		}
		return false;
	}

	void PressAnyKey(KeyboardKey key)
	{
		o2Input.OnKeyPressed(key);
		AppTestDriver::PumpFrames(1);
		o2Input.OnKeyReleased(key);
		AppTestDriver::PumpFrames(1);
	}
};

TEST_F(TokenDeliveryTutorial, EveryStepFreezesTheGameAndWaitsForATap)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);
	auto& tutorial = controller->GetTutorial();
	auto& session = controller->GetSession();

	ASSERT_TRUE(tutorial->IsActive());
	ASSERT_EQ(tutorial->GetStep(), GameTutorialComponent::Step::Intro);
	EXPECT_TRUE(tutorial->IsPausingGame());

	// the frozen intro moves nothing: no driving, no fuel burn
	Vec2F carBefore = session.GetCar().GetPos();
	float fuelBefore = session.GetFuel();
	AppTestDriver::Wait(0.6f);
	EXPECT_EQ(session.GetCar().GetPos(), carBefore);
	EXPECT_FLOAT_EQ(session.GetFuel(), fuelBefore);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "tutorial_1_intro.png");

	// a tap releases the pause and the truck loads tokens under the spotlight
	AppTestDriver::Click(Vec2F(0.0f, 0.0f));
	ASSERT_EQ(tutorial->GetStep(), GameTutorialComponent::Step::Loading);
	EXPECT_FALSE(tutorial->IsPausingGame());
	ASSERT_TRUE(WaitForStepText(controller, GameTutorialComponent::Step::Loading));
	EXPECT_GT(session.GetTokens(), 0);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "tutorial_2_load.png");

	// any key continues as well
	PressAnyKey(VK_SPACE);
	ASSERT_EQ(tutorial->GetStep(), GameTutorialComponent::Step::Controls);
	ASSERT_TRUE(WaitForStepText(controller, GameTutorialComponent::Step::Controls));
	EXPECT_NE(session.GetCar().GetPos(), carBefore); // the live segment drove the truck
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "tutorial_3_controls.png");

	PressAnyKey(VK_LEFT);
	ASSERT_EQ(tutorial->GetStep(), GameTutorialComponent::Step::Fuel);
	ASSERT_TRUE(WaitForStepText(controller, GameTutorialComponent::Step::Fuel));
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "tutorial_4_fuel.png");

	// the tank is still full: the whole intro holds the timer
	EXPECT_FLOAT_EQ(session.GetFuel(), fuelBefore);

	// the last tap ends the tutorial and hands the game over
	AppTestDriver::Click(Vec2F(0.0f, 0.0f));
	EXPECT_FALSE(tutorial->IsActive());
	EXPECT_FALSE(tutorial->IsPausingGame());

	AppTestDriver::Wait(1.0f);
	EXPECT_LT(session.GetFuel(), fuelBefore - 0.5f);
	EXPECT_GT(session.GetCar().GetSpeed(), 0.5f);

	auto overlay = o2Scene.FindActor("tutorial");
	ASSERT_TRUE(overlay);
	EXPECT_FALSE(overlay->IsEnabled()); // faded out and switched itself off
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "tutorial_5_game.png");
}

TEST_F(TokenDeliveryApp, FuelRunOutShowsLoseWindowAndRetryRestarts)
{
	auto controller = FindController();
	ASSERT_TRUE(controller);

	// one blinking segment left in the fuel panel
	controller->GetSessionMutable().DebugSetFuel(8.0f);
	AppTestDriver::PumpFrames(5);
	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_fuel_low.png");

	controller->GetSessionMutable().DebugSetFuel(0.3f);
	AppTestDriver::Wait(2.5f); // fuel dies, the car rolls to a stop, the lose window pops
	ASSERT_EQ(controller->GetSession().GetState(), td::SessionState::Lost);

	// the lose window fades the gameplay music out; the engine faded with the stopped car
	auto audio = controller->GetAudio();
	ASSERT_TRUE(audio);
	EXPECT_NEAR(audio->GetMusicPlayer()->GetVolume(), 0.0f, 0.001f);
	EXPECT_NEAR(audio->GetEnginePlayer()->GetVolume(), 0.0f, 0.001f);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "token_delivery_lose.png");

	// the retry button of the lose window sits on the window bottom edge center
	AppTestDriver::Click(UIToScreen(Vec2F(0.0f, -119.0f)));
	AppTestDriver::PumpFrames(3);
	EXPECT_EQ(controller->GetSession().GetState(), td::SessionState::Playing);
	EXPECT_GT(controller->GetSession().GetFuel(), 30.0f);

	AppTestDriver::Wait(0.4f); // the music fades back in with the new round
	EXPECT_GT(audio->GetMusicPlayer()->GetVolume(), 0.1f);
}
