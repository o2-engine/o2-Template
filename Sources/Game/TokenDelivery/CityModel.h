#pragma once

#include "TokenDelivery/IsoMath.h"
#include "o2/Utils/Types/Containers/Vector.h"
#include "o2/Utils/Types/String.h"

using namespace o2;

namespace td
{
	enum class GroundKind { Grass = 0, Pavement = 1, Road = 2 };

	enum class ObjectKind { House = 0, Office = 1, Prop = 2, Composite = 3 };

	// ---------------------------------------------------------------------------------
	// One placed piece of block art: a house, an office, a single prop or a composite —
	// a whole ready-made courtyard piece such as a market or a playground.
	// Positioned on the unit grid (kUnitsPerCell units per road cell).
	// ---------------------------------------------------------------------------------
	struct BlockObject
	{
		String     spriteId;                    // Art manifest key in Game/Blocks, no extension
		Vec2I      unit;                        // North corner in grid units
		Vec2I      unitSize;                    // Footprint in grid units
		ObjectKind kind = ObjectKind::Prop;
		int        orderIndex = -1;             // Delivery order index, >= 0 for offices

		// Returns the north corner in cell coordinates — where the sprite pivot goes
		Vec2F CornerCell() const { return UnitToCell(Vec2F((float)unit.x, (float)unit.y)); }

		// Returns the footprint center in cell coordinates
		Vec2F CenterCell() const
		{
			return UnitToCell(Vec2F(unit.x + unitSize.x*0.5f, unit.y + unitSize.y*0.5f));
		}

		// Returns the cell the footprint starts in
		Vec2I Cell() const { return Vec2I(unit.x/kUnitsPerCell, unit.y/kUnitsPerCell); }
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
		int size = 15;            // Grid side in cells
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

		Vector<GroundKind>  ground;  // Ground kinds, size*size, row-major [j*size + i]
		Vector<BlockObject> objects; // Everything standing on the blocks, in draw order
		Vector<OrderInfo>   orders;  // Delivery orders

		Vec2F         fountainCell;  // Token source: center of the plaza block's fountain
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

	// Generates the city: perpendicular road grid, then a block layout for every plot between
	// the streets — buildings around the back edges, one composite courtyard piece inside, a
	// few props and deliberate open ground. Deterministic by seed.
	CityModel GenerateCity(const CityGenParams& params, UInt32 seed);

	// -------------------------------
	// What one plot is asked to hold
	// -------------------------------
	struct BlockLayoutOptions
	{
		bool        wantOffice = false;  // Place an office and make it a delivery target
		const char* centrepiece = nullptr; // Composite placed before the buildings, so it is
										   // guaranteed the middle of the plot
		const char* preferred = nullptr;   // Nudge for the courtyard piece choice, may not fit
		float       fillScale = 1.0f;      // Scales how much of each edge gets built up; the
										   // one-cell strips along the city rim use less, a
										   // building there covers the ring road behind it
	};

	// Lays out one rectangular plot, in units local to its north corner. Exposed for tests.
	Vector<BlockObject> LayoutBlock(const Vec2I& plotUnits, Rng& rng,
									const BlockLayoutOptions& options = BlockLayoutOptions());
}
// --- META ---

PRE_ENUM_META(td::GroundKind);

PRE_ENUM_META(td::ObjectKind);
// --- END META ---
