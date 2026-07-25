#include "o2/stdafx.h"
#include "TokenDelivery/TokenDeliveryGame.h"

#include "TokenDelivery/GameControllerComponent.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Scene.h"

namespace td
{
	void LaunchTokenDelivery()
	{
		Actor::SetDefaultCreationMode(ActorCreateMode::InScene);

		auto root = mmake<Actor>(ActorCreateMode::InScene);
		root->SetName("token delivery");
		root->AddComponent<GameControllerComponent>();
	}
}
