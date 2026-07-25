#pragma once

#include "TokenDelivery/GameSession.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Scene/UI/Widgets/Image.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Scene/UI/Widgets/Toggle.h"
#include "o2/Utils/Function/Function.h"

using namespace o2;

namespace td
{
	// The whole 2D interface built from o2UI widgets (styles from GameUIStyle):
	// token counter, fuel bar, settings button, world-space order tooltips, the
	// completed-task panel sliding in from the left and the win/lose windows.
	class GameHUD
	{
	public:
		Function<void()> onRetry;     // lose window button / settings restart
		Function<void()> onNextLevel; // win window button

		void Build();
		void BindLevel(GameSession* session, const Vector<Ref<Actor>>& officeAnchors);
		void Update(float dt);
		void Clear();

		void ShowOrderCompleted(int orderIndex);
		void ShowWin();
		void ShowLose();
		void ShowSettings();
		void HideWindows();

	private:
		GameSession* mSession = nullptr;

		Ref<Widget> mRoot;
		Ref<Label>  mTokensLabel;
		Vector<Ref<Image>> mFuelSegments; // six pieces, drained right to left

		Vector<Ref<Widget>> mTooltips; // world-space, indexed by order

		Ref<Widget> mTaskPanel;        // completed task panel, slides in from the left
		Ref<Label>  mTaskLabel;
		float       mTaskTimer = -1.0f;

		Ref<Widget> mDimmer;
		Ref<Widget> mWinWindow;
		Ref<Widget> mLoseWindow;
		Ref<Widget> mSettingsWindow;
		Ref<Toggle> mSoundToggle;
		Ref<Toggle> mMusicToggle;

		float mPulsePhase = 0.0f;
		float mLowFuelBlink = 0.0f;

		// result window: full-art background, title/message centered at the given heights
		// (from the widget bottom) and a blue caption button on the bottom edge
		Ref<Widget> MakeResultWindow(const String& bgSprite, const Vec2F& offMin, const Vec2F& offMax,
									 const WString& title, float titleY,
									 const WString& message, float messageY,
									 const WString& buttonCaption, const Function<void()>& onClick);
		void BuildSettingsWindow();
		Ref<Label> MakeLabel(const Ref<Widget>& parent, const WString& text, int height,
							 const String& style = "standard");
	};
}
