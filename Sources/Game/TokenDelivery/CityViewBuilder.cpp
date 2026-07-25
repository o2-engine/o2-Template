#include "o2/stdafx.h"
#include "TokenDelivery/CityViewBuilder.h"

#include "TokenDelivery/ArtSprites.h"

namespace td
{
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

		// ground: all cells covered, roads picked by connection mask; street lamps land on
		// the road sidewalk strips like in the reference
		Rng lampRng(city.size*7717u + 13u);
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
					path = "Game/Tiles/grass.png";
				else
					path = "Game/Tiles/pavement.png";

				art::MakeSprite(path, kWorldLayer, pos, depth, handles.root);

				if (city.IsRoad(cell))
				{
					int mask = city.RoadMask(cell);
					for (int d = 0; d < 4; d++)
					{
						if ((mask & (1 << d)) != 0 || lampRng.Frand() > 0.15f)
							continue;
						// closed edge: place a lamp on its sidewalk strip
						Vec2F edge = Vec2F((float)i, (float)j) +
									 Vec2F((float)DirVec((Dir)d).x, (float)DirVec((Dir)d).y)*0.38f;
						art::MakeSprite("Game/Props/lamp.png", kWorldLayer, CellToScreen(edge),
										IsoDepth(edge) + 0.05f, handles.root);
					}
				}
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
			auto actor = art::MakeSprite(path.Data(), kWorldLayer, pos, depth, handles.root);

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
				auto hologram = art::MakeSprite("Game/Props/chip.png", kWorldLayer,
												pos + Vec2F(0.0f, 140.0f), depth + 0.05f,
												handles.root);
				hologram->transform->SetScale(Vec3F(0.9f, 0.9f, 1.0f));
				handles.hologram = hologram;
			}
		}

		return handles;
	}
}
