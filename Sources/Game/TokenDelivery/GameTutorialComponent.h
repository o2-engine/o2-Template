#pragma once

#include "TokenDelivery/GameSession.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/Widgets/Image.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Utils/Function/Function.h"

using namespace o2;

namespace td
{
	// --------------------------------------------------------------------------------------
	// Intro tutorial played over the running game: a dimmed overlay with a soft spotlight
	// hole, step title/description and illustrations. Every step plays a short live segment
	// of the real game (none for the first one) and then freezes the session until the
	// player taps or presses any key. Lives on the game actor in the bootstrap scene; the
	// controller builds it and drives Update with the real (unpaused) delta time.
	// --------------------------------------------------------------------------------------
	class GameTutorialComponent: public Component
	{
	public:
		enum class Step { Intro, Loading, Controls, Fuel, Count };

	public:
		Function<Vec2F(const Vec2F&)> worldToUI; // World point to UI space, set by the controller

	public:
		float loadingTimeout = 6.0f;    // Loading step gives up waiting for a full bed after this @SERIALIZABLE @EDITOR_PROPERTY @RANGE(1, 20)
		float controlsDriveTime = 1.8f; // Live driving shown before the controls step text @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0.5, 5)
		float fuelDriveTime = 1.5f;     // Live driving shown before the fuel step text @SERIALIZABLE @EDITOR_PROPERTY @RANGE(0.5, 5)
		float carSpotRadius = 235.0f;   // Spotlight radius around the car, UI units @SERIALIZABLE @EDITOR_PROPERTY @RANGE(100, 500)

	public:
		// Default constructor
		GameTutorialComponent();

		// Constructor with ref counter
		explicit GameTutorialComponent(RefCounter* refCounter);

		// Creates the overlay widgets: dim parts, spotlight, labels and illustrations
		void Build();

		// Removes the overlay widgets and resets the state
		void Clear();

		// Starts the tutorial from the intro step over the given session
		void Start(GameSession* session);

		// Advances the current step, the spotlight and the tap waiting
		void Update(float dt);

		// Ends the tutorial and fades the overlay out
		void Finish();

		// Returns is the tutorial running
		bool IsActive() const { return mActive; }

		// Returns is the game frozen while a step waits for the tap
		bool IsPausingGame() const { return mActive && mWaiting; }

		// Returns the current step
		Step GetStep() const { return mStep; }

		SERIALIZABLE(GameTutorialComponent);
		CLONEABLE_REF(GameTutorialComponent);

	private:
		GameSession* mSession = nullptr; // Observed level state, owned by the controller

		Ref<Widget> mRoot;        // Overlay root widget, the fixed 1280x800 UI rect
		Ref<Widget> mDim;         // Holds the dim at its own transparency, content stays solid
		Ref<Widget> mDimParts[4]; // Solid dim around the spotlight hole: left, right, top, bottom
		Ref<Image>  mSpotlight;   // Radial hole tile, butts up against the four parts
		Ref<Label>  mTitle;       // Step title, large
		Ref<Label>  mDescription; // Step description under the title
		Ref<Label>  mHint;        // Pulsing "tap to continue" at the bottom
		Ref<Widget> mPictures;    // Load -> drive -> deliver strip of the intro step
		Ref<Widget> mKeys;        // Arrow and space keycaps of the controls step

		Step  mStep = Step::Intro; // Current tutorial step
		bool  mActive = false;     // Is the tutorial running
		bool  mWaiting = false;    // Live segment is over, the game is frozen for reading
		float mLiveTime = 0.0f;    // Time inside the current live segment
		float mTapHold = 0.0f;     // Ignores the tap that started this step
		float mFade = 0.0f;        // Overlay fade phase
		bool  mWasFilling = false; // The car has been inside the source radius this segment

		Vec2F mHoleCenter; // Spotlight hole center, UI space
		Vec2F mHoleRadius; // Spotlight hole radius; zero closes the hole into a plain dim

	private:
		// Applies the current step: texts, illustrations and label placement
		void ApplyStep();

		// Returns is the live segment of the current step over
		bool IsLiveSegmentOver() const;

		// Moves the spotlight hole towards the current step target
		void UpdateHole(float dt);

		// Places the four dim parts and the spotlight around the hole rect
		void PlaceHole(const Vec2F& center, const Vec2F& radius);

		// Centers the label at the given height
		void PlaceLabel(const Ref<Label>& label, float y, float halfWidth, float halfHeight);

		// Creates an illustration image centered at the given point
		Ref<Image> MakePicture(const Ref<Widget>& parent, const String& sprite, const Vec2F& center,
							   const Vec2F& size);

		REF_COUNTERABLE_IMPL(Component);
	};
}
// --- META ---

PRE_ENUM_META(td::GameTutorialComponent::Step);

CLASS_BASES_META(td::GameTutorialComponent)
{
    BASE_CLASS(o2::Component);
}
END_META;
CLASS_FIELDS_META(td::GameTutorialComponent)
{
    FIELD().PUBLIC().NAME(worldToUI);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(1, 20).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(6.0f).NAME(loadingTimeout);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0.5, 5).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(1.8f).NAME(controlsDriveTime);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(0.5, 5).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(1.5f).NAME(fuelDriveTime);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().RANGE_ATTRIBUTE(100, 500).SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(235.0f).NAME(carSpotRadius);
    FIELD().PRIVATE().DEFAULT_VALUE(nullptr).NAME(mSession);
    FIELD().PRIVATE().NAME(mRoot);
    FIELD().PRIVATE().NAME(mDim);
    FIELD().PRIVATE().NAME(mDimParts);
    FIELD().PRIVATE().NAME(mSpotlight);
    FIELD().PRIVATE().NAME(mTitle);
    FIELD().PRIVATE().NAME(mDescription);
    FIELD().PRIVATE().NAME(mHint);
    FIELD().PRIVATE().NAME(mPictures);
    FIELD().PRIVATE().NAME(mKeys);
    FIELD().PRIVATE().DEFAULT_VALUE(Step::Intro).NAME(mStep);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mActive);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mWaiting);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mLiveTime);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mTapHold);
    FIELD().PRIVATE().DEFAULT_VALUE(0.0f).NAME(mFade);
    FIELD().PRIVATE().DEFAULT_VALUE(false).NAME(mWasFilling);
    FIELD().PRIVATE().NAME(mHoleCenter);
    FIELD().PRIVATE().NAME(mHoleRadius);
}
END_META;
CLASS_METHODS_META(td::GameTutorialComponent)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(RefCounter*);
    FUNCTION().PUBLIC().SIGNATURE(void, Build);
    FUNCTION().PUBLIC().SIGNATURE(void, Clear);
    FUNCTION().PUBLIC().SIGNATURE(void, Start, GameSession*);
    FUNCTION().PUBLIC().SIGNATURE(void, Update, float);
    FUNCTION().PUBLIC().SIGNATURE(void, Finish);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsActive);
    FUNCTION().PUBLIC().SIGNATURE(bool, IsPausingGame);
    FUNCTION().PUBLIC().SIGNATURE(Step, GetStep);
    FUNCTION().PRIVATE().SIGNATURE(void, ApplyStep);
    FUNCTION().PRIVATE().SIGNATURE(bool, IsLiveSegmentOver);
    FUNCTION().PRIVATE().SIGNATURE(void, UpdateHole, float);
    FUNCTION().PRIVATE().SIGNATURE(void, PlaceHole, const Vec2F&, const Vec2F&);
    FUNCTION().PRIVATE().SIGNATURE(void, PlaceLabel, const Ref<Label>&, float, float, float);
    FUNCTION().PRIVATE().SIGNATURE(Ref<Image>, MakePicture, const Ref<Widget>&, const String&, const Vec2F&, const Vec2F&);
}
END_META;
// --- END META ---
