#pragma once

#include "o2/Scene/Component.h"

using namespace o2;

// Rotates the owner actor around the Z axis with a constant speed. A minimal
// example of a game component: serializable field, editable in the editor,
// registered in reflection by the CodeTool
class RotatorComponent: public Component
{
public:
	float speed = 45.0f; // Rotation speed in degrees per second @SERIALIZABLE @EDITOR_PROPERTY

	SERIALIZABLE(RotatorComponent);
	CLONEABLE_REF(RotatorComponent);

private:
	// Called each frame; rotates the owner actor
	void OnUpdate(float dt) override;

	REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(RotatorComponent)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(RotatorComponent)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(45.0f).NAME(speed);
}
END_META;
CLASS_METHODS_META(RotatorComponent)
{

    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
}
END_META;
// --- END META ---
