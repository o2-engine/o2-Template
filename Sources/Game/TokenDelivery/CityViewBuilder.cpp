#include "o2/stdafx.h"
#include "TokenDelivery/CityViewBuilder.h"

#include "TokenDelivery/ArtSprites.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Scene/Components/ScriptableComponent.h"

namespace td
{
	// instantiates a generated prototype when it exists, otherwise builds the sprite by
	// code — the fallback path for a run before the assets generator has been executed
	static Ref<Actor> MakeProtoSprite(const String& protoPath, const char* spritePath,
									  const Vec2F& worldPos, float depth, const Ref<Actor>& parent)
	{
		auto proto = o2Assets.GetAssetRefByType<ActorAsset>(protoPath);
		if (!proto)
			return art::MakeSprite(spritePath, kWorldLayer, worldPos, depth, parent);

		auto actor = mmake<Actor>(proto, ActorCreateMode::InScene);
		if (parent)
			parent->AddChild(actor, false);
		actor->transform->SetWorldPosition(Vec3F(worldPos.x, worldPos.y, 0.0f));
		actor->SetDrawingDepth(depth);
		return actor;
	}

	static const char* RoadTilePath(int mask)
	{
		static const char* names[16] = {
			"Game/Tiles/road_O.png", "Game/Tiles/road_N.png", "Game/Tiles/road_E.png",
			"Game/Tiles/road_NE.png", "Game/Tiles/road_S.png", "Game/Tiles/road_NS.png",
			"Game/Tiles/road_ES.png", "Game/Tiles/road_NES.png", "Game/Tiles/road_W.png",
			"Game/Tiles/road_NW.png", "Game/Tiles/road_EW.png", "Game/Tiles/road_NEW.png",
			"Game/Tiles/road_SW.png", "Game/Tiles/road_NSW.png", "Game/Tiles/road_ESW.png",
			"Game/Tiles/road_NESW.png"
		};
		return names[mask & 15];
	}

	static String BuildingPath(const String& spriteId)
	{
		return String("Game/Buildings/") + spriteId + ".png";
	}

	static String PropPath(const String& spriteId)
	{
		return String("Game/Props/") + spriteId + ".png";
	}

	CityViewHandles BuildCityView(const CityModel& city)
	{
		CityViewHandles handles;
		handles.root = mmake<Actor>(ActorCreateMode::InScene);
		handles.root->SetName("city");
		handles.root->SetLayer(kWorldLayer);

		// backdrop under everything, centered on the city
		Vec2F center = CellToScreen(Vec2F(city.size*0.5f - 0.5f, city.size*0.5f - 0.5f));
		auto backdrop = art::MakeSprite("Game/backdrop.png", kWorldLayer, center, -10000.0f,
										handles.root);
		backdrop->transform->SetScale(Vec3F(2.2f, 2.2f, 1.0f));

		// ground: all cells covered, roads picked by connection mask
		for (int j = 0; j < city.size; j++)
		{
			for (int i = 0; i < city.size; i++)
			{
				Vec2I cell(i, j);
				Vec2F pos = CellToScreen(Vec2F((float)i, (float)j));
				float depth = -1000.0f + IsoDepth(Vec2F((float)i, (float)j));

				const char* path;
				if (city.IsRoad(cell))
					path = RoadTilePath(city.RoadMask(cell));
				else if (city.Ground(cell) == GroundKind::Grass)
				{
					// drawn above all neighbouring ground so the ragged grass edge
					// overlaps the surrounding slabs organically
					path = "Game/Tiles/grass.png";
					depth += 1.6f;
				}
				else
					path = "Game/Tiles/pavement.png";

				art::MakeSprite(path, kWorldLayer, pos, depth, handles.root);
			}
		}

		// street lamps: only on junction corners, at most one per junction
		Rng lampRng(city.size*7717u + 13u);
		for (int j = 0; j < city.size; j++)
		{
			for (int i = 0; i < city.size; i++)
			{
				Vec2I cell(i, j);
				if (!city.IsRoad(cell))
					continue;

				int mask = city.RoadMask(cell);
				int conns = 0;
				for (int d = 0; d < 4; d++)
					conns += (mask >> d) & 1;
				if (conns < 3 || lampRng.Frand() > 0.6f)
					continue;

				Vec2F diag(lampRng.Frand() < 0.5f ? -0.42f : 0.42f,
						   lampRng.Frand() < 0.5f ? -0.42f : 0.42f);
				Vec2F pos = Vec2F((float)i, (float)j) + diag;
				art::MakeSprite("Game/Props/lamp.png", kWorldLayer, CellToScreen(pos),
								IsoDepth(pos) + 0.05f, handles.root);
			}
		}

		// buildings; office anchors sit above the sprite top for tooltips
		handles.officeAnchors.resize(city.orders.Count());
		for (auto& building : city.buildings)
		{
			Vec2F fpCenter = building.FootprintCenter();
			Vec2F pos = CellToScreen(fpCenter);
			float depth = IsoDepth(fpCenter) + 0.25f;

			String path = BuildingPath(building.spriteId);
			auto actor = MakeProtoSprite(art::BuildingProtoPath(path.Data()), path.Data(),
										 pos, depth, handles.root);

			if (building.orderIndex >= 0)
			{
				auto meta = art::Find(path.Data());
				float topOffset = meta ? (float)meta->py + 50.0f : 250.0f;

				auto anchor = mmake<Actor>(ActorCreateMode::InScene);
				anchor->SetName("office anchor");
				anchor->SetLayer(kWorldLayer);
				handles.root->AddChild(anchor, false);
				anchor->transform->SetWorldPosition(Vec3F(pos.x, pos.y + topOffset, 0.0f));
				handles.officeAnchors[building.orderIndex] = anchor;
			}
		}

		// props
		for (auto& decor : city.decors)
		{
			Vec2F pos = CellToScreen(decor.cellPos);
			float depth = IsoDepth(decor.cellPos) + 0.1f;
			String path = PropPath(decor.spriteId);
			auto actor = art::MakeSprite(path.Data(), kWorldLayer, pos, depth, handles.root);

			if (decor.spriteId == "fountain")
			{
				// generated prototype carries the scale and the HologramPulse script
				auto hologram = MakeProtoSprite("Game/Protos/Hologram.proto",
												"Game/Props/chip.png",
												pos + Vec2F(0.0f, 140.0f), depth + 0.05f,
												handles.root);
				if (!hologram->GetComponent<ScriptableComponent>())
				{
					hologram->transform->SetScale(Vec3F(0.9f, 0.9f, 1.0f));
					auto script = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/HologramPulse.js"));
					if (script)
						hologram->AddComponent<ScriptableComponent>()->SetScript(script);
				}
				handles.hologram = hologram;
			}
		}

		return handles;
	}
}
