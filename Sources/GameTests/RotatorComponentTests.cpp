#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "RotatorComponent.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Scene.h"
#include "Scene/SceneTestHelpers.h"

using namespace o2;

TEST(Rotator, RotatesOwnerActor)
{
	SceneCleanGuard sceneGuard;

	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	auto rotator = actor->AddComponent<RotatorComponent>();
	rotator->speed = 90.0f;

	TickFrame();
	float angleBefore = actor->transform->angleDegrees.Get();

	TickFrames(10, 0.1f);

	float rotated = actor->transform->angleDegrees.Get() - angleBefore;
	EXPECT_NEAR(rotated, 90.0f, 5.0f);
}
