#pragma once

#include "TokenDelivery/IsoMath.h"
#include "o2/Utils/Types/Containers/Vector.h"
#include "o2/Utils/Types/String.h"

using namespace o2;

namespace td
{
	enum class GroundKind { Grass = 0, Pavement = 1, Road = 2 };

	struct BuildingInfo
	{
		String spriteId;          // art_manifest key without folder/extension, e.g. "house_brick_a"
		Vec2I  cell;              // top-left footprint cell (min i, min j)
		Vec2I  footprint;         // in cells
		int    orderIndex = -1;   // >= 0 for offices

		Vec2F FootprintCenter() const
		{
			return Vec2F(cell.x + footprint.x*0.5f - 0.5f, cell.y + footprint.y*0.5f - 0.5f);
		}
	};

	struct DecorInfo
	{
		String spriteId; // "tree_big", "tree_small", "kiosk", "bench", "lamp", "fountain"
		Vec2F  cellPos;
	};

	struct OrderInfo
	{
		String        name;
		int           amount = 0;
		Vec2I         officeCell;
		Vector<Vec2I> deliveryCells; // road cells adjacent to the office footprint
	};

	struct CityGenParams
	{
		int size = 11;       // grid side in cells
		int ordersCount = 3;
		int minOrderAmount = 35;
		int maxOrderAmount = 150;
		int trafficCars = 4;
	};

	struct CityModel
	{
		int size = 0;
		Vector<GroundKind>    ground;    // size*size, row-major [j*size + i]
		Vector<BuildingInfo>  buildings;
		Vector<DecorInfo>     decors;
		Vector<OrderInfo>     orders;
		Vec2I                 fountainCell;
		Vector<Vec2I>         sourceCells; // road cells where the car refills tokens
		Vec2I                 playerStart;
		Dir                   playerStartDir = Dir::E;
		Vector<Vec2I>         trafficStarts;

		bool InBounds(const Vec2I& c) const
		{
			return c.x >= 0 && c.y >= 0 && c.x < size && c.y < size;
		}

		GroundKind Ground(const Vec2I& c) const { return ground[c.y*size + c.x]; }
		bool IsRoad(const Vec2I& c) const { return InBounds(c) && Ground(c) == GroundKind::Road; }

		// bitmask of road connections in NESW order (see DirBit)
		int RoadMask(const Vec2I& c) const
		{
			int mask = 0;
			for (int d = 0; d < 4; d++)
			{
				if (IsRoad(c + DirVec((Dir)d)))
					mask |= 1 << d;
			}
			return mask;
		}

		// all road cells reachable from `from` by 4-neighbour steps
		Vector<Vec2I> ReachableRoadCells(const Vec2I& from) const;
	};

	// Generates the city: perpendicular road grid, blocks with sidewalks, a park block with
	// the token fountain, office buildings for orders, houses and decor. Deterministic by seed.
	CityModel GenerateCity(const CityGenParams& params, UInt32 seed);
}
// --- META ---

PRE_ENUM_META(td::GroundKind);
// --- END META ---
