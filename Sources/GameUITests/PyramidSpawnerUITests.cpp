#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Application/Input.h"
#include "o2/Application/VKCodes.h"
#include "o2/Assets/Assets.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/ActorTransform.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";
}

// Loads the Main scene (which carries a PyramidSpawner actor), and drives the real frame loop to
// verify the pyramid/projectile spawn and that pressing H launches the reused projectile.
class PyramidScene: public ::testing::Test
{
protected:
	void SetUp() override
	{
		o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Main.scn"));
		AppTestDriver::PumpFrames(5); // let OnStart build the pyramid, floor and projectile
	}

	void TearDown() override
	{
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}
};

TEST_F(PyramidScene, PyramidFloorAndProjectileSpawned)
{
	EXPECT_TRUE(o2Scene.FindActor("pyramid box"));
	EXPECT_TRUE(o2Scene.FindActor("pyramid floor"));
	ASSERT_TRUE(o2Scene.FindActor("projectile"));

	// The projectile is parked (kinematic) at the muzzle and must not fall before being fired.
	AppTestDriver::Wait(0.5f);
	EXPECT_GT(o2Scene.FindActor("projectile")->transform->GetPosition().z, 50.0f);
}

TEST_F(PyramidScene, KeyHLaunchesProjectileTowardPyramid)
{
	auto projectile = o2Scene.FindActor("projectile");
	ASSERT_TRUE(projectile);

	AppTestDriver::Wait(0.5f); // let the pyramid settle

	float startY = projectile->transform->GetPosition().y;

	// Press H for one frame.
	o2Input.OnKeyPressed(VK_H);
	AppTestDriver::PumpFrames(1);
	o2Input.OnKeyReleased(VK_H);

	AppTestDriver::Wait(1.2f); // catch the big slow projectile mid-flight for the screenshot
	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	AppTestDriver::SaveScreenshot(kScreenshotsDir + "pyramid_launch.png");

	AppTestDriver::Wait(1.3f); // let it reach the pyramid's middle
	Vec3F endPos = projectile->transform->GetPosition();
	EXPECT_GT(endPos.y, startY + 150.0f); // launched forward toward the pyramid
	EXPECT_GT(endPos.z, 100.0f);          // stayed near the middle height, did not drop to the ground
}
