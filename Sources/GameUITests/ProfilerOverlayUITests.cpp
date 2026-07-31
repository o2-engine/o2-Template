#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "TokenDelivery/GameControllerComponent.h"
#include "TokenDelivery/TokenDeliveryGame.h"
#include "o2/Application/Application.h"
#include "o2/Application/VKCodes.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Utils/Debug/Profiling/NanoProfiler.h"
#include "o2/Utils/Debug/Profiling/ProfilerOverlay.h"
#include "o2/Utils/Debug/Profiling/ProfilerWidget.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/System/Time/Timer.h"
#include "o2/Utils/Test/AppTestDriver.h"

#if defined(O2_PROFILER_ENABLED)

using namespace o2;
using namespace td;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";

	// Runs the game with the profiler panel up, so the timeline shows real game scopes
	class ProfilerOverlayApp: public ::testing::Test
	{
	protected:
		ProfilerOverlay* overlay = nullptr;

		void SetUp() override
		{
			GameControllerComponent::sForcedSeed = 1234567u;
			GameControllerComponent::sTutorialEnabled = false;
			td::LaunchTokenDelivery();
			AppTestDriver::PumpFrames(8);

			overlay = ProfilerOverlay::InstancePtr();
			ASSERT_NE(overlay, nullptr);
			overlay->SetVisible(false);
		}

		void TearDown() override
		{
			if (overlay)
				overlay->SetVisible(false);

			GameControllerComponent::sForcedSeed = 0;
			GameControllerComponent::sTutorialEnabled = true;
			o2Scene.Clear(true);
			o2Scene.UpdateDestroyingEntities();
			AppTestDriver::PumpFrames(2);
		}
	};
}

// F12 brings the panel up over the running game and the timeline fills with the game's own scopes
TEST_F(ProfilerOverlayApp, F12ShowsThePanelOverTheGame)
{
	o2Input.OnKeyPressed(VK_F12);
	AppTestDriver::PumpFrames(1);
	o2Input.OnKeyReleased(VK_F12);

	AppTestDriver::PumpFrames(40);

	ASSERT_TRUE(overlay->IsVisible());
	EXPECT_TRUE(NanoProfiler::IsEnabled());
	EXPECT_GT(NanoProfiler::GetFrameSamplesCount(), 0) << "the game frame must be profiled";

	auto widget = overlay->GetWidget();
	EXPECT_GT(widget->GetHistoryCount(), 30);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "profiler_overlay.png"));
}

// The panel hangs on the top left corner of the screen, flush with it
TEST_F(ProfilerOverlayApp, PanelSitsInTheTopLeftCorner)
{
	overlay->SetVisible(true);
	AppTestDriver::PumpFrames(3);

	const RectF panel = overlay->GetWidget()->layout->GetWorldRect();
	const Vec2F content = (Vec2F)o2Application.GetContentSize();

	EXPECT_NEAR(panel.left, -content.x*0.5f, 1.0f);
	EXPECT_NEAR(panel.top, content.y*0.5f, 1.0f);
	EXPECT_LT(panel.right, content.x*0.5f);
	EXPECT_GT(panel.bottom, -content.y*0.5f);
}

// Hovering the timeline freezes it: the frame count stops growing while the cursor is over it
TEST_F(ProfilerOverlayApp, HoveringTheTimelineFreezesItAndDetailsAFrame)
{
	overlay->SetVisible(true);
	AppTestDriver::PumpFrames(30);

	auto widget = overlay->GetWidget();
	ASSERT_GT(widget->GetHistoryCount(), 10);

	const RectF panel = widget->layout->GetWorldRect();
	// over the recently recorded part of the timeline, near its right end
	AppTestDriver::MoveCursor(Vec2F(panel.left + 260.0f, panel.top - 60.0f));
	AppTestDriver::PumpFrames(2);

	// the timeline freezes once the cursor is over it
	const int frozenAt = widget->GetHistoryCount();
	AppTestDriver::PumpFrames(10);

	EXPECT_GE(widget->GetDetailedFrame(), 0);
	EXPECT_EQ(widget->GetHistoryCount(), frozenAt);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "profiler_overlay_detailed.png"));

	AppTestDriver::MoveCursor(Vec2F(0.0f, 0.0f));
	AppTestDriver::PumpFrames(5);

	EXPECT_EQ(widget->GetDetailedFrame(), -1);
}

// A long tap in the top left corner is the touch equivalent of F12
TEST_F(ProfilerOverlayApp, LongTapInTheCornerShowsAndHidesThePanel)
{
	const Vec2F content = (Vec2F)o2Application.GetContentSize();
	const Vec2F corner(-content.x*0.5f + 25.0f, content.y*0.5f - 25.0f);

	AppTestDriver::PressCursor(corner);
	AppTestDriver::Wait(ProfilerOverlay::longTapTime + 0.2f);
	AppTestDriver::ReleaseCursor();
	EXPECT_TRUE(overlay->IsVisible());

	AppTestDriver::PressCursor(corner);
	AppTestDriver::Wait(ProfilerOverlay::longTapTime + 0.2f);
	AppTestDriver::ReleaseCursor();
	EXPECT_FALSE(overlay->IsVisible());
}

// Hidden panel means no recording: profiling scopes cost one branch and store nothing
TEST_F(ProfilerOverlayApp, HiddenPanelStopsRecording)
{
	overlay->SetVisible(true);
	AppTestDriver::PumpFrames(5);
	EXPECT_GT(NanoProfiler::GetFrameSamplesCount(), 0);

	overlay->SetVisible(false);
	AppTestDriver::PumpFrames(5);

	EXPECT_FALSE(NanoProfiler::IsEnabled());
	EXPECT_EQ(NanoProfiler::GetFrameSamplesCount(), 0);
}

// The baseline button and the resize grip light up under the cursor
TEST_F(ProfilerOverlayApp, ControlsHighlightUnderTheCursor)
{
	overlay->SetVisible(true);
	AppTestDriver::PumpFrames(10);

	auto widget = overlay->GetWidget();
	const RectF panel = widget->layout->GetWorldRect();

	AppTestDriver::MoveCursor(Vec2F(panel.right - 25.0f, panel.top - 10.0f));
	AppTestDriver::PumpFrames(2);
	EXPECT_TRUE(widget->IsBaselineHovered());

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "profiler_overlay_hover_base.png"));

	AppTestDriver::MoveCursor(Vec2F(panel.right - 5.0f, panel.bottom + 5.0f));
	AppTestDriver::PumpFrames(2);
	EXPECT_FALSE(widget->IsBaselineHovered());
	EXPECT_TRUE(widget->IsResizeGripHovered());

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "profiler_overlay_hover_grip.png"));

	AppTestDriver::MoveCursor(Vec2F());
	AppTestDriver::PumpFrames(2);
	EXPECT_FALSE(widget->IsResizeGripHovered());
}

// The panel is dragged around by its caption bar, and can't be dragged off the screen
TEST_F(ProfilerOverlayApp, CaptionBarDragsThePanelAndKeepsItOnScreen)
{
	overlay->SetVisible(true);
	AppTestDriver::PumpFrames(5);

	auto widget = overlay->GetWidget();
	const RectF before = widget->layout->GetWorldRect();

	const Vec2F grab(before.left + 40.0f, before.top - 8.0f);
	const Vec2F delta(200.0f, -120.0f);

	AppTestDriver::Drag(grab, grab + delta);
	AppTestDriver::PumpFrames(2);

	const RectF dragged = widget->layout->GetWorldRect();
	EXPECT_NEAR(dragged.left, before.left + delta.x, 3.0f);
	EXPECT_NEAR(dragged.top, before.top + delta.y, 3.0f);
	EXPECT_NEAR(dragged.Width(), before.Width(), 1.0f);

	// pulled far past the screen corner the panel stops at it, it can't be lost off screen
	const Vec2F grownGrab(dragged.left + 40.0f, dragged.top - 8.0f);
	AppTestDriver::Drag(grownGrab, grownGrab + Vec2F(4000.0f, 4000.0f));
	AppTestDriver::PumpFrames(2);

	const Vec2F content = (Vec2F)o2Application.GetContentSize();
	const RectF clamped = widget->layout->GetWorldRect();

	EXPECT_NEAR(clamped.right, content.x*0.5f, 1.0f);
	EXPECT_NEAR(clamped.top, content.y*0.5f, 1.0f);

	widget->SetContentOffset(Vec2F());
	AppTestDriver::PumpFrames(2);
}

// The panel's parts are drawn where the panel is, not where it was on the previous frame: the drag
// moves it after the update, and the button and the graph used to be placed against the old position.
// Checked while the drag is still held, that is the only moment the lag shows
TEST_F(ProfilerOverlayApp, DraggedPanelPartsFollowItOnTheSameFrame)
{
	overlay->SetVisible(true);
	AppTestDriver::PumpFrames(5);

	auto widget = overlay->GetWidget();
	const RectF before = widget->layout->GetWorldRect();

	const Vec2F grab(before.left + 40.0f, before.top - 8.0f);
	AppTestDriver::PressCursor(grab);
	AppTestDriver::MoveCursor(grab + Vec2F(180.0f, -100.0f), 1);

	const RectF panel = widget->layout->GetWorldRect();
	ASSERT_NEAR(panel.left, before.left + 180.0f, 3.0f);

	// the frame that moved the panel drew its parts in the new place, not the previous one
	EXPECT_NEAR(widget->GetBaselineButtonRect().right, panel.right, 1.0f);
	EXPECT_NEAR(widget->GetBaselineButtonRect().top, panel.top, 1.0f);
	EXPECT_NEAR(widget->GetTimelineRect().left, panel.left + 6.0f, 1.0f);

	AppTestDriver::ReleaseCursor();

	widget->SetContentOffset(Vec2F());
	AppTestDriver::PumpFrames(2);
}

// The grip in the bottom right corner follows the cursor in both axes, the top left corner stays put
TEST_F(ProfilerOverlayApp, CornerGripResizesThePanelAlongBothAxes)
{
	overlay->SetVisible(true);
	AppTestDriver::PumpFrames(30);

	auto widget = overlay->GetWidget();
	const RectF before = widget->layout->GetWorldRect();
	ASSERT_EQ(widget->GetContentSize(), widget->GetDesignSize());

	const Vec2F grip(before.right - 6.0f, before.bottom + 6.0f);
	const Vec2F drag(220.0f, -140.0f); // right and down: y grows upwards, so down is negative

	AppTestDriver::Drag(grip, grip + drag);
	AppTestDriver::PumpFrames(3);

	const RectF after = widget->layout->GetWorldRect();
	EXPECT_NEAR(after.Width(), before.Width() + drag.x, 2.0f);
	EXPECT_NEAR(after.Height(), before.Height() - drag.y, 2.0f);
	EXPECT_NEAR(after.left, before.left, 1.0f);
	EXPECT_NEAR(after.top, before.top, 1.0f);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "profiler_overlay_resized.png"));

	// and back: dragging up and left shrinks it to the minimum it still reads at
	const Vec2F grownGrip(after.right - 6.0f, after.bottom + 6.0f);
	AppTestDriver::Drag(grownGrip, grownGrip - Vec2F(1000.0f, -1000.0f));
	AppTestDriver::PumpFrames(3);

	EXPECT_EQ(widget->GetContentSize(), widget->GetMinContentSize());

	widget->SetContentSize(widget->GetDesignSize());
	AppTestDriver::PumpFrames(2);
}

// Reports what the panel costs a real game frame. Wall clock frame times can't be asserted on: the
// test suites run in parallel and compete for the GPU. The perf guards live in the engine tier
// benchmarks (NanoProfilerBenchmark, ProfilerWidgetFixture.FullPanelUpdateAndDrawStayCheap)
TEST_F(ProfilerOverlayApp, ReportsWhatThePanelCostsAFrame)
{
	// Best of several batches: the suites run in parallel, and a scheduling hiccup in one batch would
	// otherwise decide the number
	const int kFrames = 40;
	const int kBatches = 5;

	auto measureFrameMs = [&]()
	{
		AppTestDriver::PumpFrames(20); // settle

		float best = FLT_MAX;
		for (int batch = 0; batch < kBatches; batch++)
		{
			Timer timer;
			timer.GetDeltaTime();
			AppTestDriver::PumpFrames(kFrames);

			best = Math::Min(best, timer.GetDeltaTime()*1000.0f/kFrames);
		}

		return best;
	};

	overlay->SetVisible(false);
	NanoProfiler::SetEnabled(false);
	const float withoutPanel = measureFrameMs();

	NanoProfiler::SetEnabled(true);
	const float recordingOnly = measureFrameMs();
	NanoProfiler::SetEnabled(false);

	overlay->SetVisible(true);
	const float withPanel = measureFrameMs();

	o2Debug.Log("ProfilerOverlay: frame %.3f ms plain, %.3f ms recording only (+%.3f), %.3f ms with the panel (+%.3f)",
	            withoutPanel, recordingOnly, recordingOnly - withoutPanel, withPanel, withPanel - withoutPanel);

	EXPECT_TRUE(overlay->IsVisible());
	EXPECT_TRUE(NanoProfiler::IsEnabled());
}

#endif // O2_PROFILER_ENABLED
