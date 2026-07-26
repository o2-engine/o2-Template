#include "o2/stdafx.h"
#include "TokenDelivery/TrafficCarComponent.h"

#include "TokenDelivery/GameControllerComponent.h"
#include "o2/Scene/Actor.h"

namespace td
{
	TrafficCarComponent::TrafficCarComponent():
		TrafficCarComponent(nullptr)
	{}

	TrafficCarComponent::TrafficCarComponent(RefCounter* refCounter):
		Component(refCounter)
	{}

	void TrafficCarComponent::Spawn(const Ref<GameControllerComponent>& controller,
									const CarTuning& playerTuning, const Vec2F& cell, Dir dir)
	{
		mController = controller;

		CarTuning tuning = playerTuning;
		tuning.maxSpeed *= speedScale;
		tuning.accel *= accelScale;
		mSim.Reset(tuning, cell, dir);

		mDrawable = GetActor()->GetComponent<CarDrawableComponent>();
		mAIScript = GetActor()->GetComponent<ScriptableComponent>();
	}

	void TrafficCarComponent::ReadAIDecision()
	{
		if (!mAIScript)
			return;

		auto instance = mAIScript->GetInstance();
		if (!instance.IsObject())
			return;

		// the script pulses a decision; consume it so the turn is held for one tick only,
		// like a driver flicking the wheel exactly at the crossroad
		if (instance.GetProperty("_turnLeft").ToBool())
		{
			mInput.turnLeft = true;
			instance.SetProperty("_turnLeft", false);
		}
		if (instance.GetProperty("_turnRight").ToBool())
		{
			mInput.turnRight = true;
			instance.SetProperty("_turnRight", false);
		}
	}

	void TrafficCarComponent::OnUpdate(float dt)
	{
		if (!mController || mController->IsWorldPaused())
			return;

		dt = Math::Min(dt, 1.0f/20.0f);

		ReadAIDecision();
		mSim.Tick(dt, mInput, mController->GetSession().GetCity());
		mInput.turnLeft = false;
		mInput.turnRight = false;

		if (mDrawable)
			mDrawable->SetPose(mSim.GetVisualPos(), mSim.GetVisualAngle(),
							   mSim.GetDriftIntensity()*0.5f);

		Vec2F screen = CellToScreen(mSim.GetVisualPos());
		GetActor()->transform->SetPosition(Vec3F(screen.x, screen.y, 0.0f));
		GetActor()->SetDrawingDepth(IsoDepth(mSim.GetVisualPos()));
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<td::TrafficCarComponent>);
// --- META ---

DECLARE_CLASS(td::TrafficCarComponent, td__TrafficCarComponent);
// --- END META ---
