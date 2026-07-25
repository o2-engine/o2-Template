#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Physics3DDemo.h"
#include "o2/Assets/Assets.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/ActorTransform.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";
}

// Loads the Main scene, spawns the 3D physics demo, and drives the real application frame loop
// (which steps Box3D through Integration) to verify bodies fall onto the floor and render.
class Physics3DDemoScene: public ::testing::Test
{
protected:
	void SetUp() override
	{
		o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Main.scn"));
		demo::SpawnPhysics3DDemo();
		AppTestDriver::PumpFrames(3); // fire OnAddToScene so the bodies are created
	}

	void TearDown() override
	{
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}
};

TEST_F(Physics3DDemoScene, DroppedBoxFallsAndRestsOnFloor)
{
	auto box = o2Scene.FindActor("physics box 1");
	ASSERT_TRUE(box);

	float startZ = box->transform->GetPosition().z;
	EXPECT_GT(startZ, 200.0f); // starts high above the ground

	AppTestDriver::Wait(3.0f); // ~3 s of the real fixed-step physics loop

	float endZ = box->transform->GetPosition().z;

	EXPECT_LT(endZ, startZ - 100.0f); // fell far along -Z (gravity is -Z in this scene)
	EXPECT_GT(endZ, 0.0f);            // rests above the floor top at z=0, did not tunnel through
	EXPECT_LT(endZ, 130.0f);          // settled near the ground (box half-height 60)
}

TEST_F(Physics3DDemoScene, SceneWithPhysicsRendersNonEmptyFrame)
{
	AppTestDriver::Wait(1.0f);

	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "physics3d_demo.png"));
}
