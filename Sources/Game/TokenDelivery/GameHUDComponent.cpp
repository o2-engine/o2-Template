#include "o2/stdafx.h"
#include "TokenDelivery/GameHUDComponent.h"

#include "TokenDelivery/ArtSprites.h"
#include "TokenDelivery/CityViewBuilder.h"
#include "TokenDelivery/GameUIStyle.h"
#include "TokenDelivery/TokenVfxComponent.h"
#include "o2/Animation/AnimationClip.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Assets/Types/ImageAsset.h"
#include "o2/Render/FontStyle.h"
#include "o2/Render/Material.h"
#include "o2/Render/Particles/ParticlesEffects.h"
#include "o2/Render/Particles/ParticlesEmitterShapes.h"
#include "o2/Render/Render.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Render/VectorFontEffects.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/UIManager.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Utils/Math/ColorGradient.h"

namespace td
{
	// task panel slide-in x offsets (root-relative), see Update()
	static const float kTaskPanelHiddenX = -380.0f;
	static const float kTaskPanelShownX = 0.0f; // flush with the screen edge
	static const float kTaskPanelWidth = 350.0f;

	// on-screen steering buttons, bottom-right corner
	static const Vec2F kTurnButtonSize(104.0f, 112.0f);
	static const float kTurnButtonBottom = 26.0f;

	// navigation arrow orbiting the player car
	static const float kNavArrowOrbit = 96.0f;     // distance from the car center, UI units
	static const float kNavArrowSize = 56.0f;
	static const float kNavArrowHideCells = 2.0f;  // this close to the delivery it is just noise

	static const Color4 kAmountColor(34, 41, 65, 255);
	static const Color4 kAmountShortColor(226, 44, 44, 255); // order the player can't pay yet

	static Ref<Material> AdditiveMaterial()
	{
		static Ref<Material> material;
		if (!material)
		{
			material = mmake<Material>(*o2Render.GetDefaultMaterial());
			material->SetBlendMode(BlendMode::Add);
		}
		return material;
	}

	// one-shot radial burst of additive sparks, restarted at every token hit; instantiated
	// from the generated prototype, built by code before the generator has been run
	static Ref<ParticlesEmitterComponent> MakeSparkEmitter(const String& layer, float depth,
														   float particleScale)
	{
		if (auto proto = o2Assets.GetAssetRefByType<ActorAsset>(String("Game/Protos/SparkBurst.proto")))
		{
			auto actor = mmake<Actor>(proto, ActorCreateMode::InScene);
			actor->SetDrawingDepth(depth);
			if (auto emitter = actor->GetComponent<ParticlesEmitterComponent>())
			{
				emitter->Stop();
				return emitter;
			}
		}

		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName("spark emitter");
		actor->SetLayer(layer);
		actor->SetDrawingDepth(depth);

		auto emitter = actor->AddComponent<ParticlesEmitterComponent>();
		auto source = mmake<SingleSpriteParticleSource>();
		source->image = o2Assets.GetAssetRefByType<ImageAsset>(String("Game/Props/spark.png"));
		emitter->SetParticlesSource(source);
		emitter->SetMaterial(AdditiveMaterial());
		emitter->SetShape(mmake<CircleParticlesEmitterShape>());
		emitter->SetParticlesRelativity(false);
		emitter->SetMaxParticles(48);
		emitter->SetParticlesPerSecond(130.0f);
		emitter->SetEmissionDuration(0.06f);
		emitter->SetParticlesLifetime(0.45f);
		emitter->SetInitialSize(particleScale);
		emitter->SetInitialSizeRange(particleScale*0.3f);
		emitter->SetInitialSpeed(190.0f);
		emitter->SetInitialSpeedRange(90.0f);
		emitter->SetEmitParticlesMoveDirectionRange(360.0f);

		auto fade = mmake<ParticlesColorEffect>();
		fade->colorGradient = mmake<ColorGradient>(Color4(255, 255, 255, 255),
												   Color4(255, 255, 255, 0));
		emitter->AddEffect(fade);

		emitter->Stop();
		return emitter;
	}

	GameHUDComponent::GameHUDComponent():
		GameHUDComponent(nullptr)
	{}

	GameHUDComponent::GameHUDComponent(RefCounter* refCounter):
		Component(refCounter)
	{}

	Ref<Label> GameHUDComponent::MakeLabel(const Ref<Widget>& parent, const WString& text, int height,
										   const String& style)
	{
		return MakeGameLabel(parent, text, height, style);
	}

	void GameHUDComponent::Build()
	{
		Clear();

		mRoot = mmake<Widget>();
		mRoot->SetName("hud");
		mRoot->SetLayer(kUILayer);
		// a parentless widget takes its rect from layout offsets: center the 1280x800 UI rect
		mRoot->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mRoot->layout->anchorMax = Vec2F(0.0f, 0.0f);
		mRoot->layout->offsetMin = Vec2F(-640.0f, -400.0f);
		mRoot->layout->offsetMax = Vec2F(640.0f, 400.0f);
		mRoot->SetDrawingDepth(10.0f);

		// token counter panel, top-left; the chip icon is baked into the sprite
		auto tokensPanel = o2UI.CreateImage("Game/UI/ui_chips_panel.png");
		mRoot->AddChild(tokensPanel);
		tokensPanel->layout->anchorMin = Vec2F(0.0f, 1.0f);
		tokensPanel->layout->anchorMax = Vec2F(0.0f, 1.0f);
		tokensPanel->layout->offsetMin = Vec2F(24.0f, -104.0f);
		tokensPanel->layout->offsetMax = Vec2F(242.0f, -24.0f);

		// centered in the white part of the panel, right of the baked chip
		mTokensLabel = MakeLabel(tokensPanel, L"0", 34, "dark");
		mTokensLabel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mTokensLabel->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mTokensLabel->layout->offsetMin = Vec2F(73.0f, 17.0f);
		mTokensLabel->layout->offsetMax = Vec2F(-12.0f, -10.0f);

		// fuel panel, bottom-left: pump icon and bar slot are baked into the sprite
		auto fuelPanel = o2UI.CreateImage("Game/UI/ui_fuel_panel.png");
		mRoot->AddChild(fuelPanel);
		fuelPanel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		fuelPanel->layout->anchorMax = Vec2F(0.0f, 0.0f);
		fuelPanel->layout->offsetMin = Vec2F(24.0f, 20.0f);
		fuelPanel->layout->offsetMax = Vec2F(264.0f, 86.0f);

		// six fuel segments in the panel slot; the rightmost drains first. Insets are
		// tuned to the VISIBLE dark area (the slot frame is thicker on top), equal gap
		// on all sides
		mFuelSegments.Clear();
		const float segLeft = 63.0f, segRight = 226.5f, segBottom = 19.0f, segTop = 49.5f;
		const float segGap = 2.0f;
		const float segWidth = (segRight - segLeft - segGap*5.0f)/6.0f;
		for (int i = 0; i < 6; i++)
		{
			const char* segSprite = i == 0 ? "Game/UI/prog_part_left.png"
								  : i == 5 ? "Game/UI/prog_part_right.png"
								  : "Game/UI/prog_part_middle.png";
			auto segment = o2UI.CreateImage(segSprite);
			fuelPanel->AddChild(segment);
			float x = segLeft + i*(segWidth + segGap);
			segment->layout->anchorMin = Vec2F(0.0f, 0.0f);
			segment->layout->anchorMax = Vec2F(0.0f, 0.0f);
			segment->layout->offsetMin = Vec2F(x, segBottom);
			segment->layout->offsetMax = Vec2F(x + segWidth, segTop);
			mFuelSegments.Add(segment);
		}

		// the last segment blinks through a looped ping-pong clip while the fuel is low
		auto blinkClip = AnimationClip::EaseInOut("transparency", 1.0f, 0.4f, 0.5f);
		blinkClip->SetLoop(Loop::PingPong);
		mFuelSegments[0]->AddState("blink", blinkClip);

		// settings, top-right
		mSettingsButton = o2UI.CreateWidget<Button>("settings");
		mRoot->AddChild(mSettingsButton);
		mSettingsButton->layout->anchorMin = Vec2F(1.0f, 1.0f);
		mSettingsButton->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mSettingsButton->layout->offsetMin = Vec2F(-88.0f, -88.0f);
		mSettingsButton->layout->offsetMax = Vec2F(-24.0f, -24.0f);
		mSettingsButton->onClick = [this]() { PlayClick(); ShowSettings(); };

		// steering keys duplicated in the bottom-right corner for mouse and touch
		auto makeTurnButton = [&](const String& style, float right, const Function<void()>& onTap)
		{
			auto button = o2UI.CreateWidget<Button>(style);
			button->SetName(style);
			mRoot->AddChild(button);
			button->layout->anchorMin = Vec2F(1.0f, 0.0f);
			button->layout->anchorMax = Vec2F(1.0f, 0.0f);
			button->layout->offsetMin = Vec2F(right - kTurnButtonSize.x, kTurnButtonBottom);
			button->layout->offsetMax = Vec2F(right, kTurnButtonBottom + kTurnButtonSize.y);
			button->onClick = [this, onTap]() { PlayClick(); onTap(); };
			return button;
		};
		mTurnLeftButton = makeTurnButton("turn left", -156.0f,
										 [this]() { mTurnLeftTap = turnTapTime; });
		mTurnRightButton = makeTurnButton("turn right", -36.0f,
										  [this]() { mTurnRightTap = turnTapTime; });

		// green navigation arrow orbiting the player car: points at the order the player
		// can pay for, or back at the token source while nothing is affordable. Drawn as a
		// plain sprite through its own UI-layer host — widget layers place their drawables
		// by axis-aligned rects, so a widget would never rotate the arrow
		mNavArrowSprite = mmake<Sprite>(String("Game/UI/nav_arrow.png"));
		mNavActor = mmake<Actor>(ActorCreateMode::InScene);
		mNavActor->SetName("nav arrow");
		mNavActor->SetLayer(kUILayer);
		mNavActor->SetDrawingDepth(15.0f); // above the HUD panels, below the windows
		mNavActor->AddComponent<TokenVfxComponent>()->onDraw = [this]() { DrawNavArrow(); };
		mNavActor->SetEnabled(false);

		// completed task panel, slides in from the left edge on order delivery; the slide
		// is the widget "visible" state animation, so a plain SetEnabled plays it
		mTaskPanel = mmake<Widget>();
		mTaskPanel->SetName("task panel");
		mRoot->AddChild(mTaskPanel);
		// only the panel back is translucent, text and check stay solid
		auto taskBack = mTaskPanel->AddLayer("back", mmake<Sprite>(String("Game/UI/task_panel.png")));
		taskBack->SetTransparency(0.82f);
		mTaskPanel->layout->anchorMin = Vec2F(0.0f, 1.0f);
		mTaskPanel->layout->anchorMax = Vec2F(0.0f, 1.0f);
		mTaskPanel->layout->offsetMin = Vec2F(kTaskPanelShownX, -305.0f);
		mTaskPanel->layout->offsetMax = Vec2F(kTaskPanelShownX + kTaskPanelWidth, -120.0f);

		auto slideClip = mmake<AnimationClip>();
		*slideClip->AddTrack<Vec2F>("layout/offsetMin") = AnimationTrack<Vec2F>::EaseInOut(
			Vec2F(kTaskPanelHiddenX, -305.0f), Vec2F(kTaskPanelShownX, -305.0f), 0.3f);
		*slideClip->AddTrack<Vec2F>("layout/offsetMax") = AnimationTrack<Vec2F>::EaseInOut(
			Vec2F(kTaskPanelHiddenX + kTaskPanelWidth, -120.0f),
			Vec2F(kTaskPanelShownX + kTaskPanelWidth, -120.0f), 0.3f);
		mTaskPanel->AddState("visible", slideClip);

		auto taskHeader = MakeLabel(mTaskPanel, L"Active Quests", 26, "quest");
		taskHeader->layout->anchorMin = Vec2F(0.0f, 0.0f);
		taskHeader->layout->anchorMax = Vec2F(0.0f, 0.0f);
		taskHeader->layout->offsetMin = Vec2F(18.0f, 111.0f);
		taskHeader->layout->offsetMax = Vec2F(280.0f, 167.0f);
		if (auto drawable = taskHeader->GetLayerDrawable<Text>("text"))
			drawable->SetHorAlign(HorAlign::Left);

		// green check over the checkbox baked into the panel body
		auto taskCheck = o2UI.CreateImage("Game/UI/task_check.png");
		mTaskPanel->AddChild(taskCheck);
		taskCheck->layout->anchorMin = Vec2F(0.0f, 0.0f);
		taskCheck->layout->anchorMax = Vec2F(0.0f, 0.0f);
		taskCheck->layout->offsetMin = Vec2F(27.0f, 72.0f);
		taskCheck->layout->offsetMax = Vec2F(61.0f, 103.0f);

		// two lines like the reference: "Delivery office" / "<city>"
		mTaskLabel = MakeLabel(mTaskPanel, L"Delivery office", 18, "quest");
		mTaskLabel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mTaskLabel->layout->anchorMax = Vec2F(0.0f, 0.0f);
		mTaskLabel->layout->offsetMin = Vec2F(68.0f, 56.0f);
		mTaskLabel->layout->offsetMax = Vec2F(334.0f, 116.0f);
		if (auto drawable = mTaskLabel->GetLayerDrawable<Text>("text"))
			drawable->SetHorAlign(HorAlign::Left);
		mTaskPanel->SetEnabledForcible(false);

		// token flight vfx: manual-draw host + a spark emitter restarted at the arrivals
		mChipSprite = mmake<Sprite>(String("Game/Props/chip.png"));
		if (auto meta = td::art::Find("Game/Props/chip.png"))
			mChipNativeW = (float)meta->w;

		mVfxActor = mmake<Actor>(ActorCreateMode::InScene);
		mVfxActor->SetName("token vfx");
		mVfxActor->SetLayer(kWorldLayer);
		mVfxActor->SetDrawingDepth(8500.0f);
		mVfxActor->AddComponent<TokenVfxComponent>()->onDraw = [this]() { DrawVfx(); };

		mSparkEmitter = MakeSparkEmitter(kWorldLayer, 8600.0f, 0.62f);

		// windows on top
		mDimmer = mmake<Widget>();
		mDimmer->SetName("dimmer");
		mRoot->AddChild(mDimmer);
		mDimmer->AddLayer("back", mmake<Sprite>(String("Game/UI/panel_dark.png")));
		mDimmer->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mDimmer->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mDimmer->layout->offsetMin = Vec2F(-200.0f, -200.0f);
		mDimmer->layout->offsetMax = Vec2F(200.0f, 200.0f);
		mDimmer->SetTransparency(0.65f);
		mDimmer->SetEnabled(false);

		mWinWindow = MakeResultWindow("win window", "Game/UI/wnd_success_bg.png",
									  Vec2F(-231.0f, -123.0f), Vec2F(232.0f, 187.0f),
									  L"MISSION COMPLETE!", 170.0f,
									  L"All orders have been\nsuccessfully delivered.", 100.0f,
									  L"CONTINUE", [this]() { PlayClick(); onNextLevel(); });
		mLoseWindow = MakeResultWindow("lose window", "Game/UI/wnd_loose_bg.png",
									   Vec2F(-231.0f, -121.0f), Vec2F(231.0f, 194.0f),
									   L"OUT OF FUEL", 158.0f,
									   L"Your delivery truck has run out of fuel.\nRefill and try again!", 96.0f,
									   L"TRY AGAIN", [this]() { PlayClick(); onRetry(); });
		BuildSettingsWindow();
	}

	Ref<Widget> GameHUDComponent::MakeResultWindow(const String& name, const String& bgSprite,
										  const Vec2F& offMin, const Vec2F& offMax,
										  const WString& title, float titleY,
										  const WString& message, float messageY,
										  const WString& buttonCaption, const Function<void()>& onClick)
	{
		auto window = mmake<Widget>();
		window->SetName(name);
		mRoot->AddChild(window);
		window->AddLayer("back", mmake<Sprite>(bgSprite));
		window->layout->anchorMin = Vec2F(0.5f, 0.5f);
		window->layout->anchorMax = Vec2F(0.5f, 0.5f);
		window->layout->offsetMin = offMin;
		window->layout->offsetMax = offMax;

		auto titleLabel = MakeLabel(window, title, 29);
		titleLabel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		titleLabel->layout->anchorMax = Vec2F(1.0f, 0.0f);
		titleLabel->layout->offsetMin = Vec2F(20.0f, titleY - 26.0f);
		titleLabel->layout->offsetMax = Vec2F(-20.0f, titleY + 26.0f);

		auto messageLabel = MakeLabel(window, message, 17);
		messageLabel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		messageLabel->layout->anchorMax = Vec2F(1.0f, 0.0f);
		messageLabel->layout->offsetMin = Vec2F(20.0f, messageY - 30.0f);
		messageLabel->layout->offsetMax = Vec2F(-20.0f, messageY + 30.0f);

		auto button = o2UI.CreateButton(buttonCaption, onClick, "blue");
		window->AddChild(button);
		button->layout->anchorMin = Vec2F(0.5f, 0.0f);
		button->layout->anchorMax = Vec2F(0.5f, 0.0f);
		button->layout->offsetMin = Vec2F(-107.0f, -38.0f);
		button->layout->offsetMax = Vec2F(107.0f, 42.0f);

		if (auto caption = button->GetLayerDrawable<Text>("caption"))
		{
			caption->SetFontAsset(GameUIFont());
			caption->SetHeight(26);
			auto fontStyle = mmake<FontStyle>();
			fontStyle->AddEffect<FontStrokeEffect>(2.5f, Color4(44, 58, 82, 160), 100);
			caption->SetFontStyle(fontStyle);
		}

		window->SetEnabled(false);
		return window;
	}

	void GameHUDComponent::BuildSettingsWindow()
	{
		// wnd_settings_bg has the toggle rows baked (icons + empty sockets); labels,
		// switches and the accept button are placed over the measured socket rects
		mSettingsWindow = mmake<Widget>();
		mSettingsWindow->SetName("settings window");
		mRoot->AddChild(mSettingsWindow);
		mSettingsWindow->AddLayer("back", mmake<Sprite>(String("Game/UI/wnd_settings_bg.png")));
		mSettingsWindow->layout->anchorMin = Vec2F(0.5f, 0.5f);
		mSettingsWindow->layout->anchorMax = Vec2F(0.5f, 0.5f);
		mSettingsWindow->layout->offsetMin = Vec2F(-210.0f, -170.0f);
		mSettingsWindow->layout->offsetMax = Vec2F(210.0f, 169.0f);

		auto soundLabel = MakeLabel(mSettingsWindow, L"SOUND", 22);
		soundLabel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		soundLabel->layout->anchorMax = Vec2F(0.0f, 0.0f);
		soundLabel->layout->offsetMin = Vec2F(126.0f, 228.0f);
		soundLabel->layout->offsetMax = Vec2F(240.0f, 268.0f);
		if (auto drawable = soundLabel->GetLayerDrawable<Text>("text"))
			drawable->SetHorAlign(HorAlign::Left);

		auto musicLabel = MakeLabel(mSettingsWindow, L"MUSIC", 22);
		musicLabel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		musicLabel->layout->anchorMax = Vec2F(0.0f, 0.0f);
		musicLabel->layout->offsetMin = Vec2F(126.0f, 117.0f);
		musicLabel->layout->offsetMax = Vec2F(240.0f, 157.0f);
		if (auto drawable = musicLabel->GetLayerDrawable<Text>("text"))
			drawable->SetHorAlign(HorAlign::Left);

		mSoundToggle = o2UI.CreateWidget<Toggle>("switch");
		mSettingsWindow->AddChild(mSoundToggle);
		mSoundToggle->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mSoundToggle->layout->anchorMax = Vec2F(0.0f, 0.0f);
		mSoundToggle->layout->offsetMin = Vec2F(241.0f, 216.0f);
		mSoundToggle->layout->offsetMax = Vec2F(364.0f, 275.0f);
		mSoundToggle->SetValue(!mAudio || mAudio->IsSoundEnabled());
		mSoundToggle->SetStateForcible("value", mSoundToggle->GetValue());

		// the click plays after the switch applies, so turning the sound off goes silent
		// and turning it on is already audible
		mSoundToggle->onToggleByUser = [this](bool value)
		{
			if (mAudio)
				mAudio->SetSoundEnabled(value);
			PlayClick();
		};

		mMusicToggle = o2UI.CreateWidget<Toggle>("switch");
		mSettingsWindow->AddChild(mMusicToggle);
		mMusicToggle->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mMusicToggle->layout->anchorMax = Vec2F(0.0f, 0.0f);
		mMusicToggle->layout->offsetMin = Vec2F(241.0f, 105.0f);
		mMusicToggle->layout->offsetMax = Vec2F(364.0f, 164.0f);
		mMusicToggle->SetValue(!mAudio || mAudio->IsMusicEnabled());
		mMusicToggle->SetStateForcible("value", mMusicToggle->GetValue());

		mMusicToggle->onToggleByUser = [this](bool value)
		{
			if (mAudio)
				mAudio->SetMusicEnabled(value);
			PlayClick();
		};

		auto accept = o2UI.CreateWidget<Button>("accept");
		mSettingsWindow->AddChild(accept);
		accept->layout->anchorMin = Vec2F(0.5f, 0.0f);
		accept->layout->anchorMax = Vec2F(0.5f, 0.0f);
		accept->layout->offsetMin = Vec2F(-75.0f, -44.0f);
		accept->layout->offsetMax = Vec2F(75.0f, 40.0f);
		accept->onClick = [this]() { PlayClick(); HideWindows(); };

		mSettingsWindow->SetEnabled(false);
	}

	void GameHUDComponent::BindLevel(GameSession* session, const Vector<Ref<Actor>>& officeAnchors,
							const Ref<Actor>& sourceAnchor)
	{
		mSession = session;
		mSourceAnchor = sourceAnchor;
		ClearVfx();

		for (auto& tip : mTooltips)
		{
			if (tip.widget)
				tip.widget->RemoveFromScene();
		}
		mTooltips.Clear();

		auto& orders = session->GetCity().orders;
		for (int i = 0; i < orders.Count(); i++)
		{
			OrderTooltip tip;
			if (i >= officeAnchors.Count() || !officeAnchors[i])
			{
				mTooltips.Add(tip);
				continue;
			}

			Vec3F anchorPos = officeAnchors[i]->transform->GetWorldPosition();
			tip.target = Vec2F(anchorPos.x + 1.0f, anchorPos.y + 47.0f);
			tip.offsetMin = Vec2F(anchorPos.x - 99.0f, anchorPos.y);
			tip.offsetMax = Vec2F(anchorPos.x + 101.0f, anchorPos.y + 94.0f);

			// white speech bubble, chip baked in; the tail tip points at the anchor
			tip.widget = mmake<Widget>();
			tip.widget->SetName("order tooltip");
			tip.widget->SetLayer(kWorldLayer);
			tip.widget->AddLayer("back", mmake<Sprite>(String("Game/UI/token_tooltip.png")));
			tip.widget->layout->anchorMin = Vec2F(0.0f, 0.0f);
			tip.widget->layout->anchorMax = Vec2F(0.0f, 0.0f);
			tip.widget->layout->offsetMin = tip.offsetMin;
			tip.widget->layout->offsetMax = tip.offsetMax;

			// centered in the white body right of the baked chip
			tip.amount = MakeLabel(tip.widget, WString((String)orders[i].amount), 34, "dark");
			tip.amount->layout->anchorMin = Vec2F(0.0f, 0.0f);
			tip.amount->layout->anchorMax = Vec2F(1.0f, 1.0f);
			tip.amount->layout->offsetMin = Vec2F(80.0f, 22.0f);
			tip.amount->layout->offsetMax = Vec2F(-8.0f, -7.0f);

			tip.widget->SetDrawingDepth(500.0f + i);
			mTooltips.Add(tip);
		}
	}

	bool GameHUDComponent::HasVisibleTooltips() const
	{
		for (auto& tip : mTooltips)
		{
			if (tip.widget && tip.widget->IsEnabled())
				return true;
		}
		return false;
	}

	void GameHUDComponent::Update(float dt)
	{
		if (!mSession)
			return;

		mPulsePhase += dt;
		mTurnLeftTap = Math::Max(0.0f, mTurnLeftTap - dt);
		mTurnRightTap = Math::Max(0.0f, mTurnRightTap - dt);

		mTokensLabel->SetText(WString((String)mSession->GetTokens()));

		// segments disappear one by one; the looped blink state owns the transparency of
		// the last one while it lasts
		float fuelFraction = Math::Clamp01(mSession->GetFuelFraction());
		int segmentsLeft = Math::Min(6, (int)Math::Ceil(fuelFraction*6.0f));
		bool lastBlinks = segmentsLeft == 1;
		for (int i = 0; i < mFuelSegments.Count(); i++)
		{
			if (i == 0 && lastBlinks)
				continue;
			mFuelSegments[i]->SetTransparency(i < segmentsLeft ? 1.0f : 0.0f);
		}
		mFuelSegments[0]->SetState("blink", lastBlinks);

		for (int i = 0; i < mTooltips.Count(); i++)
		{
			auto& tip = mTooltips[i];
			if (!tip.widget)
				continue;

			bool completed = mSession->IsOrderCompleted(i);
			tip.linger = Math::Max(0.0f, tip.linger - dt);
			if (completed && tip.linger <= 0.0f && tip.exit < 0.0f && tip.widget->IsEnabled())
				tip.exit = 0.0f; // tokens have landed, play the bubble out
			if (tip.exit < 0.0f)
				tip.widget->SetEnabled(!completed || tip.linger > 0.0f);

			if (tip.amount)
			{
				// a delivered order is paid: spending its tokens must not paint it red
				bool enough = completed || mSession->GetTokens() >= mSession->GetCity().orders[i].amount;
				tip.amount->SetColor(enough ? kAmountColor : kAmountShortColor);
			}

			float scale = completed ? 1.0f : 1.0f + 0.05f*Math::Sin(mPulsePhase*2.5f + i);
			if (tip.bounce >= 0.0f)
			{
				tip.bounce += dt;
				float u = tip.bounce/0.3f;
				if (u >= 1.0f)
					tip.bounce = -1.0f;
				else
					scale *= 1.0f + 0.28f*Math::Sin(Math::PI()*u);
			}

			if (tip.exit >= 0.0f)
				scale *= UpdateTooltipExit(tip, dt);

			tip.widget->transform->SetScale(Vec3F(scale, scale, 1.0f));
		}

		// token stream from the source into the car bed while refilling
		if (mSession->GetState() == SessionState::Playing && mSession->IsFilling() && mSourceAnchor)
		{
			mFillSpawnAccum += dt;
			while (mFillSpawnAccum >= tokenStreamInterval)
			{
				mFillSpawnAccum -= tokenStreamInterval;
				SpawnTokenToCar();
			}
		}
		else
			mFillSpawnAccum = 0.0f;

		UpdateVfx(dt);
		UpdateNavArrow();

		// the task panel slide is its "visible" state animation, only the hold timer is here
		if (mTaskTimer >= 0.0f)
		{
			mTaskTimer += dt;
			if (mTaskTimer >= taskPanelHoldTime)
			{
				mTaskPanel->SetEnabled(false); // plays the slide-out and disables itself
				mTaskTimer = -1.0f;
			}
		}
	}

	void GameHUDComponent::ShowOrderCompleted(int orderIndex)
	{
		auto& orders = mSession->GetCity().orders;
		if (orderIndex >= 0 && orderIndex < orders.Count())
			mTaskLabel->SetText(WString(String("Delivery office\n") + orders[orderIndex].name));

		mTaskPanel->SetEnabled(true);
		mTaskTimer = 0.0f;

		// tokens fly from the car bed into the delivery tooltip
		if (orderIndex >= 0 && orderIndex < mTooltips.Count() && mTooltips[orderIndex].widget)
		{
			mTooltips[orderIndex].linger = 1.3f;
			Vec2F bed = CarBedPos();
			for (int k = 0; k < 6; k++)
			{
				TokenFlyer flyer;
				flyer.toCar = false;
				flyer.fromWorld = bed + Vec2F((float)Math::Random(-22, 22),
											  (float)Math::Random(-6, 22));
				flyer.target = mTooltips[orderIndex].target;
				flyer.tooltipIndex = orderIndex;
				flyer.duration = 0.45f + k*0.08f;
				flyer.sizeJitter = 0.82f + Math::Random(0, 100)/100.0f*0.36f;
				flyer.tilt = (float)Math::Random(-26, 26);
				mFlyers.Add(flyer);
			}
		}
	}

	float GameHUDComponent::UpdateTooltipExit(OrderTooltip& tip, float dt)
	{
		const float hopTime = 0.18f, dropTime = 0.26f;
		const float hopHeight = 22.0f, dropDepth = 60.0f;

		tip.exit += dt;
		if (tip.exit >= hopTime + dropTime)
		{
			tip.exit = -1.0f;
			tip.widget->SetEnabled(false);
			tip.widget->SetTransparency(1.0f);
			tip.widget->layout->offsetMin = tip.offsetMin;
			tip.widget->layout->offsetMax = tip.offsetMax;
			return 1.0f;
		}

		float shift, scale, alpha;
		if (tip.exit < hopTime)
		{
			float u = tip.exit/hopTime;
			shift = hopHeight*(1.0f - (1.0f - u)*(1.0f - u)); // eases out at the top
			scale = 1.0f + 0.12f*u;
			alpha = 1.0f;
		}
		else
		{
			float u = (tip.exit - hopTime)/dropTime;
			shift = hopHeight - dropDepth*u*u; // falls back faster than it rose
			scale = 1.12f - 0.32f*u;
			alpha = 1.0f - u;
		}

		tip.widget->SetTransparency(alpha);
		tip.widget->layout->offsetMin = tip.offsetMin + Vec2F(0.0f, shift);
		tip.widget->layout->offsetMax = tip.offsetMax + Vec2F(0.0f, shift);
		return scale;
	}

	Vec2F GameHUDComponent::CarBedPos() const
	{
		auto& car = mSession->GetCar();
		float rad = Math::Deg2rad(car.GetVisualAngle());
		Vec2F back(-Math::Cos(rad)*0.22f, -Math::Sin(rad)*0.22f); // bed sits behind the cab
		return CellToScreen(car.GetVisualPos() + back) + Vec2F(0.0f, 34.0f);
	}

	void GameHUDComponent::SpawnTokenToCar()
	{
		Vec3F source = mSourceAnchor->transform->GetWorldPosition();
		TokenFlyer flyer;
		flyer.fromWorld = Vec2F(source.x + (float)Math::Random(-24, 24),
								source.y + (float)Math::Random(-14, 14));
		flyer.target = CarBedPos();
		flyer.duration = 0.45f;
		flyer.sizeJitter = 0.82f + Math::Random(0, 100)/100.0f*0.36f;
		flyer.tilt = (float)Math::Random(-26, 26);
		mFlyers.Add(flyer);
	}

	void GameHUDComponent::UpdateNavArrow()
	{
		if (!mNavActor)
			return;

		bool visible = false;
		Vec2F carUI, direction;
		if (mSession->GetState() == SessionState::Playing && worldToUI)
		{
			Vec2F targetWorld;
			bool hasTarget = false;

			int order = mSession->GetAffordableOrderTarget();
			if (order >= 0 && order < mTooltips.Count() && mTooltips[order].widget)
			{
				targetWorld = mTooltips[order].target;

				// this close the car is about to deliver anyway, the arrow is just noise
				float cells = kNavArrowHideCells;
				for (auto& cell : mSession->GetCity().orders[order].deliveryCells)
				{
					cells = Math::Min(cells, (Vec2F((float)cell.x, (float)cell.y)
											  - mSession->GetCar().GetPos()).Length());
				}
				hasTarget = cells >= kNavArrowHideCells;
			}
			else if (mSourceAnchor)
			{
				Vec3F source = mSourceAnchor->transform->GetWorldPosition();
				targetWorld = Vec2F(source.x, source.y);
				hasTarget = !mSession->IsFilling(); // already loading, no need to point there
			}

			if (hasTarget)
			{
				carUI = worldToUI(CellToScreen(mSession->GetCar().GetVisualPos()));
				direction = worldToUI(targetWorld) - carUI;
				visible = direction.Length() > 1.0f;
			}
		}

		mNavActor->SetEnabled(visible);
		if (!visible)
			return;

		direction = direction.Normalized();
		Vec2F pos = carUI + direction*kNavArrowOrbit;
		mNavActor->transform->SetPosition(Vec3F(pos.x, pos.y, 0.0f));
		mNavActor->transform->SetAngleDegrees(Math::Rad2deg(Math::Atan2F(direction.y, direction.x)));
	}

	void GameHUDComponent::DrawNavArrow()
	{
		if (!mNavArrowSprite)
			return;

		float scale = kNavArrowSize/Math::Max(1.0f, (float)mNavArrowSprite->GetOriginalSize().x);
		Vec3F pos = mNavActor->transform->GetPosition();
		mNavArrowSprite->SetPosition(Vec2F(pos.x, pos.y));
		mNavArrowSprite->SetScale(Vec2F(scale, scale));
		mNavArrowSprite->SetAngleDegrees(mNavActor->transform->GetAngleDegrees());
		mNavArrowSprite->SetTransparency(0.8f + 0.2f*Math::Sin(mPulsePhase*4.0f)); // gentle pulse
		mNavArrowSprite->Draw();
	}

	void GameHUDComponent::BurstSparks(const Vec2F& pos)
	{
		mSparkEmitter->GetActor()->transform->SetWorldPosition(Vec3F(pos.x, pos.y, 0.0f));
		mSparkEmitter->SetTime(0.0f);
		mSparkEmitter->Play();
	}

	void GameHUDComponent::UpdateVfx(float dt)
	{
		for (int i = mFlyers.Count() - 1; i >= 0; i--)
		{
			auto& flyer = mFlyers[i];
			flyer.age += dt;
			float u = Math::Clamp01(flyer.age/flyer.duration);
			float eased = u*u*(1.4f - 0.4f*u); // eases out of the source, accelerates into the target

			if (flyer.toCar)
				flyer.target = CarBedPos(); // the bed keeps moving while the token flies
			flyer.drawPos = Math::Lerp(flyer.fromWorld, flyer.target, eased);
			flyer.drawPos.y += 60.0f*Math::Sin(Math::PI()*eased); // slight arc over the flight

			// swells at mid flight, then shrinks into the target
			float swell = 1.0f + 0.35f*Math::Sin(Math::PI()*u);
			flyer.drawScale = 36.0f*flyer.sizeJitter*swell*(1.0f - 0.5f*eased);
			flyer.drawAngle = flyer.tilt*(1.0f - 0.4f*eased);

			if (flyer.age < flyer.duration)
				continue;

			// arrival: sparks + credit tick + bounce on the target
			BurstSparks(flyer.target);
			if (mAudio)
				mAudio->PlayChips();
			if (!flyer.toCar && flyer.tooltipIndex >= 0 && flyer.tooltipIndex < mTooltips.Count())
				mTooltips[flyer.tooltipIndex].bounce = 0.0f;

			mFlyers.RemoveAt(i);
		}
	}

	void GameHUDComponent::DrawVfx()
	{
		if (!mChipSprite)
			return;

		for (auto& flyer : mFlyers)
		{
			if (flyer.age <= 0.0f)
				continue;

			float scale = flyer.drawScale/mChipNativeW;
			mChipSprite->SetPosition(flyer.drawPos);
			mChipSprite->SetScale(Vec2F(scale, scale));
			mChipSprite->SetAngleDegrees(flyer.drawAngle);
			mChipSprite->Draw();
		}
	}

	void GameHUDComponent::ClearVfx()
	{
		mFlyers.Clear();
		if (mSparkEmitter)
			mSparkEmitter->Stop();
	}

	void GameHUDComponent::SetSettingsEnabled(bool enabled)
	{
		if (mSettingsButton && mSettingsButton->IsEnabled() != enabled)
			mSettingsButton->SetEnabled(enabled);
	}

	bool GameHUDComponent::IsTurningLeft() const
	{
		return mTurnLeftTap > 0.0f || (mTurnLeftButton && mTurnLeftButton->GetState("pressed"));
	}

	bool GameHUDComponent::IsTurningRight() const
	{
		return mTurnRightTap > 0.0f || (mTurnRightButton && mTurnRightButton->GetState("pressed"));
	}

	void GameHUDComponent::ShowWin()
	{
		mDimmer->SetEnabled(true);
		mWinWindow->SetEnabled(true);
	}

	void GameHUDComponent::ShowLose()
	{
		mDimmer->SetEnabled(true);
		mLoseWindow->SetEnabled(true);
	}

	void GameHUDComponent::ShowSettings()
	{
		mDimmer->SetEnabled(true);
		mSettingsWindow->SetEnabled(true);
	}

	bool GameHUDComponent::IsSettingsOpen() const
	{
		return mSettingsWindow && mSettingsWindow->IsEnabled();
	}

	void GameHUDComponent::PlayClick()
	{
		if (mAudio)
			mAudio->PlayButton();
	}

	void GameHUDComponent::HideWindows()
	{
		if (mDimmer)
			mDimmer->SetEnabled(false);
		if (mWinWindow)
			mWinWindow->SetEnabled(false);
		if (mLoseWindow)
			mLoseWindow->SetEnabled(false);
		if (mSettingsWindow)
			mSettingsWindow->SetEnabled(false);
	}

	void GameHUDComponent::Clear()
	{
		ClearVfx();

		for (auto& tip : mTooltips)
		{
			if (tip.widget)
				tip.widget->RemoveFromScene();
		}
		mTooltips.Clear();

		if (mVfxActor)
			mVfxActor->RemoveFromScene();
		mVfxActor = nullptr;
		if (mSparkEmitter)
			mSparkEmitter->GetActor()->RemoveFromScene();
		mSparkEmitter = nullptr;

		if (mRoot)
			mRoot->RemoveFromScene();
		mRoot = nullptr;
		mSettingsButton = nullptr;
		if (mNavActor)
			mNavActor->RemoveFromScene();
		mNavActor = nullptr;
		mNavArrowSprite = nullptr;
		mTurnLeftButton = nullptr;
		mTurnRightButton = nullptr;
		mTurnLeftTap = 0.0f;
		mTurnRightTap = 0.0f;
		mSourceAnchor = nullptr;
		mSession = nullptr;
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<td::GameHUDComponent>);
// --- META ---

DECLARE_CLASS(td::GameHUDComponent, td__GameHUDComponent);
// --- END META ---
