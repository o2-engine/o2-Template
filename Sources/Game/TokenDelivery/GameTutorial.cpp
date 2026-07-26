#include "o2/stdafx.h"
#include "TokenDelivery/GameTutorial.h"

#include "TokenDelivery/CityViewBuilder.h"
#include "TokenDelivery/GameUIStyle.h"
#include "o2/Application/Input.h"
#include "o2/Render/Sprite.h"
#include "o2/Scene/UI/UIManager.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"

namespace td
{
	static const Color4 kDimColor(16, 22, 38, 255);
	static const float  kDimAlpha = 0.74f;
	static const float  kDimEdge = 2000.0f; // dim parts run past the screen into the overscan

	static const float kHintY = -338.0f;
	static const float kCarSpotRadius = 235.0f;

	// fuel panel rect in UI space: HUD root corner (-640, -400) plus the panel offsets
	static const Vec2F kFuelSpotCenter(-496.0f, -347.0f);
	static const Vec2F kFuelSpotRadius(260.0f, 150.0f);

	static const float kLoadedTokens = 100.0f; // enough tokens seen loading, step 2 is done

	void GameTutorial::Build()
	{
		Clear();

		mRoot = mmake<Widget>();
		mRoot->SetName("tutorial");
		mRoot->SetLayer(kUILayer);
		mRoot->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mRoot->layout->anchorMax = Vec2F(0.0f, 0.0f);
		mRoot->layout->offsetMin = Vec2F(-640.0f, -400.0f);
		mRoot->layout->offsetMax = Vec2F(640.0f, 400.0f);
		mRoot->SetDrawingDepth(200.0f); // over the whole HUD

		mDim = mmake<Widget>();
		mDim->SetName("dim");
		mRoot->AddChild(mDim);
		mDim->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mDim->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mDim->layout->offsetMin = Vec2F();
		mDim->layout->offsetMax = Vec2F();
		mDim->SetTransparency(kDimAlpha);

		for (auto& part : mDimParts)
		{
			part = mmake<Widget>();
			part->AddLayer("back", mmake<Sprite>(kDimColor));
			mDim->AddChild(part);
			part->layout->anchorMin = Vec2F(0.5f, 0.5f);
			part->layout->anchorMax = Vec2F(0.5f, 0.5f);
		}

		mSpotlight = o2UI.CreateImage("Game/UI/tut_spotlight.png");
		mDim->AddChild(mSpotlight);
		mSpotlight->layout->anchorMin = Vec2F(0.5f, 0.5f);
		mSpotlight->layout->anchorMax = Vec2F(0.5f, 0.5f);

		mTitle = MakeGameLabel(mRoot, L"", 40);
		mDescription = MakeGameLabel(mRoot, L"", 22);
		mHint = MakeGameLabel(mRoot, L"Tap or press any key to continue", 22);
		PlaceLabel(mHint, kHintY, 400.0f, 26.0f);

		// intro strip: three illustrations of the loop with arrows and captions between
		mPictures = mmake<Widget>();
		mPictures->SetName("tutorial pictures");
		mRoot->AddChild(mPictures);
		mPictures->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mPictures->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mPictures->layout->offsetMin = Vec2F();
		mPictures->layout->offsetMax = Vec2F();

		const Vec2F cardSize(300.0f, 220.0f);
		const float cardY = 15.0f;
		struct PicDef { const char* sprite; float x; const wchar_t* caption; };
		const PicDef pics[] = { { "Game/UI/tut_pic_load.png", -390.0f, L"1. LOAD" },
								{ "Game/UI/tut_pic_drive.png", 0.0f, L"2. DRIVE" },
								{ "Game/UI/tut_pic_deliver.png", 390.0f, L"3. DELIVER" } };
		for (auto& pic : pics)
		{
			MakePicture(mPictures, pic.sprite, Vec2F(pic.x, cardY), cardSize);
			auto caption = MakeGameLabel(mPictures, pic.caption, 24);
			caption->layout->anchorMin = Vec2F(0.5f, 0.5f);
			caption->layout->anchorMax = Vec2F(0.5f, 0.5f);
			caption->layout->offsetMin = Vec2F(pic.x - 150.0f, cardY - 152.0f);
			caption->layout->offsetMax = Vec2F(pic.x + 150.0f, cardY - 108.0f);
		}
		for (float x : { -195.0f, 195.0f })
			MakePicture(mPictures, "Game/UI/tut_arrow.png", Vec2F(x, cardY), Vec2F(96.0f, 68.0f));

		// steering keycaps of the controls step
		mKeys = mmake<Widget>();
		mKeys->SetName("tutorial keys");
		mRoot->AddChild(mKeys);
		mKeys->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mKeys->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mKeys->layout->offsetMin = Vec2F();
		mKeys->layout->offsetMax = Vec2F();

		MakePicture(mKeys, "Game/UI/tut_key_left.png", Vec2F(-249.0f, 0.0f), Vec2F(132.0f, 142.0f));
		MakePicture(mKeys, "Game/UI/tut_key_right.png", Vec2F(-99.0f, 0.0f), Vec2F(132.0f, 142.0f));
		MakePicture(mKeys, "Game/UI/tut_key_space.png", Vec2F(150.0f, 0.0f), Vec2F(330.0f, 142.0f));

		mRoot->SetEnabled(false);
	}

	Ref<Image> GameTutorial::MakePicture(const Ref<Widget>& parent, const String& sprite,
										 const Vec2F& center, const Vec2F& size)
	{
		auto image = o2UI.CreateImage(sprite);
		parent->AddChild(image);
		image->layout->anchorMin = Vec2F(0.5f, 0.5f);
		image->layout->anchorMax = Vec2F(0.5f, 0.5f);
		image->layout->offsetMin = center - size*0.5f;
		image->layout->offsetMax = center + size*0.5f;
		return image;
	}

	void GameTutorial::PlaceLabel(const Ref<Label>& label, float y, float halfWidth,
								  float halfHeight)
	{
		label->layout->anchorMin = Vec2F(0.5f, 0.5f);
		label->layout->anchorMax = Vec2F(0.5f, 0.5f);
		label->layout->offsetMin = Vec2F(-halfWidth, y - halfHeight);
		label->layout->offsetMax = Vec2F(halfWidth, y + halfHeight);
	}

	void GameTutorial::Start(GameSession* session)
	{
		if (!mRoot)
			return;

		mSession = session;
		mActive = true;
		mStep = Step::Intro;
		mWaiting = true; // the intro has no live segment, it opens frozen
		mLiveTime = 0.0f;
		mTapHold = 0.3f;
		mFade = 0.0f;
		mWasFilling = false;
		mHoleRadius = Vec2F();
		mRoot->SetEnabled(true);
		mRoot->SetTransparency(0.0f);
		ApplyStep();
	}

	void GameTutorial::Finish()
	{
		mActive = false;
		mWaiting = false;
		mStep = Step::Count; // closes the spotlight while the overlay fades out
		ApplyStep();
	}

	void GameTutorial::ApplyStep()
	{
		mPictures->SetEnabled(mWaiting && mStep == Step::Intro);
		mKeys->SetEnabled(mWaiting && mStep == Step::Controls);
		mTitle->SetEnabled(mWaiting);
		mDescription->SetEnabled(mWaiting);
		mHint->SetEnabled(mWaiting);

		if (!mWaiting)
			return;

		switch (mStep)
		{
			case Step::Intro:
			mTitle->SetText(L"TOKEN DELIVERY");
			mDescription->SetText(L"Load AI tokens at the source, look for the order bubbles\n"
								  L"over the offices and drive by to deliver them.");
			break;

			case Step::Loading:
			mTitle->SetText(L"LOAD YOUR TRUCK");
			mDescription->SetText(L"Drive close to the token source and the chips fly into\n"
								  L"your bed. An order costs the tokens written on its bubble.");
			break;

			case Step::Controls:
			mTitle->SetText(L"TURN AT CROSSROADS");
			mDescription->SetText(L"Your truck never stops. Turn at an intersection with the\n"
								  L"arrow keys or the corner buttons, SPACE takes any turn.");
			break;

			case Step::Fuel:
			mTitle->SetText(L"60 SECONDS OF FUEL");
			mDescription->SetText(L"The tank drains all the time and the run ends when the\n"
								  L"last bar is gone. Deliver every order before that.");
			break;

			default: break;
		}

		PlaceLabel(mTitle, 296.0f, 600.0f, 40.0f);
		PlaceLabel(mDescription, 212.0f, 500.0f, 46.0f);
	}

	bool GameTutorial::IsLiveSegmentOver() const
	{
		switch (mStep)
		{
			// enough tokens loaded, or the car has driven out of the source radius
			case Step::Loading:
			return !mSession || mSession->GetTokens() >= kLoadedTokens ||
				   (mWasFilling && !mSession->IsFilling()) || mLiveTime > 6.0f;

			case Step::Controls: return mLiveTime > 1.8f;
			case Step::Fuel: return mLiveTime > 1.5f;
			default: return true;
		}
	}

	void GameTutorial::UpdateHole(float dt)
	{
		Vec2F center = mHoleCenter;
		Vec2F radius;

		// the car is spotlighted while it loads and while it shows off the steering; the
		// keycaps of the paused controls step read better over a plain dim
		bool carSpot = mStep == Step::Loading || (mStep == Step::Controls && !mWaiting);
		if (carSpot && mSession && worldToUI)
		{
			center = worldToUI(CellToScreen(mSession->GetCar().GetVisualPos())) + Vec2F(0.0f, 12.0f);
			radius = Vec2F(kCarSpotRadius, kCarSpotRadius);
		}
		else if (mStep == Step::Fuel)
		{
			center = kFuelSpotCenter;
			radius = kFuelSpotRadius;
		}

		float lerp = Math::Min(1.0f, dt*8.0f);
		mHoleCenter = mHoleRadius.x < 1.0f ? center : Math::Lerp(mHoleCenter, center, lerp);
		mHoleRadius = Math::Lerp(mHoleRadius, radius, lerp);
		PlaceHole(mHoleCenter, mHoleRadius);
	}

	void GameTutorial::PlaceHole(const Vec2F& center, const Vec2F& radius)
	{
		float x0 = center.x - radius.x, x1 = center.x + radius.x;
		float y0 = center.y - radius.y, y1 = center.y + radius.y;

		auto place = [](const Ref<Widget>& part, float left, float bottom, float right, float top)
		{
			part->layout->offsetMin = Vec2F(left, bottom);
			part->layout->offsetMax = Vec2F(right, top);
		};
		// edges are shared exactly: overlapping parts would double the dim into a seam
		place(mDimParts[0], -kDimEdge, -kDimEdge, x0, kDimEdge);
		place(mDimParts[1], x1, -kDimEdge, kDimEdge, kDimEdge);
		place(mDimParts[2], x0, y1, x1, kDimEdge);
		place(mDimParts[3], x0, -kDimEdge, x1, y0);

		mSpotlight->SetEnabled(radius.x > 1.0f);
		mSpotlight->layout->offsetMin = Vec2F(x0, y0);
		mSpotlight->layout->offsetMax = Vec2F(x1, y1);
	}

	void GameTutorial::Update(float dt)
	{
		if (!mRoot || !mRoot->IsEnabled())
			return;

		mFade = Math::Clamp01(mFade + (mActive ? dt*4.0f : -dt*3.0f));
		mRoot->SetTransparency(mFade);

		if (!mActive)
		{
			UpdateHole(dt);
			if (mFade <= 0.0f)
				mRoot->SetEnabled(false);
			return;
		}

		mTapHold = Math::Max(0.0f, mTapHold - dt);
		mHintPhase += dt;
		mHint->SetTransparency(0.6f + 0.4f*Math::Sin(mHintPhase*3.0f));

		if (mWaiting)
		{
			bool tap = o2Input.IsCursorPressed() || !o2Input.GetPressedKeys().IsEmpty();
			if (tap && mTapHold <= 0.0f)
			{
				int next = (int)mStep + 1;
				if (next >= (int)Step::Count)
				{
					Finish();
					return;
				}

				mStep = (Step)next;
				mWaiting = false;
				mLiveTime = 0.0f;
				mWasFilling = false;
				ApplyStep();
			}
		}
		else
		{
			mLiveTime += dt;
			if (mSession && mSession->IsFilling())
				mWasFilling = true;

			if (IsLiveSegmentOver())
			{
				mWaiting = true;
				mTapHold = 0.15f;
				ApplyStep();
			}
		}

		UpdateHole(dt);
	}

	void GameTutorial::Clear()
	{
		if (mRoot)
			mRoot->RemoveFromScene();

		mRoot = nullptr;
		mDim = nullptr;
		for (auto& part : mDimParts)
			part = nullptr;
		mSpotlight = nullptr;
		mTitle = nullptr;
		mDescription = nullptr;
		mHint = nullptr;
		mPictures = nullptr;
		mKeys = nullptr;
		mSession = nullptr;
		mActive = false;
	}
}
// --- META ---

ENUM_META(td::GameTutorial::Step, td__GameTutorial__Step)
{
    ENUM_ENTRY(Controls);
    ENUM_ENTRY(Count);
    ENUM_ENTRY(Fuel);
    ENUM_ENTRY(Intro);
    ENUM_ENTRY(Loading);
}
END_ENUM_META;
// --- END META ---
