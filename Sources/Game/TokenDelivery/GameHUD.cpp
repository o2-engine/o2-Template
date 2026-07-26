#include "o2/stdafx.h"
#include "TokenDelivery/GameHUD.h"

#include "TokenDelivery/ArtSprites.h"
#include "TokenDelivery/CityViewBuilder.h"
#include "TokenDelivery/GameUIStyle.h"
#include "o2/Assets/Assets.h"
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
	static const float kTurnTapTime = 0.35f; // a tap steers for this long, a hold — while held

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

	// manual-draw host for the flying tokens; the flyers are plain data in GameHUD —
	// actors must not be spawned mid scene update
	class TokenVfxComponent: public Component
	{
	public:
		Function<void()> onDraw;

		TokenVfxComponent(): TokenVfxComponent(nullptr) {}
		explicit TokenVfxComponent(RefCounter* refCounter): Component(refCounter) {}

		SERIALIZABLE(TokenVfxComponent);
		CLONEABLE_REF(TokenVfxComponent);

	private:
		void OnDraw() override { onDraw(); }

		REF_COUNTERABLE_IMPL(Component);
	};

	// one-shot radial burst of additive sparks, restarted at every token hit
	static Ref<ParticlesEmitterComponent> MakeSparkEmitter(const String& layer, float depth,
														   float particleScale)
	{
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

	Ref<Label> GameHUD::MakeLabel(const Ref<Widget>& parent, const WString& text, int height,
								  const String& style)
	{
		return MakeGameLabel(parent, text, height, style);
	}

	void GameHUD::Build()
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

		// settings, top-right
		mSettingsButton = o2UI.CreateWidget<Button>("settings");
		mRoot->AddChild(mSettingsButton);
		mSettingsButton->layout->anchorMin = Vec2F(1.0f, 1.0f);
		mSettingsButton->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mSettingsButton->layout->offsetMin = Vec2F(-88.0f, -88.0f);
		mSettingsButton->layout->offsetMax = Vec2F(-24.0f, -24.0f);
		mSettingsButton->onClick = [this]() { ShowSettings(); };

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
			button->onClick = onTap;
			return button;
		};
		mTurnLeftButton = makeTurnButton("turn left", -156.0f,
										 [this]() { mTurnLeftTap = kTurnTapTime; });
		mTurnRightButton = makeTurnButton("turn right", -36.0f,
										  [this]() { mTurnRightTap = kTurnTapTime; });

		// completed task panel, slides in from the left edge on order delivery
		mTaskPanel = mmake<Widget>();
		mTaskPanel->SetName("task panel");
		mRoot->AddChild(mTaskPanel);
		// only the panel back is translucent, text and check stay solid
		auto taskBack = mTaskPanel->AddLayer("back", mmake<Sprite>(String("Game/UI/task_panel.png")));
		taskBack->SetTransparency(0.82f);
		mTaskPanel->layout->anchorMin = Vec2F(0.0f, 1.0f);
		mTaskPanel->layout->anchorMax = Vec2F(0.0f, 1.0f);
		mTaskPanel->layout->offsetMin = Vec2F(kTaskPanelHiddenX, -305.0f);
		mTaskPanel->layout->offsetMax = Vec2F(kTaskPanelHiddenX + kTaskPanelWidth, -120.0f);

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
		mTaskPanel->SetEnabled(false);

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
									  L"CONTINUE", [this]() { onNextLevel(); });
		mLoseWindow = MakeResultWindow("lose window", "Game/UI/wnd_loose_bg.png",
									   Vec2F(-231.0f, -121.0f), Vec2F(231.0f, 194.0f),
									   L"OUT OF FUEL", 158.0f,
									   L"Your delivery truck has run out of fuel.\nRefill and try again!", 96.0f,
									   L"TRY AGAIN", [this]() { onRetry(); });
		BuildSettingsWindow();
	}

	Ref<Widget> GameHUD::MakeResultWindow(const String& name, const String& bgSprite,
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

	void GameHUD::BuildSettingsWindow()
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
		mSoundToggle->SetValue(true);
		mSoundToggle->SetStateForcible("value", true);

		mMusicToggle = o2UI.CreateWidget<Toggle>("switch");
		mSettingsWindow->AddChild(mMusicToggle);
		mMusicToggle->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mMusicToggle->layout->anchorMax = Vec2F(0.0f, 0.0f);
		mMusicToggle->layout->offsetMin = Vec2F(241.0f, 105.0f);
		mMusicToggle->layout->offsetMax = Vec2F(364.0f, 164.0f);
		mMusicToggle->SetValue(true);
		mMusicToggle->SetStateForcible("value", true);

		auto accept = o2UI.CreateWidget<Button>("accept");
		mSettingsWindow->AddChild(accept);
		accept->layout->anchorMin = Vec2F(0.5f, 0.0f);
		accept->layout->anchorMax = Vec2F(0.5f, 0.0f);
		accept->layout->offsetMin = Vec2F(-75.0f, -44.0f);
		accept->layout->offsetMax = Vec2F(75.0f, 40.0f);
		accept->onClick = [this]() { HideWindows(); };

		mSettingsWindow->SetEnabled(false);
	}

	void GameHUD::BindLevel(GameSession* session, const Vector<Ref<Actor>>& officeAnchors,
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

	bool GameHUD::HasVisibleTooltips() const
	{
		for (auto& tip : mTooltips)
		{
			if (tip.widget && tip.widget->IsEnabled())
				return true;
		}
		return false;
	}

	void GameHUD::Update(float dt)
	{
		if (!mSession)
			return;

		mPulsePhase += dt;
		mTurnLeftTap = Math::Max(0.0f, mTurnLeftTap - dt);
		mTurnRightTap = Math::Max(0.0f, mTurnRightTap - dt);

		mTokensLabel->SetText(WString((String)mSession->GetTokens()));

		// segments disappear one by one; the last one blinks while it lasts
		float fuelFraction = Math::Clamp01(mSession->GetFuelFraction());
		int segmentsLeft = Math::Min(6, (int)Math::Ceil(fuelFraction*6.0f));
		for (int i = 0; i < mFuelSegments.Count(); i++)
			mFuelSegments[i]->SetTransparency(i < segmentsLeft ? 1.0f : 0.0f);
		if (segmentsLeft == 1)
		{
			mLowFuelBlink += dt*6.0f;
			mFuelSegments[0]->SetTransparency(0.4f + 0.6f*(0.5f + 0.5f*Math::Sin(mLowFuelBlink)));
		}

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
			while (mFillSpawnAccum >= 0.12f)
			{
				mFillSpawnAccum -= 0.12f;
				SpawnTokenToCar();
			}
		}
		else
			mFillSpawnAccum = 0.0f;

		UpdateVfx(dt);

		// task panel slide in/out
		if (mTaskTimer >= 0.0f)
		{
			mTaskTimer += dt;
			float x;
			if (mTaskTimer < 0.3f)
				x = Math::Lerp(kTaskPanelHiddenX, kTaskPanelShownX, mTaskTimer/0.3f);
			else if (mTaskTimer < 2.2f)
				x = kTaskPanelShownX;
			else if (mTaskTimer < 2.5f)
				x = Math::Lerp(kTaskPanelShownX, kTaskPanelHiddenX, (mTaskTimer - 2.2f)/0.3f);
			else
			{
				mTaskPanel->SetEnabled(false);
				mTaskTimer = -1.0f;
				x = kTaskPanelHiddenX;
			}
			mTaskPanel->layout->offsetMin = Vec2F(x, -305.0f);
			mTaskPanel->layout->offsetMax = Vec2F(x + kTaskPanelWidth, -120.0f);
		}
	}

	void GameHUD::ShowOrderCompleted(int orderIndex)
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

	float GameHUD::UpdateTooltipExit(OrderTooltip& tip, float dt)
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

	Vec2F GameHUD::CarBedPos() const
	{
		auto& car = mSession->GetCar();
		float rad = Math::Deg2rad(car.GetVisualAngle());
		Vec2F back(-Math::Cos(rad)*0.22f, -Math::Sin(rad)*0.22f); // bed sits behind the cab
		return CellToScreen(car.GetVisualPos() + back) + Vec2F(0.0f, 34.0f);
	}

	void GameHUD::SpawnTokenToCar()
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

	void GameHUD::BurstSparks(const Vec2F& pos)
	{
		mSparkEmitter->GetActor()->transform->SetWorldPosition(Vec3F(pos.x, pos.y, 0.0f));
		mSparkEmitter->SetTime(0.0f);
		mSparkEmitter->Play();
	}

	void GameHUD::UpdateVfx(float dt)
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

			// arrival: sparks + bounce on the target
			BurstSparks(flyer.target);
			if (!flyer.toCar && flyer.tooltipIndex >= 0 && flyer.tooltipIndex < mTooltips.Count())
				mTooltips[flyer.tooltipIndex].bounce = 0.0f;

			mFlyers.RemoveAt(i);
		}
	}

	void GameHUD::DrawVfx()
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

	void GameHUD::ClearVfx()
	{
		mFlyers.Clear();
		if (mSparkEmitter)
			mSparkEmitter->Stop();
	}

	void GameHUD::SetSettingsEnabled(bool enabled)
	{
		if (mSettingsButton && mSettingsButton->IsEnabled() != enabled)
			mSettingsButton->SetEnabled(enabled);
	}

	bool GameHUD::IsTurningLeft() const
	{
		return mTurnLeftTap > 0.0f || (mTurnLeftButton && mTurnLeftButton->GetState("pressed"));
	}

	bool GameHUD::IsTurningRight() const
	{
		return mTurnRightTap > 0.0f || (mTurnRightButton && mTurnRightButton->GetState("pressed"));
	}

	void GameHUD::ShowWin()
	{
		mDimmer->SetEnabled(true);
		mWinWindow->SetEnabled(true);
	}

	void GameHUD::ShowLose()
	{
		mDimmer->SetEnabled(true);
		mLoseWindow->SetEnabled(true);
	}

	void GameHUD::ShowSettings()
	{
		mDimmer->SetEnabled(true);
		mSettingsWindow->SetEnabled(true);
	}

	void GameHUD::HideWindows()
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

	void GameHUD::Clear()
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
		mTurnLeftButton = nullptr;
		mTurnRightButton = nullptr;
		mTurnLeftTap = 0.0f;
		mTurnRightTap = 0.0f;
		mSourceAnchor = nullptr;
		mSession = nullptr;
	}
}
// --- META ---

CLASS_BASES_META(td::TokenVfxComponent)
{
    BASE_CLASS(o2::Component);
}
END_META;
CLASS_FIELDS_META(td::TokenVfxComponent)
{
}
END_META;
CLASS_METHODS_META(td::TokenVfxComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
}
END_META;

DECLARE_CLASS(td::TokenVfxComponent, td__TokenVfxComponent);
// --- END META ---
