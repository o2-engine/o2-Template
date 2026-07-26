#pragma once

#include "o2/Scene/CameraActor.h"
#include "o2/Scene/UI/Widget.h"

using namespace o2;

namespace td
{
	// -------------------------------------------------------------------------------------
	// Boot splash: a white screen with a large dark "made with o2 engine" caption slowly
	// growing for about a second before the game scene loads. Owned and driven by
	// GameApplication; lives on its own scene layer with its own camera, so starting the
	// game simply replaces it with the bootstrap scene.
	// -------------------------------------------------------------------------------------
	class SplashScreen
	{
	public:
		// Creates the splash camera and caption and starts the grow animation
		void Show();

		// Advances the splash timer
		void Update(float dt);

		// Returns has the splash played its full time
		bool IsFinished() const;

		// Removes the splash actors
		void Clear();

	private:
		static constexpr float kDuration = 1.1f; // Splash time, seconds

		Ref<CameraActor> mCamera;      // White-filled splash camera
		Ref<Widget>      mCaption;     // Caption widget with the growing text
		float            mTime = 0.0f; // Time since the splash has been shown
	};
}
