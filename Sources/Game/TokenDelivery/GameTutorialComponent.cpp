#include "o2/stdafx.h"
#include "TokenDelivery/GameTutorialComponent.h"

#include "TokenDelivery/CityViewBuilder.h"
#include "TokenDelivery/GameUIStyle.h"
#include "o2/Animation/AnimationClip.h"
#include "o2/Application/Input.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/VideoAsset.h"
#include "o2/Render/Sprite.h"
#include "o2/Scene/UI/UIManager.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"

namespace td
{
	static const float kDimAlpha = 0.74f;
	static const float kHintY = -338.0f;

	// the helper character video, nearly full screen height at the left edge; the fuel
	// step spotlights the bottom-left panel, so there the character flips to the right edge
	static const float kPersHeight = 760.0f;
	static const float kPersLeftX = -426.0f;
	static const float kPersRightX = 426.0f;
	static const float kPersY = -20.0f;

	// fuel panel rect in UI space: HUD root corner (-640, -400) plus the panel offsets
	static const Vec2F kFuelSpotCenter(-496.0f, -347.0f);
	static const Vec2F kFuelSpotRadius(260.0f, 150.0f);

	static const float kLoadedTokens = 100.0f; // enough tokens seen loading, step 2 is done

	GameTutorialComponent::GameTutorialComponent():
		GameTutorialComponent(nullptr)
	{}

	GameTutorialComponent::GameTutorialComponent(RefCounter* refCounter):
		Component(refCounter)
	{}

	void GameTutorialComponent::Build()
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

		// one solid mesh with the spotlight hole: abutting sprite rects left subpixel
		// seams between their independently rounded layouts
		mDimMesh = mmake<TutorialDimDrawable>();
		mDim->AddLayer("dim", mDimMesh);

		mTitle = MakeGameLabel(mRoot, L"", 40);
		mDescription = MakeGameLabel(mRoot, L"", 22);
		mHint = MakeGameLabel(mRoot, L"Tap or press any key to continue", 22);
		PlaceLabel(mHint, kHintY, 400.0f, 26.0f);

		// the hint pulses through a looped ping-pong clip for as long as it is shown
		auto pulseClip = AnimationClip::EaseInOut("transparency", 1.0f, 0.2f, 1.05f);
		pulseClip->SetLoop(Loop::PingPong);
		mHint->AddState("pulse", pulseClip);
		mHint->SetState("pulse", true);

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
		// the strip sits right of center: the helper character occupies the left edge
		const PicDef pics[] = { { "Game/UI/tut_pic_load.png", -180.0f, L"1. LOAD" },
								{ "Game/UI/tut_pic_drive.png", 130.0f, L"2. DRIVE" },
								{ "Game/UI/tut_pic_deliver.png", 440.0f, L"3. DELIVER" } };
		for (auto& pic : pics)
		{
			MakePicture(mPictures, pic.sprite, Vec2F(pic.x, cardY), cardSize);
			auto caption = MakeGameLabel(mPictures, pic.caption, 24);
			caption->layout->anchorMin = Vec2F(0.5f, 0.5f);
			caption->layout->anchorMax = Vec2F(0.5f, 0.5f);
			caption->layout->offsetMin = Vec2F(pic.x - 150.0f, cardY - 152.0f);
			caption->layout->offsetMax = Vec2F(pic.x + 150.0f, cardY - 108.0f);
		}
		for (float x : { -25.0f, 285.0f })
			MakePicture(mPictures, "Game/UI/tut_arrow.png", Vec2F(x, cardY), Vec2F(96.0f, 68.0f));

		// steering keycaps of the controls step
		mKeys = mmake<Widget>();
		mKeys->SetName("tutorial keys");
		mRoot->AddChild(mKeys);
		mKeys->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mKeys->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mKeys->layout->offsetMin = Vec2F();
		mKeys->layout->offsetMax = Vec2F();

		MakePicture(mKeys, "Game/UI/tut_key_left.png", Vec2F(-119.0f, 0.0f), Vec2F(132.0f, 142.0f));
		MakePicture(mKeys, "Game/UI/tut_key_right.png", Vec2F(31.0f, 0.0f), Vec2F(132.0f, 142.0f));
		MakePicture(mKeys, "Game/UI/tut_key_space.png", Vec2F(280.0f, 0.0f), Vec2F(330.0f, 142.0f));

		// animated helper character on the left of every step screen: a chroma-keyed
		// looping video, drawn just above the dim so the flat green backdrop disappears
		if (auto pers = o2Assets.GetAssetRefByType<VideoAsset>(String("Game/Video/pers.mp4")))
		{
			mPersActor = mmake<Actor>(ActorCreateMode::InScene);
			mPersActor->SetName("tutorial pers");
			mPersActor->SetLayer(kUILayer);
			mPersActor->SetDrawingDepth(201.0f);

			mPersVideo = mPersActor->AddComponent<VideoComponent>();
			mPersVideo->SetVideoAsset(pers);
			mPersVideo->SetLoop(Loop::Repeat);
			mPersVideo->SetChromaKeyEnabled(true);
			mPersVideo->SetKeyColor(Color4(73, 106, 71, 255)); // the flat green of pers.mp4
			// the difference key measures rgb distance: the muted green key sits close to
			// the grey costume tones, a wide similarity makes the body translucent
			mPersVideo->SetSimilarity(0.1f);
			mPersVideo->SetSmoothness(0.06f);
			mPersVideo->SetSpill(0.4f);

			mPersActor->transform->SetSize2D(Vec2F(kPersHeight*9.0f/16.0f, kPersHeight));
			mPersActor->transform->SetPosition(Vec3F(kPersLeftX, kPersY, 0.0f));
			mPersActor->SetEnabled(false);
		}

		// fullscreen tap catcher on top of the overlay: on mobile web the taps arrive only
		// through the widget event system, polling the raw cursor state misses them
		mTapCatcher = mmake<Button>();
		mTapCatcher->SetName("tutorial tap catcher");
		mRoot->AddChild(mTapCatcher);
		mTapCatcher->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mTapCatcher->layout->anchorMax = Vec2F(1.0f, 1.0f);
		mTapCatcher->layout->offsetMin = Vec2F(-2000.0f, -2000.0f); // reaches into the window
		mTapCatcher->layout->offsetMax = Vec2F(2000.0f, 2000.0f);  // overscan past the UI rect
		mTapCatcher->onClick = [this]() { AdvanceStep(); };

		mRoot->SetEnabled(false);
	}

	Ref<Image> GameTutorialComponent::MakePicture(const Ref<Widget>& parent, const String& sprite,
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

	void GameTutorialComponent::PlaceLabel(const Ref<Label>& label, float y, float halfWidth,
								  float halfHeight)
	{
		label->layout->anchorMin = Vec2F(0.5f, 0.5f);
		label->layout->anchorMax = Vec2F(0.5f, 0.5f);
		label->layout->offsetMin = Vec2F(-halfWidth, y - halfHeight);
		label->layout->offsetMax = Vec2F(halfWidth, y + halfHeight);
	}

	void GameTutorialComponent::Start(GameSession* session)
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

	void GameTutorialComponent::Finish()
	{
		mActive = false;
		mWaiting = false;
		mStep = Step::Count; // closes the spotlight while the overlay fades out
		ApplyStep();
	}

	void GameTutorialComponent::ApplyStep()
	{
		mPictures->SetEnabled(mWaiting && mStep == Step::Intro);
		mKeys->SetEnabled(mWaiting && mStep == Step::Controls);
		mTitle->SetEnabled(mWaiting);
		if (mPersActor)
		{
			mPersActor->SetEnabled(mWaiting);
			// the fuel step spotlights the bottom-left panel: the character flips over to
			// the right edge; mirroring can't go through the transform (basis decomposition
			// drops the negative scale), so a pre-mirrored video swaps in instead
			bool onRight = mStep == Step::Fuel;
			mPersActor->transform->SetPosition(Vec3F(onRight ? kPersRightX : kPersLeftX,
													 kPersY, 0.0f));
			String persPath = onRight ? "Game/Video/pers_flip.mp4" : "Game/Video/pers.mp4";
			auto current = mPersVideo->GetVideoAsset();
			if (!current || current->GetPath() != persPath)
			{
				if (auto video = o2Assets.GetAssetRefByType<VideoAsset>(persPath))
				{
					mPersVideo->SetVideoAsset(video);
					mPersVideo->Play();
				}
			}
		}
		if (mTapCatcher)
			mTapCatcher->SetEnabled(mWaiting);
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

	void GameTutorialComponent::AdvanceStep()
	{
		if (!mActive || !mWaiting || mTapHold > 0.0f)
			return;

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

	bool GameTutorialComponent::IsLiveSegmentOver() const
	{
		switch (mStep)
		{
			// enough tokens loaded, or the car has driven out of the source radius
			case Step::Loading:
			return !mSession || mSession->GetTokens() >= kLoadedTokens ||
				   (mWasFilling && !mSession->IsFilling()) || mLiveTime > loadingTimeout;

			case Step::Controls: return mLiveTime > controlsDriveTime;
			case Step::Fuel: return mLiveTime > fuelDriveTime;
			default: return true;
		}
	}

	void GameTutorialComponent::UpdateHole(float dt)
	{
		Vec2F center = mHoleCenter;
		Vec2F radius;

		// the car is spotlighted while it loads and while it shows off the steering; the
		// keycaps of the paused controls step read better over a plain dim
		bool carSpot = mStep == Step::Loading || (mStep == Step::Controls && !mWaiting);
		if (carSpot && mSession && worldToUI)
		{
			center = worldToUI(CellToScreen(mSession->GetCar().GetVisualPos())) + Vec2F(0.0f, 12.0f);
			radius = Vec2F(carSpotRadius, carSpotRadius);
		}
		else if (mStep == Step::Fuel)
		{
			center = kFuelSpotCenter;
			radius = kFuelSpotRadius;
		}

		float lerp = Math::Min(1.0f, dt*8.0f);
		mHoleCenter = mHoleRadius.x < 1.0f ? center : Math::Lerp(mHoleCenter, center, lerp);
		mHoleRadius = Math::Lerp(mHoleRadius, radius, lerp);
		mDimMesh->SetHole(mHoleCenter, mHoleRadius);
	}

	void GameTutorialComponent::Update(float dt)
	{
		if (!mRoot || !mRoot->IsEnabled())
			return;

		mFade = Math::Clamp01(mFade + (mActive ? dt*4.0f : -dt*3.0f));
		mRoot->SetTransparency(mFade);
		if (mPersVideo)
			mPersVideo->SetTransparency(mFade); // a plain actor is outside the widget fade

		if (!mActive)
		{
			UpdateHole(dt);
			if (mFade <= 0.0f)
				mRoot->SetEnabled(false);
			return;
		}

		mTapHold = Math::Max(0.0f, mTapHold - dt);

		if (mWaiting)
		{
			// taps go through the tap catcher button; here only the any-key path
			if (!o2Input.GetPressedKeys().IsEmpty())
				AdvanceStep();
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

	void GameTutorialComponent::Clear()
	{
		if (mRoot)
			mRoot->RemoveFromScene();

		mRoot = nullptr;
		mDim = nullptr;
		mDimMesh = nullptr;
		mTapCatcher = nullptr;
		if (mPersActor)
			mPersActor->RemoveFromScene();
		mPersActor = nullptr;
		mPersVideo = nullptr;
		mTitle = nullptr;
		mDescription = nullptr;
		mHint = nullptr;
		mPictures = nullptr;
		mKeys = nullptr;
		mSession = nullptr;
		mActive = false;
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<td::GameTutorialComponent>);
// --- META ---

ENUM_META(td::GameTutorialComponent::Step, td__GameTutorialComponent__Step)
{
    ENUM_ENTRY(Controls);
    ENUM_ENTRY(Count);
    ENUM_ENTRY(Fuel);
    ENUM_ENTRY(Intro);
    ENUM_ENTRY(Loading);
}
END_ENUM_META;

DECLARE_CLASS(td::GameTutorialComponent, td__GameTutorialComponent);
// --- END META ---
