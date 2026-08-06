#include "o2/stdafx.h"

#include <gtest/gtest.h>

#include "DragonDefense/DragonDefenseBootstrap.h"
#include "o2/Assets/Assets.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "Scene/SceneTestHelpers.h"

using namespace o2;

// Builds the bootstrap scene in code and saves it as the editor entry scene asset.
// The scene holds only the bootstrap actor, so it is safe to serialize headless.
TEST(DragonScene, SavesAndReloadsBootstrapScene)
{
	SceneCleanGuard sceneGuard;

	String path = o2Assets.GetAssetsPath() + "DragonDefense/DragonDefense.scn";
	DragonDefenseBootstrap::SaveBootstrapScene(path);
	EXPECT_TRUE(o2FileSystem.IsFileExist(path));

	o2Scene.Clear(true);
	o2Scene.UpdateDestroyingEntities();

	o2Scene.Load(path);
	auto bootstrap = o2Scene.FindActor("Bootstrap");
	ASSERT_TRUE(bootstrap);
	EXPECT_TRUE(bootstrap->GetComponent<DragonDefenseBootstrap>());
}
