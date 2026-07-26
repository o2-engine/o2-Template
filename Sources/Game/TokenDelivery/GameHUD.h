#pragma once

#include "TokenDelivery/GameSession.h"
#include "o2/Scene/Components/ParticlesEmitterComponent.h"
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
		void BindLevel(GameSession* session, const Vector<Ref<Actor>>& officeAnchors,
					   const Ref<Actor>& sourceAnchor = nullptr);
		void Update(float dt);
		void Clear();

		void ShowOrderCompleted(int orderIndex);

		// true while any order tooltip is still on screen, including its exit animation —
		// the win window waits for the last one to leave
		bool HasVisibleTooltips() const;

		void SetSettingsEnabled(bool enabled); // off while the tutorial eats every tap

		void ShowWin();
		void ShowLose();
		void ShowSettings();
		void HideWindows();

	private:
		GameSession* mSession = nullptr;

		Ref<Widget> mRoot;
		Ref<Label>  mTokensLabel;
		Vector<Ref<Image>> mFuelSegments; // six pieces, drained right to left

		// world-space order tooltip, indexed by order
		struct OrderTooltip
		{
			Ref<Widget> widget;
			Ref<Label>  amount;    // turns red while the player is short on tokens
			Vec2F       target;    // token flight target inside the bubble, world
			Vec2F       offsetMin; // layout rect at rest, the exit animation shifts it
			Vec2F       offsetMax;
			float       linger = 0.0f;  // keeps a completed tooltip visible for the flight
			float       bounce = -1.0f; // bounce phase after a token lands, -1 = idle
			float       exit = -1.0f;   // exit animation phase, -1 = idle

			bool operator==(const OrderTooltip& other) const { return this == &other; }
		};
		Vector<OrderTooltip> mTooltips;

		// flying tokens: plain data drawn manually by the vfx host actor (no per-particle
		// actors — the scene must not change mid-update); arrival sparks are o2 particle
		// emitters restarted at the hit point
		struct TokenFlyer
		{
			bool  toCar = true; // target follows the moving car bed, otherwise it is fixed
			Vec2F fromWorld;
			Vec2F target;
			Vec2F drawPos;
			float drawScale = 1.0f;
			float drawAngle = 0.0f;
			float sizeJitter = 1.0f;
			float tilt = 0.0f;
			int   tooltipIndex = -1;
			float age = 0.0f;
			float duration = 0.5f;

			bool operator==(const TokenFlyer& other) const { return this == &other; }
		};
		Vector<TokenFlyer> mFlyers;
		Ref<Actor>  mSourceAnchor; // token source in the city (fountain hologram)
		Ref<Actor>  mVfxActor;     // draws the flyers above the city
		Ref<ParticlesEmitterComponent> mSparkEmitter;
		Ref<Sprite> mChipSprite;
		float       mFillSpawnAccum = 0.0f;
		float       mChipNativeW = 200.0f;

		Ref<Button> mSettingsButton;

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
		Ref<Widget> MakeResultWindow(const String& name, const String& bgSprite,
									 const Vec2F& offMin, const Vec2F& offMax,
									 const WString& title, float titleY,
									 const WString& message, float messageY,
									 const WString& buttonCaption, const Function<void()>& onClick);
		void BuildSettingsWindow();
		Ref<Label> MakeLabel(const Ref<Widget>& parent, const WString& text, int height,
							 const String& style = "standard");
		// hop up, then a faster drop fading to zero alpha; returns the scale multiplier
		float UpdateTooltipExit(OrderTooltip& tip, float dt);
		Vec2F CarBedPos() const; // world point above the car bed, where tokens land
		void SpawnTokenToCar();
		void BurstSparks(const Vec2F& pos);
		void UpdateVfx(float dt);
		void DrawVfx();
		void ClearVfx();
	};
}
