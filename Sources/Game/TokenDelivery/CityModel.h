#pragma once

#include "TokenDelivery/IsoMath.h"
#include "o2/Utils/Types/Containers/Vector.h"
#include "o2/Utils/Types/String.h"

using namespace o2;

namespace td
{
	enum class GroundKind { Grass = 0, Pavement = 1, Road = 2 };

	// ------------------------------------------
	// Placed building: sprite, footprint, order
	// ------------------------------------------
	struct BuildingInfo
	{
		String spriteId;        // Art manifest key without folder/extension, e.g. "house_brick_a"
		Vec2I  cell;            // Top-left footprint cell (min i, min j)
		Vec2I  footprint;       // Occupied cells
		int    orderIndex = -1; // Delivery order index, >= 0 for offices

		// Returns the footprint center in cell coordinates
		Vec2F FootprintCenter() const
		{
			return Vec2F(cell.x + footprint.x*0.5f - 0.5f, cell.y + footprint.y*0.5f - 0.5f);
		}
	};

	// -----------------------------
	// Placed decor prop of the city
	// -----------------------------
	struct DecorInfo
	{
		String spriteId; // "tree_big", "tree_small", "kiosk", "bench", "lamp", "fountain"
		Vec2F  cellPos;  // Position in cell coordinates
	};

	// ------------------------------------------
	// Delivery order bound to an office building
	// ------------------------------------------
	struct OrderInfo
	{
		String        name;          // Office city name shown in the HUD
		int           amount = 0;    // Tokens to pay for the delivery
		Vec2I         officeCell;    // Office footprint anchor cell
		Vector<Vec2I> deliveryCells; // Road cells adjacent to the office footprint
	};

	// ----------------------
	// City generation params
	// ----------------------
	struct CityGenParams
	{
		int size = 11;            // Grid side in cells
		int ordersCount = 3;      // Delivery orders (offices) to place
		int minOrderAmount = 35;  // Order price lower bound
		int maxOrderAmount = 150; // Order price upper bound
		int trafficCars = 4;      // Traffic cars to spawn
	};

	// ---------------------------------------------------------------------------------
	// Generated city: ground grid, buildings, decor, orders and the gameplay landmarks.
	// Plain data, owned by the game session; the view layer instantiates it.
	// ---------------------------------------------------------------------------------
	struct CityModel
	{
		int size = 0; // Grid side in cells

		Vector<GroundKind>   ground;    // Ground kinds, size*size, row-major [j*size + i]
		Vector<BuildingInfo> buildings; // Placed buildings
		Vector<DecorInfo>    decors;    // Placed decor props
		Vector<OrderInfo>    orders;    // Delivery orders

		Vec2I         fountainCell;  // Token source fountain cell
		Vector<Vec2I> sourceCells;   // Road cells where the car refills tokens
		Vec2I         playerStart;   // Player car start cell
		Dir           playerStartDir = Dir::E; // Player car start heading
		Vector<Vec2I> trafficStarts; // Traffic car start cells

		// Returns is the cell inside the grid
		bool InBounds(const Vec2I& c) const
		{
			return c.x >= 0 && c.y >= 0 && c.x < size && c.y < size;
		}

		// Returns the ground kind of the cell
		GroundKind Ground(const Vec2I& c) const { return ground[c.y*size + c.x]; }

		// Returns is the cell a road
		bool IsRoad(const Vec2I& c) const { return InBounds(c) && Ground(c) == GroundKind::Road; }

		// Returns the bitmask of road connections in NESW order (see DirBit)
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

		// Returns all road cells reachable from `from` by 4-neighbour steps
		Vector<Vec2I> ReachableRoadCells(const Vec2I& from) const;
	};

	// Generates the city: perpendicular road grid, blocks with sidewalks, a park block with
	// the token fountain, office buildings for orders, houses and decor. Deterministic by seed.
	CityModel GenerateCity(const CityGenParams& params, UInt32 seed);
}
// --- META ---

PRE_ENUM_META(td::GroundKind);
// --- END META ---
