#pragma once

// -------------------------------------------------------------------------------------------------
// A small, self-contained showcase of the o2 3D physics API (Box3D): o2::RigidBody3D bodies with
// o2::BoxCollider3D / o2::SphereCollider3D colliders, synced to the scene's 3D transforms.
//
// SpawnPhysics3DDemo() configures gravity/scale for the Main scene (Z-up, ~100 units) and drops a
// few colored boxes and spheres onto a static floor aligned with the visual ground plane. Called
// once from GameApplication after the scene is loaded. Safe to remove — it only adds its own actors.
// -------------------------------------------------------------------------------------------------
namespace demo
{
    // Configures 3D gravity/scale and spawns the falling physics bodies into the current scene
    void SpawnPhysics3DDemo();
}
