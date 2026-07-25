#include "o2/stdafx.h"
#include "TokenDelivery/GameHUD.h"

#include "TokenDelivery/CityViewBuilder.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/UIManager.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"

namespace td
{
	Ref<Label> GameHUD::MakeLabel(const Ref<Widget>& parent, const WString& text, int height,
								  bool dark)
	{
		auto label = o2UI.CreateLabel(text);
		if (auto drawable = label->GetLayerDrawable<Text>("text"))
		{
			drawable->SetHeight(height);
			if (dark)
			{
				drawable->SetColor(Color4(58, 70, 94, 255));
				drawable->SetFontStyle(nullptr);
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

		// token counter pill, top-left; the big chip icon overlaps its left edge
		auto pill = o2UI.CreateImage("Game/UI/pill.png");
		mRoot->AddChild(pill);
		pill->layout->anchorMin = Vec2F(0.0f, 1.0f);
		pill->layout->anchorMax = Vec2F(0.0f, 1.0f);
		pill->layout->offsetMin = Vec2F(42.0f, -92.0f);
		pill->layout->offsetMax = Vec2F(290.0f, -24.0f);

		auto chip = o2UI.CreateImage("Game/Props/chip.png");
		pill->AddChild(chip);
		chip->layout->anchorMin = Vec2F(0.0f, 0.5f);
		chip->layout->anchorMax = Vec2F(0.0f, 0.5f);
		chip->layout->offsetMin = Vec2F(-34.0f, -40.0f);
		chip->layout->offsetMax = Vec2F(46.0f, 40.0f);

		mTokensLabel = MakeLabel(pill, L"0", 36, true);
		mTokensLabel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mTokensLabel->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mTokensLabel->layout->offsetMin = Vec2F(56.0f, 0.0f);
		mTokensLabel->layout->offsetMax = Vec2F(-12.0f, 0.0f);

		// fuel bar, bottom-left: dark capsule image under the fill progress
		auto fuelBack = o2UI.CreateImage("Game/UI/fuel_bg.png");
		mRoot->AddChild(fuelBack);
		fuelBack->layout->anchorMin = Vec2F(0.0f, 0.0f);
		fuelBack->layout->anchorMax = Vec2F(0.0f, 0.0f);
		fuelBack->layout->offsetMin = Vec2F(76.0f, 17.0f);
		fuelBack->layout->offsetMax = Vec2F(429.0f, 96.0f);

		mFuelBar = o2UI.CreateHorProgress("fuel");
		mRoot->AddChild(mFuelBar);
		mFuelBar->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mFuelBar->layout->anchorMax = Vec2F(0.0f, 0.0f);
		mFuelBar->layout->offsetMin = Vec2F(85.0f, 25.0f);
		mFuelBar->layout->offsetMax = Vec2F(420.0f, 88.0f);
		mFuelBar->SetInteractable(false);
		mFuelBar->SetValueForcible(1.0f);

		auto fuelIcon = o2UI.CreateImage("Game/UI/fuel_icon.png");
		mRoot->AddChild(fuelIcon);
		fuelIcon->layout->anchorMin = Vec2F(0.0f, 0.0f);
		fuelIcon->layout->anchorMax = Vec2F(0.0f, 0.0f);
		fuelIcon->layout->offsetMin = Vec2F(20.0f, 22.0f);
		fuelIcon->layout->offsetMax = Vec2F(88.0f, 90.0f);

		// wide Acceleration Boost button (cut from the reference) + reserve bar, bottom-right
		mBoostButton = o2UI.CreateWidget<Button>("boost");
		mRoot->AddChild(mBoostButton);
		mBoostButton->layout->anchorMin = Vec2F(1.0f, 0.0f);
		mBoostButton->layout->anchorMax = Vec2F(1.0f, 0.0f);
		mBoostButton->layout->offsetMin = Vec2F(-372.0f, 44.0f);
		mBoostButton->layout->offsetMax = Vec2F(-32.0f, 173.0f);

		mBoostReserveBar = o2UI.CreateHorProgress("fuel");
		mRoot->AddChild(mBoostReserveBar);
		mBoostReserveBar->layout->anchorMin = Vec2F(1.0f, 0.0f);
		mBoostReserveBar->layout->anchorMax = Vec2F(1.0f, 0.0f);
		mBoostReserveBar->layout->offsetMin = Vec2F(-340.0f, 14.0f);
		mBoostReserveBar->layout->offsetMax = Vec2F(-64.0f, 42.0f);
		mBoostReserveBar->SetInteractable(false);
		mBoostReserveBar->SetValueForcible(1.0f);

		// touch arrows cross, bottom-center
		struct ArrowDef { Dir dir; const char* style; Vec2F center; };
		const ArrowDef arrows[] = {
			{ Dir::N, "arrow_n", Vec2F(0.0f, 160.0f) }, { Dir::S, "arrow_s", Vec2F(0.0f, 68.0f) },
			{ Dir::W, "arrow_w", Vec2F(-92.0f, 114.0f) }, { Dir::E, "arrow_e", Vec2F(92.0f, 114.0f) }
		};
		for (auto& def : arrows)
		{
			auto arrow = o2UI.CreateWidget<Button>(def.style);
			mRoot->AddChild(arrow);
			arrow->layout->anchorMin = Vec2F(0.5f, 0.0f);
			arrow->layout->anchorMax = Vec2F(0.5f, 0.0f);
			arrow->layout->offsetMin = def.center + Vec2F(-42.0f, -42.0f);
			arrow->layout->offsetMax = def.center + Vec2F(42.0f, 42.0f);
			mArrows[(int)def.dir] = arrow;
		}

		// gear (restart), top-right
		auto gear = o2UI.CreateWidget<Button>("gear");
		mRoot->AddChild(gear);
		gear->layout->anchorMin = Vec2F(1.0f, 1.0f);
		gear->layout->anchorMax = Vec2F(1.0f, 1.0f);
		gear->layout->offsetMin = Vec2F(-90.0f, -90.0f);
		gear->layout->offsetMax = Vec2F(-20.0f, -20.0f);
		gear->onClick = [this]() { onRetry(); };

		// order completed plash, slides in from the left edge
		mPlash = mmake<Widget>();
		mPlash->SetName("plash");
		mRoot->AddChild(mPlash);
		mPlash->AddLayer("back", mmake<Sprite>(String("Game/UI/pill.png")));
		mPlash->layout->anchorMin = Vec2F(0.0f, 0.5f);
		mPlash->layout->anchorMax = Vec2F(0.0f, 0.5f);
		mPlash->layout->offsetMin = Vec2F(-400.0f, 20.0f);
		mPlash->layout->offsetMax = Vec2F(-40.0f, 104.0f);

		auto plashCheck = o2UI.CreateImage("Game/UI/check.png");
		mPlash->AddChild(plashCheck);
		plashCheck->layout->anchorMin = Vec2F(0.0f, 0.5f);
		plashCheck->layout->anchorMax = Vec2F(0.0f, 0.5f);
		plashCheck->layout->offsetMin = Vec2F(12.0f, -28.0f);
		plashCheck->layout->offsetMax = Vec2F(68.0f, 28.0f);

		mPlashLabel = MakeLabel(mPlash, L"Order delivered!", 26, true);
		mPlashLabel->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mPlashLabel->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mPlashLabel->layout->offsetMin = Vec2F(72.0f, 0.0f);
		mPlashLabel->layout->offsetMax = Vec2F(-8.0f, 0.0f);
		mPlash->SetEnabled(false);

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

		mWinWindow = MakeWindow(L"All orders delivered!",
								L"The whole city got its AI tokens in time.",
								L"Next level", "green", [this]() { onNextLevel(); });
		mLoseWindow = MakeWindow(L"Out of fuel!",
								 L"Some offices are still waiting for tokens...",
								 L"Retry", "standard", [this]() { onRetry(); });
	}

	Ref<Widget> GameHUD::MakeWindow(const WString& title, const WString& message,
									const WString& buttonCaption, const String& buttonStyle,
									const Function<void()>& onClick)
	{
		auto window = mmake<Widget>();
		window->SetName("window");
		mRoot->AddChild(window);
		window->AddLayer("back", mmake<Sprite>(String("Game/UI/window.png")));
		window->layout->anchorMin = Vec2F(0.5f, 0.5f);
		window->layout->anchorMax = Vec2F(0.5f, 0.5f);
		window->layout->offsetMin = Vec2F(-320.0f, -210.0f);
		window->layout->offsetMax = Vec2F(320.0f, 210.0f);

		auto titleLabel = MakeLabel(window, title, 38, true);
		titleLabel->layout->anchorMin = Vec2F(0.0f, 1.0f);
		titleLabel->layout->anchorMax = Vec2F(1.0f, 1.0f);
		titleLabel->layout->offsetMin = Vec2F(20.0f, -120.0f);
		titleLabel->layout->offsetMax = Vec2F(-20.0f, -40.0f);

		auto messageLabel = MakeLabel(window, message, 24, true);
		messageLabel->layout->anchorMin = Vec2F(0.0f, 0.5f);
		messageLabel->layout->anchorMax = Vec2F(1.0f, 0.5f);
		messageLabel->layout->offsetMin = Vec2F(20.0f, -20.0f);
		messageLabel->layout->offsetMax = Vec2F(-20.0f, 60.0f);

		auto button = o2UI.CreateButton(buttonCaption, onClick, buttonStyle);
		window->AddChild(button);
		button->layout->anchorMin = Vec2F(0.5f, 0.0f);
		button->layout->anchorMax = Vec2F(0.5f, 0.0f);
		button->layout->offsetMin = Vec2F(-180.0f, 40.0f);
		button->layout->offsetMax = Vec2F(180.0f, 150.0f);

		window->SetEnabled(false);
		return window;
	}

	void GameHUD::BindLevel(GameSession* session, const Vector<Ref<Actor>>& officeAnchors)
	{
		mSession = session;

		if (mQuestPanel)
			mQuestPanel->RemoveFromScene();
		mQuestLabels.Clear();
		mQuestChecks.Clear();

		auto& orders = session->GetCity().orders;
		float panelHeight = 52.0f + orders.Count()*34.0f;

		mQuestPanel = mmake<Widget>();
		mQuestPanel->SetName("quests");
		mRoot->AddChild(mQuestPanel);
		mQuestPanel->AddLayer("back", mmake<Sprite>(String("Game/UI/panel_dark.png")));
		mQuestPanel->SetTransparency(0.92f);
		mQuestPanel->layout->anchorMin = Vec2F(0.0f, 1.0f);
		mQuestPanel->layout->anchorMax = Vec2F(0.0f, 1.0f);
		mQuestPanel->layout->offsetMin = Vec2F(20.0f, -108.0f - panelHeight);
		mQuestPanel->layout->offsetMax = Vec2F(310.0f, -108.0f);

		auto header = MakeLabel(mQuestPanel, L"Active Quests", 26);
		header->layout->anchorMin = Vec2F(0.0f, 1.0f);
		header->layout->anchorMax = Vec2F(1.0f, 1.0f);
		header->layout->offsetMin = Vec2F(10.0f, -46.0f);
		header->layout->offsetMax = Vec2F(-10.0f, -6.0f);

		for (int i = 0; i < orders.Count(); i++)
		{
			float y = -52.0f - i*38.0f;

			auto check = o2UI.CreateImage("Game/UI/check.png");
			mQuestPanel->AddChild(check);
			check->layout->anchorMin = Vec2F(0.0f, 1.0f);
			check->layout->anchorMax = Vec2F(0.0f, 1.0f);
			check->layout->offsetMin = Vec2F(12.0f, y - 15.0f);
			check->layout->offsetMax = Vec2F(42.0f, y + 15.0f);
			check->SetTransparency(0.25f);
			mQuestChecks.Add(check);

			String row = String("Office ") + orders[i].name + "  x" + (String)orders[i].amount;
			auto rowLabel = MakeLabel(mQuestPanel, WString(row), 19);
			rowLabel->layout->anchorMin = Vec2F(0.0f, 1.0f);
			rowLabel->layout->anchorMax = Vec2F(1.0f, 1.0f);
			rowLabel->layout->offsetMin = Vec2F(52.0f, y - 19.0f);
			rowLabel->layout->offsetMax = Vec2F(-8.0f, y + 19.0f);
			if (auto drawable = rowLabel->GetLayerDrawable<Text>("text"))
				drawable->SetHorAlign(HorAlign::Left);
			mQuestLabels.Add(rowLabel);
		}

		// world-space tooltips above offices
		for (auto& tooltip : mTooltips)
		{
			if (tooltip)
				tooltip->RemoveFromScene();
		}
		mTooltips.Clear();

		for (int i = 0; i < orders.Count(); i++)
		{
			if (i >= officeAnchors.Count() || !officeAnchors[i])
			{
				mTooltips.Add(nullptr);
				continue;
			}

			Vec3F anchorPos = officeAnchors[i]->transform->GetWorldPosition();

			// speech bubble cut from the reference: chip baked in, tail at the bottom
			auto tooltip = mmake<Widget>();
			tooltip->SetName("order tooltip");
			tooltip->SetLayer(kWorldLayer);
			tooltip->AddLayer("back", mmake<Sprite>(String("Game/UI/bubble.png")));
			tooltip->layout->anchorMin = Vec2F(0.0f, 0.0f);
			tooltip->layout->anchorMax = Vec2F(0.0f, 0.0f);
			tooltip->layout->offsetMin = Vec2F(anchorPos.x - 105.0f, anchorPos.y);
			tooltip->layout->offsetMax = Vec2F(anchorPos.x + 105.0f, anchorPos.y + 121.0f);

			auto amount = MakeLabel(tooltip, WString((String)orders[i].amount), 34, true);
			amount->layout->anchorMin = Vec2F(0.0f, 0.0f);
			amount->layout->anchorMax = Vec2F(1.0f, 1.0f);
			amount->layout->offsetMin = Vec2F(74.0f, 30.0f);
			amount->layout->offsetMax = Vec2F(-14.0f, -4.0f);

			tooltip->SetDrawingDepth(500.0f + i);
			mTooltips.Add(tooltip);
		}
	}

	bool GameHUD::IsBoostHeld() const
	{
		return mBoostButton && mBoostButton->IsPressed();
	}

	bool GameHUD::IsArrowHeld(Dir dir) const
	{
		auto& arrow = mArrows[(int)dir];
		return arrow && arrow->IsPressed();
	}

	void GameHUD::Update(float dt)
	{
		if (!mSession)
			return;

		mPulsePhase += dt;

		mTokensLabel->SetText(WString((String)mSession->GetTokens()));

		mFuelBar->SetValueForcible(Math::Clamp01(mSession->GetFuelFraction()));
		if (mSession->GetFuel() < 10.0f)
		{
			mLowFuelBlink += dt*6.0f;
			mFuelBar->SetTransparency(0.65f + 0.35f*Math::Sin(mLowFuelBlink));
		}
		else
			mFuelBar->SetTransparency(1.0f);

		mBoostReserveBar->SetValueForcible(Math::Clamp01(mSession->GetBoostFraction()));

		for (int i = 0; i < mQuestChecks.Count(); i++)
		{
			bool completed = mSession->IsOrderCompleted(i);
			mQuestChecks[i]->SetTransparency(completed ? 1.0f : 0.25f);

			if (i < mTooltips.Count() && mTooltips[i])
			{
				mTooltips[i]->SetEnabled(!completed);
				if (!completed)
				{
					float scale = 1.0f + 0.05f*Math::Sin(mPulsePhase*2.5f + i);
					mTooltips[i]->transform->SetScale(Vec3F(scale, scale, 1.0f));
				}
			}
		}

		// plash slide in/out
		if (mPlashTimer >= 0.0f)
		{
			mPlashTimer += dt;
			float x;
			if (mPlashTimer < 0.25f)
				x = Math::Lerp(-400.0f, 40.0f, mPlashTimer/0.25f);
			else if (mPlashTimer < 1.6f)
				x = 40.0f;
			else if (mPlashTimer < 1.85f)
				x = Math::Lerp(40.0f, -400.0f, (mPlashTimer - 1.6f)/0.25f);
			else
			{
				mPlash->SetEnabled(false);
				mPlashTimer = -1.0f;
				x = -400.0f;
			}
			mPlash->layout->offsetMin = Vec2F(x, 20.0f);
			mPlash->layout->offsetMax = Vec2F(x + 360.0f, 104.0f);
		}
	}

	void GameHUD::ShowOrderCompleted(int orderIndex)
	{
		auto& orders = mSession->GetCity().orders;
		if (orderIndex >= 0 && orderIndex < orders.Count())
			mPlashLabel->SetText(WString(String("Office ") + orders[orderIndex].name + " done!"));

		mPlash->SetEnabled(true);
		mPlashTimer = 0.0f;
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

	void GameHUD::HideWindows()
	{
		if (mDimmer)
			mDimmer->SetEnabled(false);
		if (mWinWindow)
			mWinWindow->SetEnabled(false);
		if (mLoseWindow)
			mLoseWindow->SetEnabled(false);
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
		mQuestPanel = nullptr;
		mSession = nullptr;
	}
}
