#include "o2/stdafx.h"
#include "GameApplication.h"

#include "TokenDelivery/TokenDeliveryGame.h"
#include "o2/Assets/Assets.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Debug/Debug.h"

GameApplication::GameApplication(RefCounter* refCounter):
	Application(refCounter)
{}

void GameApplication::OnStarted()
{
	o2Application.SetWindowSize(Vec2I(1280, 800));

	mSplash.Show();
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("Token Delivery") +
		"; FPS: " + (String)((int)o2Time.GetFPS());

	if (!mGameStarted)
	{
		mSplash.Update(dt);
		if (mSplash.IsFinished())
		{
			// booting here is safe: the application update runs outside the scene update,
			// and loading the bootstrap scene clears the whole scene, splash included
			mSplash.Clear();
			td::LaunchTokenDelivery();
			mGameStarted = true;
		}
	}
}

void GameApplication::OnDraw()
{
	o2Render.camera = Camera::Default();
}
