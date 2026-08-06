#include "o2/stdafx.h"
#include "DragonInputComponent.h"

#include "DragonLayout.h"
#include "o2/Application/Input.h"
#include "o2/Integration.h"
#include "o2/Render/Render.h"

bool DragonInputComponent::IsClicked() const
{
	return o2Input.IsCursorPressed();
}

bool DragonInputComponent::IsRightClicked() const
{
	return o2Input.IsRightMousePressed();
}

Vec2F DragonInputComponent::GetCursorWorld() const
{
	return ScreenToWorld(o2Input.GetCursorPos());
}

float DragonInputComponent::WorldPerScreenPixel()
{
	if (Integration::IsHeadless())
		return 1.0f;

	// Mirrors Camera::FittedSize: the camera shows kScreenSize scaled up to fit the window
	Vec2F resolution = o2Render.GetCurrentResolution();
	if (resolution.x < 1.0f || resolution.y < 1.0f)
		return 1.0f;

	Vec2F scaled = resolution*(dragon::kScreenSize.x/resolution.x);
	if (scaled.y < dragon::kScreenSize.y)
		scaled = resolution*(dragon::kScreenSize.y/resolution.y);

	return scaled.x/resolution.x;
}

Vec2F DragonInputComponent::ScreenToWorld(const Vec2F& screen)
{
	return screen*WorldPerScreenPixel();
}

Vec2F DragonInputComponent::WorldToScreen(const Vec2F& world)
{
	return world/WorldPerScreenPixel();
}
// --- META ---

DECLARE_CLASS(DragonInputComponent, DragonInputComponent);
// --- END META ---
