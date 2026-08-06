#include "o2/stdafx.h"
#include "GameApplication.h"

#include "DragonDefense/DragonDefenseBootstrap.h"
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

	// Dragon Defense builds its scene in code from the bootstrap component
	DragonDefenseBootstrap::CreateBootstrapActor();
}

void GameApplication::OnUpdate(float dt)
{
	o2Application.windowCaption = String("Dragon Defense: Merge & Blast") +
		"; FPS: " + (String)((int)o2Time.GetFPS());
}

void GameApplication::OnDraw()
{
	o2Render.camera = Camera::Default();
}
