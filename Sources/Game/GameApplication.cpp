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

	td::LaunchTokenDelivery();
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("Token Delivery") +
		"; FPS: " + (String)((int)o2Time.GetFPS());
}

void GameApplication::OnDraw()
{
	o2Render.camera = Camera::Default();
}
