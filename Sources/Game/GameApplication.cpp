#include "o2/stdafx.h"
#include "GameApplication.h"

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

	// The main scene shows a deferred 3D layer with a 2D overlay on top;
	// the same scene is opened by the editor
	o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Main.scn"));
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("o2 Template") +
		"; FPS: " + (String)((int)o2Time.GetFPS());
}

void GameApplication::OnDraw()
{
	o2Render.camera = Camera::Default();
}
