#include "o2/stdafx.h"
#include "TokenDelivery/SplashScreen.h"

#include "TokenDelivery/GameUIStyle.h"
#include "o2/Animation/AnimationClip.h"
#include "o2/Render/Text.h"
#include "o2/Scene/Components/AnimationComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayout.h"

namespace td
{
	static const char* kSplashLayer = "Splash";

	void SplashScreen::Show()
	{
		o2Scene.AddLayer(kSplashLayer);

		mCamera = mmake<CameraActor>();
		mCamera->SetName("splash camera");
		mCamera->SetFittedSize(Vec2F(1280.0f, 800.0f));
		mCamera->drawLayers.SetLayers(Vector<String>{ kSplashLayer });
		mCamera->fillBackground = true;
		mCamera->fillColor = Color4(255, 255, 255, 255);

		mCaption = mmake<Widget>();
		mCaption->SetName("splash caption");
		mCaption->SetLayer(kSplashLayer);
		mCaption->layout->anchorMin = Vec2F(0.0f, 0.0f);
		mCaption->layout->anchorMax = Vec2F(0.0f, 0.0f);
		mCaption->layout->offsetMin = Vec2F(-500.0f, -100.0f);
		mCaption->layout->offsetMax = Vec2F(500.0f, 100.0f);

		auto text = mmake<Text>(GameUIFont());
		text->SetText(L"made with o2 engine");
		text->SetHeight(64);
		text->SetColor(Color4(34, 41, 65, 255));
		text->SetHorAlign(HorAlign::Middle);
		text->SetVerAlign(VerAlign::Middle);
		mCaption->AddLayer("text", text);

		// slow slight growth over the whole splash time, engine animation
		auto grow = AnimationClip::Linear<Vec3F>("transform/scale", Vec3F(1.0f, 1.0f, 1.0f),
												 Vec3F(1.08f, 1.08f, 1.0f), kDuration);
		mCaption->AddComponent<AnimationComponent>()->Play(grow, "grow");

		mTime = 0.0f;
	}

	void SplashScreen::Update(float dt)
	{
		// the boot frames can hitch, a clamp keeps the splash visible for its full time
		mTime += Math::Min(dt, 1.0f/20.0f);
	}

	bool SplashScreen::IsFinished() const
	{
		return mTime >= kDuration;
	}

	void SplashScreen::Clear()
	{
		if (mCamera)
			mCamera->RemoveFromScene();
		mCamera = nullptr;

		if (mCaption)
			mCaption->RemoveFromScene();
		mCaption = nullptr;
	}
}
