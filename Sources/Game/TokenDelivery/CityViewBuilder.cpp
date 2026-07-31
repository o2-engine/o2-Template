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

	static String BlockPath(const String& spriteId)
	{
		return String("Game/Blocks/") + spriteId + ".png";
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

		// block art: houses, offices, composites and props. The sprite pivot is its footprint's
		// north corner, the depth comes from the footprint center so cars sort against it.
		handles.officeAnchors.resize(city.orders.Count());
		for (auto& object : city.objects)
		{
			Vec2F pos = CellToScreen(object.CornerCell());
			Vec2F center = object.CenterCell();
			float depth = IsoDepth(center) + (object.kind == ObjectKind::Composite ? 0.05f : 0.25f);

			String path = BlockPath(object.spriteId);
			art::MakeSprite(path.Data(), kWorldLayer, pos, depth, handles.root);

			if (object.orderIndex >= 0 && object.orderIndex < handles.officeAnchors.Count())
			{
				// over the middle of the building and above its roof: an office covers more
				// than one cell, and a tooltip hanging off its north corner points nowhere
				auto meta = art::Find(path.Data());
				float top = pos.y + (meta ? (float)meta->py + 50.0f : 250.0f);

				auto anchor = mmake<Actor>(ActorCreateMode::InScene);
				anchor->SetName("office anchor");
				anchor->SetLayer(kWorldLayer);
				handles.root->AddChild(anchor, false);
				anchor->transform->SetWorldPosition(Vec3F(CellToScreen(center).x, top, 0.0f));
				handles.officeAnchors[object.orderIndex] = anchor;
			}

		}

		// the token source hologram hangs over the plaza fountain; the same composite may well
		// be used as a courtyard piece elsewhere, so it is placed from the model, not per sprite
		{
			Vec2F over = CellToScreen(city.fountainCell);
			auto hologram = MakeProtoSprite("Game/Protos/Hologram.proto", "Game/Props/chip.png",
											over + Vec2F(0.0f, 150.0f),
											IsoDepth(city.fountainCell) + 1.0f, handles.root);
			if (!hologram->GetComponent<ScriptableComponent>())
			{
				hologram->transform->SetScale(Vec3F(0.9f, 0.9f, 1.0f));
				auto script = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/HologramPulse.js"));
				if (script)
					hologram->AddComponent<ScriptableComponent>()->SetScript(script);
			}
			handles.hologram = hologram;
		}

		return handles;
	}
}
