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
	// ---------------------------------------------------------------------------------------
	// The whole 2D interface built from o2UI widgets (styles from GameUIStyle): token counter,
	// fuel bar, settings button, world-space order tooltips, the completed-task panel sliding
	// in from the left and the win/lose windows. Lives on the game actor in the bootstrap
	// scene; the controller component builds it and drives Update explicitly to keep the
	// frame order deterministic.
	// ---------------------------------------------------------------------------------------
	class GameHUDComponent: public Component
	{
	public:
		Function<void()> onRetry;     // Lose window button / settings restart callback
		Function<void()> onNextLevel; // Win window button callback

	public:
		float taskPanelHoldTime = 2.2f;  // Completed-task panel hold before the slide-out @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0.5, 10)
		float turnTapTime = 0.35f;       // A steering tap keeps turning for this long @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0.05, 1)
		float tokenStreamInterval = 0.12f; // Pause between tokens of the filling stream @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0.02, 1)

	public:
		// Default constructor
		GameHUDComponent();

		// Constructor with ref counter
		explicit GameHUDComponent(RefCounter* refCounter);

		// Creates all widgets: panels, buttons, windows and the token flight vfx host
		void Build();

		// Removes all widgets and resets the level binding
		void Clear();

		// Binds the HUD to a fresh level: session state and world anchors for the tooltips
		void BindLevel(GameSession* session, const Vector<Ref<Actor>>& officeAnchors,
					   const Ref<Actor>& sourceAnchor = nullptr);

		// Updates counters, tooltips, vfx and panel animations; called by the controller
		void Update(float dt);

		// Shows the completed-task panel and starts the token flight into the tooltip
		void ShowOrderCompleted(int orderIndex);

		// Returns true while any order tooltip is still on screen, including its exit
		// animation — the win window waits for the last one to leave
		bool HasVisibleTooltips() const;

		// Enables or disables the settings button; off while the tutorial eats every tap
		void SetSettingsEnabled(bool enabled);

		// Returns is the left on-screen steering pressed: held, or tapped a moment ago
		bool IsTurningLeft() const;

		// Returns is the right on-screen steering pressed: held, or tapped a moment ago
		bool IsTurningRight() const;

		// Shows the win window over the dimmer
		void ShowWin();

		// Shows the lose window over the dimmer
		void ShowLose();

		// Shows the settings window over the dimmer
		void ShowSettings();

		// Hides the dimmer and every popup window
		void HideWindows();

		SERIALIZABLE(GameHUDComponent);
		CLONEABLE_REF(GameHUDComponent);

	private:
		// World-space order tooltip, indexed by order
		struct OrderTooltip
		{
			Ref<Widget> widget;
			Ref<Label>  amount;    // Turns red while the player is short on tokens
			Vec2F       target;    // Token flight target inside the bubble, world
			Vec2F       offsetMin; // Layout rect at rest, the exit animation shifts it
			Vec2F       offsetMax;
			float       linger = 0.0f;  // Keeps a completed tooltip visible for the flight
			float       bounce = -1.0f; // Bounce phase after a token lands, -1 = idle
			float       exit = -1.0f;   // Exit animation phase, -1 = idle

			bool operator==(const OrderTooltip& other) const { return this == &other; }
		};

		// Flying token: plain data drawn manually by the vfx host actor (no per-particle
		// actors — the scene must not change mid-update); arrival sparks are o2 particle
		// emitters restarted at the hit point
		struct TokenFlyer
		{
			bool  toCar = true; // Target follows the moving car bed, otherwise it is fixed
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

	private:
		GameSession* mSession = nullptr; // Bound level state, owned by the controller

		Ref<Widget> mRoot;        // HUD root widget, the fixed 1280x800 UI rect
		Ref<Label>  mTokensLabel; // Token counter in the top-left panel

		Vector<Ref<Image>> mFuelSegments; // Six pieces, drained right to left

		Vector<OrderTooltip> mTooltips; // World-space order tooltips, indexed by order

		Vector<TokenFlyer> mFlyers;       // Flying tokens, manually drawn plain data
		Ref<Actor>         mSourceAnchor; // Token source in the city (fountain hologram)
		Ref<Actor>         mVfxActor;     // Draws the flyers above the city

		Ref<ParticlesEmitterComponent> mSparkEmitter; // Arrival sparks burst emitter

		Ref<Sprite> mChipSprite;             // Shared sprite for all flying tokens
		float       mFillSpawnAccum = 0.0f;  // Time to the next token of the filling stream
		float       mChipNativeW = 200.0f;   // Chip sprite native width, scales the flyers

		Ref<Button> mSettingsButton;  // Settings gear, top-right
		Ref<Button> mTurnLeftButton;  // On-screen steering, bottom-right corner
		Ref<Button> mTurnRightButton; // On-screen steering, bottom-right corner

		float mTurnLeftTap = 0.0f;  // A tap keeps the turn command alive for a moment,
		float mTurnRightTap = 0.0f; // long enough to catch the crossroad ahead

		Ref<Widget> mTaskPanel;        // Completed task panel, slides in from the left
		Ref<Label>  mTaskLabel;        // Task panel office name text
		float       mTaskTimer = -1.0f; // Task panel animation phase, -1 = hidden

		Ref<Widget> mDimmer;         // Darkens the game under the popup windows
		Ref<Widget> mWinWindow;      // Mission complete popup
		Ref<Widget> mLoseWindow;     // Out of fuel popup
		Ref<Widget> mSettingsWindow; // Sound/music switches popup
		Ref<Toggle> mSoundToggle;    // Sound switch of the settings window
		Ref<Toggle> mMusicToggle;    // Music switch of the settings window

		float mPulsePhase = 0.0f; // Shared phase of the tooltip idle pulse

	private:
		// Creates a result window: full-art background, title/message centered at the given
		// heights (from the widget bottom) and a blue caption button on the bottom edge
		Ref<Widget> MakeResultWindow(const String& name, const String& bgSprite,
									 const Vec2F& offMin, const Vec2F& offMax,
									 const WString& title, float titleY,
									 const WString& message, float messageY,
									 const WString& buttonCaption, const Function<void()>& onClick);

		// Creates the settings window over the baked background art
		void BuildSettingsWindow();

		// Creates a game-styled label under the parent widget
		Ref<Label> MakeLabel(const Ref<Widget>& parent, const WString& text, int height,
							 const String& style = "standard");

		// Advances the tooltip exit: hop up, then a faster drop fading to zero alpha;
		// returns the scale multiplier
		float UpdateTooltipExit(OrderTooltip& tip, float dt);

		// Returns the world point above the car bed, where tokens land
		Vec2F CarBedPos() const;

		// Spawns one token flying from the source into the car bed
		void SpawnTokenToCar();

		// Restarts the spark emitter at the given world point
		void BurstSparks(const Vec2F& pos);

		// Advances all flying tokens and triggers the arrival effects
		void UpdateVfx(float dt);

		// Draws the flying tokens; called by the vfx host actor draw hook
		void DrawVfx();

		// Removes all flyers and stops the sparks
		void ClearVfx();

		REF_COUNTERABLE_IMPL(Component);
	};
}
// --- META ---

CLASS_BASES_META(td::GameHUDComponent)
{
    BASE_CLASS(o2::Component);
}
END_META;
CLASS_FIELDS_META(td::GameHUDComponent)
{
    FIELD().PUBLIC().NAME(onRetry);
    FIELD().PUBLIC().NAME(onNextLevel);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0.5, 10).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(2.2f).NAME(taskPanelHoldTime);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0.05, 1).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.35f).NAME(turnTapTime);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0.02, 1).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.12f).NAME(tokenStreamInterval);
    FIELD().PRIVATE().DEFAULT_VALUE(nullptr).NAME(mSession);
    FIELD().PRIVATE().NAME(mRoot);
    FIELD().PRIVATE().NAME(mTokensLabel);
    FIELD().PRIVATE().NAME(mFuelSegments);
    FIELD().PRIVATE().NAME(mTooltips);
    FIELD().PRIVATE().NAME(mFlyers);
    FIELD().PRIVATE().NAME(mSourceAnchor);
    FIELD().PRIVATE().NAME(mVfxActor);
    FIELD().PRIVATE().NAME(mSparkEmitter);
    FIELD().PRIVATE().NAME(mChipSprite);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mFillSpawnAccum);
    FIELD().PRIVATE().DEFAULT_VALUE(200.0f).NAME(mChipNativeW);
    FIELD().PRIVATE().NAME(mSettingsButton);
    FIELD().PRIVATE().NAME(mTurnLeftButton);
    FIELD().PRIVATE().NAME(mTurnRightButton);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mTurnLeftTap);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mTurnRightTap);
    FIELD().PRIVATE().NAME(mTaskPanel);
    FIELD().PRIVATE().NAME(mTaskLabel);
    FIELD().PRIVATE().DEFAULT_VALUE(-1.0f).NAME(mTaskTimer);
    FIELD().PRIVATE().NAME(mDimmer);
    FIELD().PRIVATE().NAME(mWinWindow);
    FIELD().PRIVATE().NAME(mLoseWindow);
    FIELD().PRIVATE().NAME(mSettingsWindow);
    FIELD().PRIVATE().NAME(mSoundToggle);
    FIELD().PRIVATE().NAME(mMusicToggle);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mPulsePhase);
}
END_META;
CLASS_METHODS_META(td::GameHUDComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
    FUNCTION().PUBLIC().SIGNATURE(void, Build);
    FUNCTION().PUBLIC().SIGNATURE(void, Clear);
    FUNCTION().PUBLIC().SIGNATURE(void, BindLevel, GameSession*, const Vector<Ref<Actor>>&, const Ref<Actor>&);
    FUNCTION().PUBLIC().SIGNATURE(void, Update, float);
    FUNCTION().PUBLIC().SIGNATURE(void, ShowOrderCompleted, int);
    FUNCTION().PUBLIC().SIGNATURE(bool, HasVisibleTooltips);
    FUNCTION().PUBLIC().SIGNATURE(void, SetSettingsEnabled, bool);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsTurningLeft);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsTurningRight);
    FUNCTION().PUBLIC().SIGNATURE(void, ShowWin);
    FUNCTION().PUBLIC().SIGNATURE(void, ShowLose);
    FUNCTION().PUBLIC().SIGNATURE(void, ShowSettings);
    FUNCTION().PUBLIC().SIGNATURE(void, HideWindows);
    FUNCTION().PRIVATE().SIGNATURE(Ref<Widget>, MakeResultWindow, const String&, const String&, const Vec2F&, const Vec2F&, const WString&, float, const WString&, float, const WString&, const Function<void()>&);
    FUNCTION().PRIVATE().SIGNATURE(void, BuildSettingsWindow);
    FUNCTION().PRIVATE().SIGNATURE(Ref<Label>, MakeLabel, const Ref<Widget>&, const WString&, int, const String&);
    FUNCTION().PRIVATE().SIGNATURE(float, UpdateTooltipExit, OrderTooltip&, float);
    FUNCTION().PRIVATE().SIGNATURE(Vec2F, CarBedPos);
    FUNCTION().PRIVATE().SIGNATURE(void, SpawnTokenToCar);
    FUNCTION().PRIVATE().SIGNATURE(void, BurstSparks, const Vec2F&);
    FUNCTION().PRIVATE().SIGNATURE(void, UpdateVfx, float);
    FUNCTION().PRIVATE().SIGNATURE(void, DrawVfx);
    FUNCTION().PRIVATE().SIGNATURE(void, ClearVfx);
}
END_META;
// --- END META ---
