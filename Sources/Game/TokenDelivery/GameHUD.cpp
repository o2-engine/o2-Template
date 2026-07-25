#include "o2/stdafx.h"
#include "TokenDelivery/GameHUD.h"

#include "TokenDelivery/CityViewBuilder.h"
#include "TokenDelivery/GameUIStyle.h"
#include "o2/Render/FontStyle.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Render/VectorFontEffects.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/UIManager.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"

namespace td
{
	// task panel slide-in x offsets (root-relative), see Update()
	static const float kTaskPanelHiddenX = -380.0f;
	static const float kTaskPanelShownX = 0.0f; // flush with the screen edge
	static const float kTaskPanelWidth = 350.0f;

	Ref<Label> GameHUD::MakeLabel(const Ref<Widget>& parent, const WString& text, int height,
								  const String& style)
	{
		auto label = o2UI.CreateLabel(text, style);
		if (auto drawable = label->GetLayerDrawable<Text>("text"))
		{
			// widget style cloning resets the Text drawable (font, color, aligns, font
			// style) to engine defaults — reapply everything here
			drawable->SetFontAsset(GameUIFont());
			drawable->SetHeight(height);
			drawable->SetHorAlign(HorAlign::Middle);
			drawable->SetVerAlign(VerAlign::Middle);
			drawable->SetColor(style == "dark" ? Color4(34, 41, 65, 255)
							 : style == "quest" ? Color4(248, 240, 216, 255)
							 : Color4(255, 255, 255, 255));
			if (style == "quest")
			{
				auto fontStyle = mmake<FontStyle>();
				fontStyle->AddEffect<FontStrokeEffect>(2.5f, Color4(44, 58, 82, 220), 100);
				drawable->SetFontStyle(fontStyle);
			}
			else if (style == "standard")
			{
				auto fontStyle = mmake<FontStyle>();
				fontStyle->AddEffect<FontShadowEffect>(2.0f, Vec2I(1, -2), Color4(30, 40, 60, 90));
				drawable->SetFontStyle(fontStyle);
			}
		}
		parent->AddChild(label);
		return label;
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

		// six fuel segments in the panel slot; the rightmost drains first
		mFuelSegments.Clear();
		const float segLeft = 63.0f, segRight = 226.0f, segBottom = 19.0f, segTop = 45.0f;
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
		auto settings = o2UI.CreateWidget<Button>("settings");
		mRoot->AddChild(settings);
		settings->layout->anchorMin = Vec2F(1.0f, 1.0f);
		settings->layout->anchorMax = Vec2F(1.0f, 1.0f);
		settings->layout->offsetMin = Vec2F(-88.0f, -88.0f);
		settings->layout->offsetMax = Vec2F(-24.0f, -24.0f);
		settings->onClick = [this]() { ShowSettings(); };

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

		mWinWindow = MakeResultWindow("Game/UI/wnd_success_bg.png",
									  Vec2F(-231.0f, -123.0f), Vec2F(232.0f, 187.0f),
									  L"MISSION COMPLETE!", 170.0f,
									  L"All orders have been\nsuccessfully delivered.", 100.0f,
									  L"CONTINUE", [this]() { onNextLevel(); });
		mLoseWindow = MakeResultWindow("Game/UI/wnd_loose_bg.png",
									   Vec2F(-231.0f, -121.0f), Vec2F(231.0f, 194.0f),
									   L"OUT OF FUEL", 158.0f,
									   L"Your delivery truck has run out of fuel.\nRefill and try again!", 96.0f,
									   L"TRY AGAIN", [this]() { onRetry(); });
		BuildSettingsWindow();
	}

	Ref<Widget> GameHUD::MakeResultWindow(const String& bgSprite, const Vec2F& offMin, const Vec2F& offMax,
										  const WString& title, float titleY,
										  const WString& message, float messageY,
										  const WString& buttonCaption, const Function<void()>& onClick)
	{
		auto window = mmake<Widget>();
		window->SetName("window");
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

	void GameHUD::BindLevel(GameSession* session, const Vector<Ref<Actor>>& officeAnchors)
	{
		mSession = session;

		for (auto& tooltip : mTooltips)
		{
			if (tooltip)
				tooltip->RemoveFromScene();
		}
		mTooltips.Clear();

		auto& orders = session->GetCity().orders;
		for (int i = 0; i < orders.Count(); i++)
		{
			if (i >= officeAnchors.Count() || !officeAnchors[i])
			{
				mTooltips.Add(nullptr);
				continue;
			}

			Vec3F anchorPos = officeAnchors[i]->transform->GetWorldPosition();

			// white speech bubble, chip baked in; the tail tip points at the anchor
			auto tooltip = mmake<Widget>();
			tooltip->SetName("order tooltip");
			tooltip->SetLayer(kWorldLayer);
			tooltip->AddLayer("back", mmake<Sprite>(String("Game/UI/token_tooltip.png")));
			tooltip->layout->anchorMin = Vec2F(0.0f, 0.0f);
			tooltip->layout->anchorMax = Vec2F(0.0f, 0.0f);
			tooltip->layout->offsetMin = Vec2F(anchorPos.x - 99.0f, anchorPos.y);
			tooltip->layout->offsetMax = Vec2F(anchorPos.x + 101.0f, anchorPos.y + 94.0f);

			// centered in the white body right of the baked chip
			auto amount = MakeLabel(tooltip, WString((String)orders[i].amount), 34, "dark");
			amount->layout->anchorMin = Vec2F(0.0f, 0.0f);
			amount->layout->anchorMax = Vec2F(1.0f, 1.0f);
			amount->layout->offsetMin = Vec2F(80.0f, 22.0f);
			amount->layout->offsetMax = Vec2F(-8.0f, -7.0f);

			tooltip->SetDrawingDepth(500.0f + i);
			mTooltips.Add(tooltip);
		}
	}

	void GameHUD::Update(float dt)
	{
		if (!mSession)
			return;

		mPulsePhase += dt;

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
			if (!mTooltips[i])
				continue;

			bool completed = mSession->IsOrderCompleted(i);
			mTooltips[i]->SetEnabled(!completed);
			if (!completed)
			{
				float scale = 1.0f + 0.05f*Math::Sin(mPulsePhase*2.5f + i);
				mTooltips[i]->transform->SetScale(Vec3F(scale, scale, 1.0f));
			}
		}

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
		for (auto& tooltip : mTooltips)
		{
			if (tooltip)
				tooltip->RemoveFromScene();
		}
		mTooltips.Clear();

		if (mRoot)
			mRoot->RemoveFromScene();
		mRoot = nullptr;
		mSession = nullptr;
	}
}
