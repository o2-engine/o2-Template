#include "o2/stdafx.h"
#include "RotatorComponent.h"

#include "o2/Scene/Actor.h"

void RotatorComponent::OnUpdate(float dt)
{
	if (auto owner = GetActor())
		owner->transform->angleDegrees = owner->transform->angleDegrees.Get() + speed*dt;
}
// --- META ---

DECLARE_CLASS(RotatorComponent, RotatorComponent);
// --- END META ---
