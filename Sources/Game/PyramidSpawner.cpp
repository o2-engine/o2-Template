#include "o2/stdafx.h"
#include "PyramidSpawner.h"

#include "o2/Application/Input.h"
#include "o2/Application/VKCodes.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Components/MeshPrimitiveComponent.h"
#include "o2/Scene/Physics3D/BoxCollider3D.h"

Vec3F PyramidSpawner::MuzzlePosition() const
{
    // In front of the pyramid on the camera side (-Y), raised to the pyramid's mid-height.
    Vec3F base = GetActor()->transform->GetWorldPosition();
    return base + Vec3F(0.0f, -350.0f, rows*boxSize*0.5f);
}

Ref<RigidBody3D> PyramidSpawner::SpawnBox(const String& name, RigidBody3D::Type type, const Vec3F& pos,
                                         const Vec3F& size, const Color4& color)
{
    auto body = mmake<RigidBody3D>();
    body->SetName(name);
    body->SetLayer("3D");
    body->SetBodyType(type);
    body->transform->SetPosition(pos);

    auto mesh = body->AddComponent<MeshPrimitiveComponent>();
    mesh->SetPrimitiveType(PrimitiveType3D::Box);
    mesh->SetSize(size);
    mesh->SetColor(color);

    body->AddComponent<BoxCollider3D>()->SetSize(size);

    return body;
}

void PyramidSpawner::OnStart()
{
    auto owner = GetActor();
    if (!owner)
        return;

    Vec3F base = owner->transform->GetWorldPosition();

    // Spawned bodies must be in the scene so RigidBody3D creates its Box3D body.
    auto prevMode = Actor::GetDefaultCreationMode();
    Actor::SetDefaultCreationMode(ActorCreateMode::InScene);

    // Static floor: a thin slab whose top sits at the pyramid base height (no mesh, the visual ground exists).
    auto floor = mmake<RigidBody3D>();
    floor->SetName("pyramid floor");
    floor->SetLayer("3D");
    floor->SetBodyType(RigidBody3D::Type::Static);
    floor->transform->SetPosition(base + Vec3F(0.0f, 0.0f, -50.0f));
    floor->AddComponent<BoxCollider3D>()->SetSize(Vec3F(4000.0f, 4000.0f, 100.0f));

    // Pyramid: the bottom row has `rows` boxes, each row above has one fewer, centered over the row below.
    const Color4 stoneA(150, 130, 110, 255);
    const Color4 stoneB(120, 110, 100, 255);
    for (int r = 0; r < rows; r++)
    {
        int n = rows - r;
        float z = base.z + boxSize*0.5f + r*boxSize;
        float x0 = base.x - (n - 1)*0.5f*boxSize;
        for (int i = 0; i < n; i++)
        {
            Vec3F pos(x0 + i*boxSize, base.y, z);
            const Color4& color = ((r + i)%2 == 0) ? stoneA : stoneB;
            SpawnBox("pyramid box", RigidBody3D::Type::Dynamic, pos, Vec3F(boxSize, boxSize, boxSize), color);
        }
    }

    // Reusable projectile, twice the pyramid box size, parked (kinematic, so it doesn't fall) at the
    // muzzle until the first shot.
    float projSize = boxSize*2.0f;
    mProjectile = SpawnBox("projectile", RigidBody3D::Type::Kinematic, MuzzlePosition(),
                           Vec3F(projSize, projSize, projSize), Color4(230, 90, 40, 255));

    Actor::SetDefaultCreationMode(prevMode);
}

void PyramidSpawner::OnUpdate(float /*dt*/)
{
    if (o2Input.IsKeyPressed(VK_H))
        LaunchProjectile();
}

void PyramidSpawner::LaunchProjectile()
{
    if (!mProjectile)
        return;

    // Reset the same box to the muzzle and fire it toward the pyramid (+Y). World gravity is strong at
    // this scale (~981 u/s^2), so a slow shot would just drop; a low gravity scale lets it fly into the
    // middle roughly on the level instead.
    mProjectile->SetBodyType(RigidBody3D::Type::Dynamic);
    mProjectile->SetGravityScale(0.05f);
    mProjectile->transform->SetPosition(MuzzlePosition());
    mProjectile->SetAngularVelocity(Vec3F());
    mProjectile->SetLinearVelocity(Vec3F(0.0f, launchSpeed, 0.0f));
    mProjectile->SetIsSleeping(false);
}

void PyramidSpawner::OnRemoveFromScene()
{
    mProjectile = nullptr;
    Component::OnRemoveFromScene();
}
// --- META ---

DECLARE_CLASS(PyramidSpawner, PyramidSpawner);
// --- END META ---
