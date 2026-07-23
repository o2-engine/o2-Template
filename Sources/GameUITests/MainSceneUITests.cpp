#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "RotatorComponent.h"
#include "o2/Assets/Assets.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/LightComponent.h"
#include "o2/Scene/Components/MeshPrimitiveComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";

	int CountDistinctColors(const Ref<Bitmap>& bitmap)
	{
		if (!bitmap)
			return 0;

		Vector<UInt32> seen;
		const UInt32* pixels = reinterpret_cast<const UInt32*>(bitmap->GetData());
		Vec2I size = bitmap->GetSize();
		for (int y = 0; y < size.y; y += 16)
		{
			for (int x = 0; x < size.x; x += 16)
			{
				UInt32 color = pixels[y*size.x + x];
				if (!seen.Contains(color))
					seen.Add(color);
			}
		}

		return seen.Count();
	}
}

class MainSceneUI: public ::testing::Test
{
protected:
	void SetUp() override
	{
		o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("Main.scn"));
		AppTestDriver::PumpFrames(5); // settle transforms and prime the cameras
	}

	void TearDown() override
	{
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}
};

TEST_F(MainSceneUI, LoadsBothCameras)
{
	Vector<Ref<CameraActor>> cameras;
	for (auto& weakCamera : o2Scene.GetCameras())
	{
		if (auto camera = weakCamera.Lock())
			cameras.Add(camera);
	}

	ASSERT_EQ(cameras.Count(), 2);
	EXPECT_TRUE(cameras.FindOrDefault([](auto& x) { return x->GetName() == "camera 3d"; }));
	EXPECT_TRUE(cameras.FindOrDefault([](auto& x) { return x->GetName() == "ui camera"; }));
}

TEST_F(MainSceneUI, Has3DObjects)
{
	for (auto name : { "ground", "box", "sphere", "cylinder" })
	{
		auto actor = o2Scene.FindActor(name);
		ASSERT_TRUE(actor) << name;
		EXPECT_TRUE(actor->GetComponent<MeshPrimitiveComponent>()) << name;
	}

	auto sun = o2Scene.FindActor("sun");
	ASSERT_TRUE(sun);
	EXPECT_TRUE(sun->GetComponent<LightComponent>());

	auto pointLight = o2Scene.FindActor("point light");
	ASSERT_TRUE(pointLight);
	EXPECT_TRUE(pointLight->GetComponent<LightComponent>());
}

TEST_F(MainSceneUI, Has2DObjects)
{
	auto logo = o2Scene.FindActor("logo");
	ASSERT_TRUE(logo);
	EXPECT_TRUE(logo->GetComponent<ImageComponent>());

	auto gem = o2Scene.FindActor("gem");
	ASSERT_TRUE(gem);
	EXPECT_TRUE(gem->GetComponent<ImageComponent>());
	EXPECT_TRUE(gem->GetComponent<RotatorComponent>());

	EXPECT_TRUE(o2Scene.FindActor("title label"));
}

// The composed frame shows the deferred 3D scene with the 2D overlay on top
TEST_F(MainSceneUI, SceneRendersNonEmptyFrame)
{
	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);
	EXPECT_GT(CountDistinctColors(bitmap), 8) << "the lit 3D scene with 2D overlay is far from a blank frame";

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "main_scene.png"));
}
