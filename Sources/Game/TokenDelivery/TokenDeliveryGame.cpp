#include "o2/stdafx.h"
#include "TokenDelivery/TokenDeliveryGame.h"

#include "TokenDelivery/GameControllerComponent.h"
#include "o2/Assets/Assets.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/FileSystem/FileSystem.h"

namespace td
{
	void LaunchTokenDelivery()
	{
		Actor::SetDefaultCreationMode(ActorCreateMode::InScene);

		// the bootstrap scene carries the cameras and the game actor with all components
		// linked; the same scene starts the game from the editor in play mode
		String scenePath = o2Assets.GetBuiltAssetsPath() + "Bootstrap.scn";
		if (o2FileSystem.IsFileExist(scenePath))
		{
			o2Scene.Load(scenePath);
			return;
		}

		// fallback for a build before the assets generator has been run
		auto root = mmake<Actor>(ActorCreateMode::InScene);
		root->SetName("token delivery");
		root->AddComponent<GameControllerComponent>();
	}
}
