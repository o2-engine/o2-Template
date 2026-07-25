#pragma once

#include "o2/Scene/Component.h"
#include "o2/Scene/Physics3D/RigidBody3D.h"

using namespace o2;

// Spawns a pyramid of physics boxes (plus a static floor) at the owner actor's position, and keeps a
// single reusable projectile box parked in front of it. Press H to fire the projectile into the
// pyramid; each press resets the same box back to the muzzle and launches it again. A game-side
// example of the o2 3D physics API (Box3D). Serializable fields are editable in the editor.
class PyramidSpawner: public Component
{
public:
    int   rows = 4;              // Number of pyramid rows @SERIALIZABLE @EDITOR_PROPERTY
    float boxSize = 100.0f;      // Box edge size, world units @SERIALIZABLE @EDITOR_PROPERTY
    float launchSpeed = 1500.0f; // Projectile launch speed, world units per second @SERIALIZABLE @EDITOR_PROPERTY

    SERIALIZABLE(PyramidSpawner);
    CLONEABLE_REF(PyramidSpawner);

private:
    Ref<RigidBody3D> mProjectile; // Reused projectile box

    // Builds the floor, the pyramid and the projectile
    void OnStart() override;

    // Polls the H key to launch the projectile
    void OnUpdate(float dt) override;

    // Releases the projectile reference on removal
    void OnRemoveFromScene() override;

    // Returns the launch point in front of the pyramid, in world space
    Vec3F MuzzlePosition() const;

    // Creates a box body with a mesh + collider at a world position
    Ref<RigidBody3D> SpawnBox(const String& name, RigidBody3D::Type type, const Vec3F& pos,
                              const Vec3F& size, const Color4& color);

    // Resets the projectile to the muzzle and fires it toward the pyramid
    void LaunchProjectile();

    REF_COUNTERABLE_IMPL(Component);
};
// --- META ---

CLASS_BASES_META(PyramidSpawner)
{
    BASE_CLASS(Component);
}
END_META;
CLASS_FIELDS_META(PyramidSpawner)
{
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(4).NAME(rows);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(100.0f).NAME(boxSize);
    FIELD().PUBLIC().EDITOR_PROPERTY_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(1500.0f).NAME(launchSpeed);
    FIELD().PRIVATE().NAME(mProjectile);
}
END_META;
CLASS_METHODS_META(PyramidSpawner)
{

    FUNCTION().PRIVATE().SIGNATURE(void, OnStart);
    FUNCTION().PRIVATE().SIGNATURE(void, OnUpdate, float);
    FUNCTION().PRIVATE().SIGNATURE(void, OnRemoveFromScene);
    FUNCTION().PRIVATE().SIGNATURE(Vec3F, MuzzlePosition);
    FUNCTION().PRIVATE().SIGNATURE(Ref<RigidBody3D>, SpawnBox, const String&, RigidBody3D::Type, const Vec3F&, const Vec3F&, const Color4&);
    FUNCTION().PRIVATE().SIGNATURE(void, LaunchProjectile);
}
END_META;
// --- END META ---
