#pragma once

#include "o2/Utils/Math/Vector2.h"

using namespace o2;

// Isometric 2:1 projection contract, shared with the art pipeline (Tools/ArtPipeline)
// and art_manifest.json: cell (i, j) -> screen x = (i - j)*128, screen y (up) = -(i + j)*64.
// E = +i, W = -i, S = +j, N = -j.
namespace td
{
	constexpr float kTileHalfW = 128.0f;
	constexpr float kTileHalfH = 64.0f;
	constexpr float kZScale = 64.0f; // screen pixels per one cell of height

	// Block art is laid out on a grid finer than the road cell: a cell holds kUnitsPerCell
	// squared units, so a house is sized in units and several of them share one cell.
	constexpr int kUnitsPerCell = 4;

	enum class Dir { N = 0, E = 1, S = 2, W = 3 };

	// Returns the cell step of the direction
	inline Vec2I DirVec(Dir d)
	{
		switch (d)
		{
			case Dir::N: return Vec2I(0, -1);
			case Dir::E: return Vec2I(1, 0);
			case Dir::S: return Vec2I(0, 1);
			default:     return Vec2I(-1, 0);
		}
	}

	// Returns the reverse direction
	inline Dir Opposite(Dir d) { return (Dir)(((int)d + 2)%4); }

	// Returns the direction right of the given one
	inline Dir RightOf(Dir d) { return (Dir)(((int)d + 1)%4); }

	// Returns the direction left of the given one
	inline Dir LeftOf(Dir d) { return (Dir)(((int)d + 3)%4); }

	// Returns the direction bit; matches road tile naming order NESW
	inline int DirBit(Dir d) { return 1 << (int)d; }

	// Cell position of a unit grid node. Unit (0, 0) is the north corner of cell (0, 0), which
	// in cell coordinates — those name cell centers — sits at (-0.5, -0.5)
	inline Vec2F UnitToCell(const Vec2F& unit)
	{
		return unit/(float)kUnitsPerCell - Vec2F(0.5f, 0.5f);
	}

	// Projects a cell position to screen space
	inline Vec2F CellToScreen(const Vec2F& cell)
	{
		return Vec2F((cell.x - cell.y)*kTileHalfW, -(cell.x + cell.y)*kTileHalfH);
	}

	// Unprojects a screen point back to cell space
	inline Vec2F ScreenToCell(const Vec2F& screen)
	{
		float a = screen.x/kTileHalfW, b = -screen.y/kTileHalfH;
		return Vec2F((a + b)*0.5f, (b - a)*0.5f);
	}

	// Returns the draw depth of the cell; grows toward the camera. Static objects set it
	// once, movers every frame
	inline float IsoDepth(const Vec2F& cell) { return cell.x + cell.y; }

	// ----------------------------------------------
	// Deterministic xorshift rng for world generation
	// ----------------------------------------------
	struct Rng
	{
		UInt32 state; // Generator state, never zero

		explicit Rng(UInt32 seed): state(seed ? seed : 0x9e3779b9u) {}

		// Returns the next raw 32-bit value
		UInt32 Next()
		{
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return state;
		}

		// Returns a value in the inclusive range
		int Range(int minInclusive, int maxInclusive)
		{
			return minInclusive + (int)(Next()%(UInt32)(maxInclusive - minInclusive + 1));
		}

		// Returns a value in [0, 1)
		float Frand() { return (Next() >> 8)*(1.0f/16777216.0f); }
	};
}
// --- META ---

PRE_ENUM_META(td::Dir);
// --- END META ---
