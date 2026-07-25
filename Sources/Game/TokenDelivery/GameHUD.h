#pragma once

#include "TokenDelivery/GameSession.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Scene/UI/Widgets/HorizontalProgress.h"
#include "o2/Scene/UI/Widgets/Image.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Utils/Function/Function.h"

using namespace o2;

namespace td
{
	// The whole 2D interface built from o2UI widgets (styles from GameUIStyle):
	// token counter, quest list, fuel/boost bars, boost button, touch arrows, world-space
	// order tooltips, the completion plash and the win/lose windows.
	class GameHUD
	{
	public:
		Function<void()> onRetry;     // lose window button / gear restart
		Function<void()> onNextLevel; // win window button

		void Build();
		void BindLevel(GameSession* session, const Vector<Ref<Actor>>& officeAnchors);
		void Update(float dt);
		void Clear();

		bool IsBoostHeld() const;
		bool IsArrowHeld(Dir dir) const;

		void ShowOrderCompleted(int orderIndex);
		void ShowWin();
		void ShowLose();
		void HideWindows();

	private:
		GameSession* mSession = nullptr;

		Ref<Widget> mRoot;
		Ref<Label>  mTokensLabel;
		Ref<HorizontalProgress> mFuelBar;
		Ref<HorizontalProgress> mBoostReserveBar;
		Ref<Button> mBoostButton;
		Ref<Button> mArrows[4];

		Ref<Widget>         mQuestPanel;
		Vector<Ref<Label>>  mQuestLabels;
		Vector<Ref<Image>>  mQuestChecks;

		Vector<Ref<Widget>> mTooltips; // world-space, indexed by order

		Ref<Widget> mPlash;
		Ref<Label>  mPlashLabel;
		float       mPlashTimer = -1.0f;

		Ref<Widget> mDimmer;
		Ref<Widget> mWinWindow;
		Ref<Widget> mLoseWindow;

		float mPulsePhase = 0.0f;
		float mLowFuelBlink = 0.0f;

		Ref<Widget> MakeWindow(const WString& title, const WString& message,
							   const WString& buttonCaption, const String& buttonStyle,
							   const Function<void()>& onClick);
		// dark=true recolors the standard label to navy and drops the light shadow —
		// for text on white pills/bubbles/windows like the reference numbers
		Ref<Label> MakeLabel(const Ref<Widget>& parent, const WString& text, int height,
							 bool dark = false);
	};
}
