#include "o2/stdafx.h"
#include "TokenDelivery/GameAssetsGen.h"

#include "TokenDelivery/ArtSprites.h"
#include "TokenDelivery/CarDrawableComponent.h"
#include "TokenDelivery/CityViewBuilder.h"
#include "TokenDelivery/GameControllerComponent.h"
#include "TokenDelivery/GameHUDComponent.h"
#include "TokenDelivery/GameTutorialComponent.h"
#include "TokenDelivery/TiltShiftPass.h"
#include "TokenDelivery/TrafficCarComponent.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Assets/Types/AtlasAsset.h"
#include "o2/Assets/Types/ImageAsset.h"
#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Assets/Types/SceneAsset.h"
#include "o2/Render/Particles/ParticlesEffects.h"
#include "o2/Render/Particles/ParticlesEmitterShapes.h"
#include "o2/Render/Pipeline/RenderPipeline.h"
#include "o2/Render/Render.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/ParticlesEmitterComponent.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Debug/Debug.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Math/ColorGradient.h"

namespace td
{
	static const char* kProtosDir = "Game/Protos/";

	// particle emitter setters reseed the global rand with a fixed value (editor bake
	// machinery), which would mint identical UIDs for every asset saved after touching
	// an emitter — reseed with advancing entropy right before minting
	static void ReseedUidRandom()
	{
		static unsigned counter = 0;
		srand((unsigned)::time(nullptr) ^ (unsigned)::clock() ^ (++counter*2654435761u));
	}

	// saves the actor as a .proto under Assets/; an already known asset is reused so its
	// UID survives regeneration — references in other generated assets hold it
	static AssetRef<ActorAsset> SaveProto(const Ref<Actor>& actor, const String& path)
	{
		if (auto existing = o2Assets.GetAssetRefByType<ActorAsset>(path))
		{
			existing->SetActor(actor);
			existing->Save();
			return existing;
		}

		ReseedUidRandom();
		auto asset = AssetRef<ActorAsset>::CreateAsset(actor);
		asset->Save(path);
		return asset;
	}

	// detached sprite actor with size and pivot from the art manifest
	static Ref<Actor> MakeSpriteProtoActor(const char* spritePath)
	{
		auto actor = mmake<Actor>(ActorCreateMode::NotInScene);
		actor->SetName(spritePath);
		actor->SetLayer(kWorldLayer);

		auto image = actor->AddComponent<ImageComponent>();
		image->LoadFromImage(String(spritePath));

		if (auto meta = art::Find(spritePath))
		{
			actor->transform->SetSize2D(Vec2F((float)meta->w, (float)meta->h));
			actor->transform->SetPivot2D(art::NormalizedPivot(*meta));
		}
		else
			image->FitActorByImage();

		return actor;
	}

	static AssetRef<ActorAsset> MakeCarProto(const String& path, const String& name,
											 CarDrawableComponent::CarKind kind, bool traffic)
	{
		auto actor = mmake<Actor>(ActorCreateMode::NotInScene);
		actor->SetName(name);
		actor->SetLayer(kWorldLayer);
		actor->AddComponent<CarDrawableComponent>()->SetupCar(kind);

		if (traffic)
		{
			auto script = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/TrafficAI.js"));
			if (script)
				actor->AddComponent<ScriptableComponent>()->SetScript(script);
			actor->AddComponent<TrafficCarComponent>();
		}

		return SaveProto(actor, path);
	}

	static void MakeBuildingProtos()
	{
		for (auto& meta : art::kSprites)
		{
			String path(meta.path);
			if (!path.StartsWith("Game/Buildings/"))
				continue;

			SaveProto(MakeSpriteProtoActor(meta.path), art::BuildingProtoPath(meta.path));
		}
	}

	static AssetRef<ActorAsset> MakeHologramProto()
	{
		auto actor = MakeSpriteProtoActor("Game/Props/chip.png");
		actor->transform->SetScale(Vec3F(0.9f, 0.9f, 1.0f));

		auto script = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/HologramPulse.js"));
		if (script)
			actor->AddComponent<ScriptableComponent>()->SetScript(script);

		return SaveProto(actor, String(kProtosDir) + "Hologram.proto");
	}

	static AssetRef<ActorAsset> MakeSparkBurstProto()
	{
		auto actor = mmake<Actor>(ActorCreateMode::NotInScene);
		actor->SetName("spark emitter");
		actor->SetLayer(kWorldLayer);

		auto emitter = actor->AddComponent<ParticlesEmitterComponent>();
		auto source = mmake<SingleSpriteParticleSource>();
		source->image = o2Assets.GetAssetRefByType<ImageAsset>(String("Game/Props/spark.png"));
		emitter->SetParticlesSource(source);

		auto material = mmake<Material>(*o2Render.GetDefaultMaterial());
		material->SetBlendMode(BlendMode::Add);
		emitter->SetMaterial(material);

		emitter->SetShape(mmake<CircleParticlesEmitterShape>());
		emitter->SetParticlesRelativity(false);
		emitter->SetMaxParticles(48);
		emitter->SetParticlesPerSecond(130.0f);
		emitter->SetEmissionDuration(0.06f);
		emitter->SetParticlesLifetime(0.45f);
		emitter->SetInitialSize(0.62f);
		emitter->SetInitialSizeRange(0.62f*0.3f);
		emitter->SetInitialSpeed(190.0f);
		emitter->SetInitialSpeedRange(90.0f);
		emitter->SetEmitParticlesMoveDirectionRange(360.0f);
		emitter->Stop();

		auto fade = mmake<ParticlesColorEffect>();
		fade->colorGradient = mmake<ColorGradient>(Color4(255, 255, 255, 255),
												   Color4(255, 255, 255, 0));
		emitter->AddEffect(fade);

		return SaveProto(actor, String(kProtosDir) + "SparkBurst.proto");
	}

	static AssetRef<AtlasAsset> EnsureAtlas(const String& path)
	{
		if (auto existing = o2Assets.GetAssetRefByType<AtlasAsset>(path))
			return existing;

		ReseedUidRandom();
		auto atlas = AssetRef<AtlasAsset>::CreateAsset();
		atlas->Save(path);
		return atlas;
	}

	static void AssignToAtlas(const String& imagePath, const AssetRef<AtlasAsset>& atlas)
	{
		auto image = o2Assets.GetAssetRefByType<ImageAsset>(imagePath);
		if (!image)
			return;

		// Save without a loaded bitmap rewrites only the .meta, the png stays untouched
		if (image->GetAtlasUID() != atlas->GetUID())
		{
			image->SetAtlas(atlas->GetUID());
			image->Save();
		}
	}

	// world sprites and UI pack into two atlases; ground tiles stay standalone — they abut
	// on screen and the atlas padding is transparent, so sampling past a packed tile edge
	// would draw seams between tiles. The backdrop is a single huge image, an atlas page
	// would mostly carry it alone
	static void ConfigureAtlases()
	{
		auto setupMeta = [](const AssetRef<AtlasAsset>& atlas)
		{
			auto meta = atlas->GetMeta();
			meta->common.maxSize = Vec2I(2048, 2048);
			meta->common.border = 2;
			// None on every platform including WebAssembly: WebGL2 guarantees no compressed
			// texture format without extensions (S3TC is desktop-only, ASTC mobile-only, and
			// the engine has no ETC2), and the flat-color art downloads far smaller as png
			// atlas pages than as DXT/ASTC blocks
			meta->common.compression = TextureCompression::None;
			atlas->Save();
		};

		auto worldAtlas = EnsureAtlas("Game/WorldAtlas.atlas");
		auto uiAtlas = EnsureAtlas("Game/UIAtlas.atlas");
		setupMeta(worldAtlas);
		setupMeta(uiAtlas);

		for (auto& sprite : art::kSprites)
		{
			String path(sprite.path);
			if (path.StartsWith("Game/Buildings/") || path.StartsWith("Game/Cars/") ||
				path.StartsWith("Game/Props/"))
			{
				AssignToAtlas(path, worldAtlas);
			}
		}

		auto uiFolder = o2FileSystem.GetFolderInfo(o2Assets.GetAssetsPath() + "Game/UI");
		for (auto& file : uiFolder.files)
		{
			String fileName = o2FileSystem.GetPathWithoutDirectories(file.path);
			if (fileName.EndsWith(".png"))
				AssignToAtlas(String("Game/UI/") + fileName, uiAtlas);
		}
	}

	// the bootstrap scene: layers, both cameras and the game actor with the controller,
	// HUD and tutorial components linked together
	static void MakeBootstrapScene(const AssetRef<ActorAsset>& playerProto,
								   const AssetRef<ActorAsset>& vanProto,
								   const AssetRef<ActorAsset>& hatchbackProto)
	{
		o2Scene.Clear(true);
		o2Scene.AddLayer(kWorldLayer);
		o2Scene.AddLayer(kUILayer);

		auto worldCamera = mmake<CameraActor>();
		worldCamera->SetName("world camera");
		// fitted, not fixed: a fixed size stretches its rect onto any window shape and
		// squashes the isometry. Fitting keeps the tile scale and shows more city on a
		// wider window
		worldCamera->SetFittedSize(Vec2F(1760.0f, 1100.0f));
		worldCamera->drawLayers.SetLayers(Vector<String>{ kWorldLayer });
		worldCamera->fillBackground = true;
		worldCamera->fillColor = Color4(166, 190, 205, 255);

		// tilt-shift: the world renders through an offscreen pass with edge blur; the UI
		// camera draws afterwards and stays sharp
		auto pipeline = mmake<RenderPipeline>();
		pipeline->AddPass(mmake<TiltShiftPass>());
		worldCamera->SetRenderPipeline(pipeline);

		// camera follow is scripted in JS; the controller feeds the target every frame
		auto followScript = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/CameraFollow.js"));
		if (followScript)
			worldCamera->AddComponent<ScriptableComponent>()->SetScript(followScript);

		auto uiCamera = mmake<CameraActor>();
		uiCamera->SetName("ui camera");
		uiCamera->SetFittedSize(Vec2F(1280.0f, 800.0f));
		uiCamera->drawLayers.SetLayers(Vector<String>{ kUILayer });
		uiCamera->fillBackground = false;

		auto game = mmake<Actor>(ActorCreateMode::InScene);
		game->SetName("token delivery");
		auto hud = game->AddComponent<GameHUDComponent>();
		auto tutorial = game->AddComponent<GameTutorialComponent>();
		auto controller = game->AddComponent<GameControllerComponent>();
		controller->SetSceneLinks(worldCamera, uiCamera, hud, tutorial,
								  playerProto, vanProto, hatchbackProto);

		// pull the freshly created actors out of the added queue into the scene roots;
		// OnStart is not fired — the controller must not build the game into this scene
		o2Scene.UpdateAddedEntities();

		String scenePath("Bootstrap.scn");
		o2Scene.Save(o2Assets.GetAssetsPath() + scenePath);
		if (!o2Assets.IsAssetExist(scenePath))
		{
			ReseedUidRandom();
			auto sceneAsset = AssetRef<SceneAsset>::CreateAsset();
			sceneAsset->SetPath(scenePath);
			sceneAsset->Save();
		}
	}

	void GenerateGameAssets()
	{
		o2Scene.AddLayer(kWorldLayer);
		o2Scene.AddLayer(kUILayer);

		ConfigureAtlases();

		auto playerProto = MakeCarProto(String(kProtosDir) + "PlayerCar.proto", "player car",
										CarDrawableComponent::CarKind::PlayerPickup, false);
		auto vanProto = MakeCarProto(String(kProtosDir) + "TrafficVan.proto", "traffic car",
									 CarDrawableComponent::CarKind::Van, true);
		auto hatchbackProto = MakeCarProto(String(kProtosDir) + "TrafficHatchback.proto", "traffic car",
										   CarDrawableComponent::CarKind::Hatchback, true);

		MakeBuildingProtos();
		MakeHologramProto();
		MakeSparkBurstProto();
		MakeBootstrapScene(playerProto, vanProto, hatchbackProto);

		o2Debug.Log("Game assets generated into " + o2Assets.GetAssetsPath());
	}
}
