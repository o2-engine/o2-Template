#include "o2/stdafx.h"
#include "Physics3DDemo.h"

#include "o2/Scene/Actor.h"
#include "o2/Scene/Components/MeshPrimitiveComponent.h"
#include "o2/Scene/Physics3D/BoxCollider3D.h"
#include "o2/Scene/Physics3D/RigidBody3D.h"
#include "o2/Scene/Physics3D/SphereCollider3D.h"

using namespace o2;

namespace demo
{
    namespace
    {
        // 3D physics is configured data-driven in ProjectSettings.json (physics3D): the Main scene is
        // Z-up in ~100-units-per-object, so gravity is -Z and the world->physics scale maps ~100 world
        // units to 1 physics meter, keeping Box3D bodies near the human scale its solver likes.

        Ref<RigidBody3D> MakeBody(const String& name, RigidBody3D::Type type, const Vec3F& pos)
        {
            auto body = mmake<RigidBody3D>();
            body->SetName(name);
            body->SetLayer("3D");
            body->SetBodyType(type);
            body->transform->SetPosition(pos);
            return body;
        }

        void AddBox(const Ref<RigidBody3D>& body, const Vec3F& size, const Color4& color)
        {
            auto mesh = body->AddComponent<MeshPrimitiveComponent>();
            mesh->SetPrimitiveType(PrimitiveType3D::Box);
            mesh->SetSize(size);
            mesh->SetColor(color);

            auto collider = body->AddComponent<BoxCollider3D>();
            collider->SetSize(size);
        }

        void AddSphere(const Ref<RigidBody3D>& body, float diameter, const Color4& color)
        {
            auto mesh = body->AddComponent<MeshPrimitiveComponent>();
            mesh->SetPrimitiveType(PrimitiveType3D::Sphere);
            mesh->SetSize(Vec3F(diameter, diameter, diameter));
            mesh->SetColor(color);

            auto collider = body->AddComponent<SphereCollider3D>();
            collider->SetRadius(diameter*0.5f);
        }
    }

    void SpawnPhysics3DDemo()
    {
        // Force spawned actors into the running scene so RigidBody3D creates its Box3D body.
        auto prevMode = Actor::GetDefaultCreationMode();
        Actor::SetDefaultCreationMode(ActorCreateMode::InScene);

        // Static floor: a thin slab whose top sits at z=0, matching the visual ground plane (no mesh).
        auto floor = MakeBody("physics floor", RigidBody3D::Type::Static, Vec3F(0.0f, 0.0f, -50.0f));
        floor->AddComponent<BoxCollider3D>()->SetSize(Vec3F(2000.0f, 2000.0f, 100.0f));

        // Falling bodies, dropped from different heights and spots so they land in sequence.
        AddBox(MakeBody("physics box 1", RigidBody3D::Type::Dynamic, Vec3F(-110.0f, -80.0f, 400.0f)),
               Vec3F(120.0f, 120.0f, 120.0f), Color4(210, 80, 70, 255));

        AddBox(MakeBody("physics box 2", RigidBody3D::Type::Dynamic, Vec3F(130.0f, 110.0f, 720.0f)),
               Vec3F(120.0f, 120.0f, 120.0f), Color4(90, 160, 210, 255));

        AddSphere(MakeBody("physics ball 1", RigidBody3D::Type::Dynamic, Vec3F(-260.0f, 120.0f, 560.0f)),
                  130.0f, Color4(230, 200, 80, 255));

        AddSphere(MakeBody("physics ball 2", RigidBody3D::Type::Dynamic, Vec3F(250.0f, -150.0f, 900.0f)),
                  100.0f, Color4(150, 220, 130, 255));

        Actor::SetDefaultCreationMode(prevMode);
    }
}
