#pragma once

#include "TokenDelivery/GameSession.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/Widgets/Image.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Utils/Function/Function.h"

using namespace o2;

namespace td
{
	// Intro tutorial played over the running game: a dimmed overlay with a soft spotlight
	// hole, step title/description and illustrations. Every step plays a short live segment
	// of the real game (none for the first one) and then freezes the session until the
	// player taps or presses any key.
	class GameTutorial
	{
	public:
		enum class Step { Intro, Loading, Controls, Fuel, Count };

		Function<Vec2F(const Vec2F&)> worldToUI; // world point to UI space, set by the controller

		void Build();
		void Clear();

		void Start(GameSession* session);
		void Update(float dt);
		void Finish();

		bool IsActive() const { return mActive; }
		bool IsPausingGame() const { return mActive && mWaiting; } // waiting for the tap
		Step GetStep() const { return mStep; }

	private:
		GameSession* mSession = nullptr;

		Ref<Widget> mRoot;
		Ref<Widget> mDim;         // holds the dim at its own transparency, content stays solid
		Ref<Widget> mDimParts[4]; // solid dim around the spotlight hole: left, right, top, bottom
		Ref<Image>  mSpotlight;   // radial hole tile, butts up against the four parts
		Ref<Label>  mTitle;
		Ref<Label>  mDescription;
		Ref<Label>  mHint;
		Ref<Widget> mPictures; // load -> drive -> deliver strip of the intro step
		Ref<Widget> mKeys;     // arrow and space keycaps of the controls step

		Step  mStep = Step::Intro;
		bool  mActive = false;
		bool  mWaiting = false;  // live segment is over, the game is frozen for reading
		float mLiveTime = 0.0f;
		float mTapHold = 0.0f;   // ignores the tap that started this step
		float mFade = 0.0f;
		float mHintPhase = 0.0f;
		bool  mWasFilling = false;

		Vec2F mHoleCenter;
		Vec2F mHoleRadius; // zero closes the hole into a plain full-screen dim

		void ApplyStep();
		bool IsLiveSegmentOver() const;
		void UpdateHole(float dt);
		void PlaceHole(const Vec2F& center, const Vec2F& radius);
		void PlaceLabel(const Ref<Label>& label, float y, float halfWidth, float halfHeight);
		Ref<Image> MakePicture(const Ref<Widget>& parent, const String& sprite, const Vec2F& center,
							   const Vec2F& size);
	};
}
// --- META ---

PRE_ENUM_META(td::GameTutorial::Step);
// --- END META ---
