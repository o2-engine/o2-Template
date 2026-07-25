#include "o2/stdafx.h"
#include "TokenDelivery/CityModel.h"

namespace td
{
	Vector<Vec2I> CityModel::ReachableRoadCells(const Vec2I& from) const
	{
		Vector<Vec2I> result;
		if (!IsRoad(from))
			return result;

		Vector<bool> visited;
		visited.resize(size*size, false);
		Vector<Vec2I> queue { from };
		visited[from.y*size + from.x] = true;

		while (!queue.IsEmpty())
		{
			Vec2I c = queue.PopBack();
			result.Add(c);
			for (int d = 0; d < 4; d++)
			{
				Vec2I n = c + DirVec((Dir)d);
				if (IsRoad(n) && !visited[n.y*size + n.x])
				{
					visited[n.y*size + n.x] = true;
					queue.Add(n);
				}
			}
		}
		return result;
	}

	namespace
	{
		struct HouseType { const char* sprite; int fw; int fh; };
		const HouseType kHouses[] = {
			{ "house_brick_a", 1, 1 }, { "house_brick_b", 1, 1 }, { "house_cream", 1, 1 },
			{ "house_terracotta", 1, 1 }, { "house_shop_awning", 1, 1 }, { "house_cafe", 1, 1 },
			{ "house_tall", 1, 1 }, { "house_double", 2, 1 }, { "house_long", 1, 2 },
			{ "house_corner", 2, 2 }
		};
		const HouseType kOffices[] = {
			{ "office_glass", 2, 2 }, { "office_loft", 2, 1 }, { "office_classic", 1, 1 }
		};
		const char* kOrderNames[] = {
			"London", "Paris", "Berlin", "Madrid", "Rome", "Vienna", "Prague", "Amsterdam",
			"Lisbon", "Dublin"
		};

		struct Block
		{
			Vector<Vec2I> cells;
			bool border = false;
			Vec2F Centroid() const
			{
				Vec2F sum;
				for (auto& c : cells)
					sum += Vec2F((float)c.x, (float)c.y);
				return sum/(float)cells.Count();
			}
		};

		// road line coordinates from 1 to size-2 with block widths of 2..4 cells
		Vector<int> MakeRoadLines(int size, Rng& rng)
		{
			Vector<int> lines { 1 };
			int last = size - 2;
			int cur = 1;
			while (true)
			{
				int gap = rng.Range(2, 3);
				int next = cur + gap + 1;
				if (next + 3 > last)
					break;
				lines.Add(next);
				cur = next;
			}
			if (last - cur - 1 < 2 && lines.Count() > 1)
				lines.PopBack();
			lines.Add(last);
			return lines;
		}

		Vector<Block> FindBlocks(const CityModel& m)
		{
			Vector<Block> blocks;
			Vector<bool> visited;
			visited.resize(m.size*m.size, false);
			for (int j = 0; j < m.size; j++)
			{
				for (int i = 0; i < m.size; i++)
				{
					Vec2I start(i, j);
					if (m.IsRoad(start) || visited[j*m.size + i])
						continue;

					Block block;
					Vector<Vec2I> queue { start };
					visited[j*m.size + i] = true;
					while (!queue.IsEmpty())
					{
						Vec2I c = queue.PopBack();
						block.cells.Add(c);
						if (c.x == 0 || c.y == 0 || c.x == m.size - 1 || c.y == m.size - 1)
							block.border = true;
						for (int d = 0; d < 4; d++)
						{
							Vec2I n = c + DirVec((Dir)d);
							if (m.InBounds(n) && !m.IsRoad(n) && !visited[n.y*m.size + n.x])
							{
								visited[n.y*m.size + n.x] = true;
								queue.Add(n);
							}
						}
					}
					blocks.Add(block);
				}
			}
			return blocks;
		}

		struct Occupancy
		{
			Vector<bool> taken;
			int size;

			Occupancy(int sz): size(sz) { taken.resize(sz*sz, false); }

			bool IsFree(const Vec2I& c) const { return !taken[c.y*size + c.x]; }
			void Take(const Vec2I& c, const Vec2I& fp)
			{
				for (int j = 0; j < fp.y; j++)
				{
					for (int i = 0; i < fp.x; i++)
						taken[(c.y + j)*size + c.x + i] = true;
				}
			}
		};

		bool FootprintFits(const CityModel& m, const Occupancy& occ, const Vector<Vec2I>& blockCells,
						   const Vec2I& at, const Vec2I& fp)
		{
			for (int j = 0; j < fp.y; j++)
			{
				for (int i = 0; i < fp.x; i++)
				{
					Vec2I c = at + Vec2I(i, j);
					if (!m.InBounds(c) || m.IsRoad(c) || !occ.IsFree(c) || !blockCells.Contains(c))
						return false;
				}
			}
			return true;
		}

		Vector<Vec2I> AdjacentRoadCells(const CityModel& m, const Vec2I& at, const Vec2I& fp)
		{
			Vector<Vec2I> result;
			for (int j = 0; j < fp.y; j++)
			{
				for (int i = 0; i < fp.x; i++)
				{
					for (int d = 0; d < 4; d++)
					{
						Vec2I n = at + Vec2I(i, j) + DirVec((Dir)d);
						if (m.IsRoad(n) && !result.Contains(n))
							result.Add(n);
					}
				}
			}
			return result;
		}

		CityModel TryGenerate(const CityGenParams& params, UInt32 seed)
		{
			Rng rng(seed);
			CityModel m;
			m.size = params.size;
			m.ground.resize(m.size*m.size, GroundKind::Pavement);

			auto vLines = MakeRoadLines(m.size, rng);
			auto hLines = MakeRoadLines(m.size, rng);
			for (int x : vLines)
			{
				for (int y = 1; y <= m.size - 2; y++)
					m.ground[y*m.size + x] = GroundKind::Road;
			}
			for (int y : hLines)
			{
				for (int x = 1; x <= m.size - 2; x++)
					m.ground[y*m.size + x] = GroundKind::Road;
			}

			auto blocks = FindBlocks(m);
			Occupancy occ(m.size);

			// park: interior block closest to the city center gets the token fountain
			Vec2F center(m.size*0.5f - 0.5f, m.size*0.5f - 0.5f);
			int parkIdx = -1;
			float parkDist = 1e9f;
			for (int b = 0; b < blocks.Count(); b++)
			{
				if (blocks[b].border)
					continue;
				float dist = (blocks[b].Centroid() - center).Length();
				if (dist < parkDist)
				{
					parkDist = dist;
					parkIdx = b;
				}
			}

			if (parkIdx >= 0)
			{
				auto& park = blocks[parkIdx];
				Vec2F c = park.Centroid();
				Vec2I best = park.cells[0];
				for (auto& cell : park.cells)
				{
					m.ground[cell.y*m.size + cell.x] = GroundKind::Grass;
					if ((Vec2F((float)cell.x, (float)cell.y) - c).Length() <
						(Vec2F((float)best.x, (float)best.y) - c).Length())
					{
						best = cell;
					}
				}
				m.fountainCell = best;
				occ.Take(best, Vec2I(1, 1));
				m.decors.Add({ "fountain", Vec2F((float)best.x, (float)best.y) });
			}

			// offices, one per non-park interior block while possible
			Vector<int> officeBlocks;
			for (int b = 0; b < blocks.Count(); b++)
			{
				if (!blocks[b].border && b != parkIdx)
					officeBlocks.Add(b);
			}
			for (int i = officeBlocks.Count() - 1; i > 0; i--)
				std::swap(officeBlocks[i], officeBlocks[rng.Range(0, i)]);

			for (int orderIdx = 0; orderIdx < params.ordersCount && !officeBlocks.IsEmpty(); orderIdx++)
			{
				int blockIdx = officeBlocks[orderIdx%officeBlocks.Count()];
				auto& block = blocks[blockIdx];

				int typeIdx = rng.Range(0, 2);
				BuildingInfo office;
				bool placed = false;
				for (int attempt = 0; attempt < 3 && !placed; attempt++, typeIdx = (typeIdx + 1)%3)
				{
					auto& type = kOffices[typeIdx];
					Vec2I fp(type.fw, type.fh);
					Vector<Vec2I> candidates;
					for (auto& cell : block.cells)
					{
						if (FootprintFits(m, occ, block.cells, cell, fp) &&
							!AdjacentRoadCells(m, cell, fp).IsEmpty())
						{
							candidates.Add(cell);
						}
					}
					if (candidates.IsEmpty())
						continue;

					office.spriteId = type.sprite;
					office.cell = candidates[rng.Range(0, candidates.Count() - 1)];
					office.footprint = fp;
					office.orderIndex = m.orders.Count();
					placed = true;
				}
				if (!placed)
					continue;

				occ.Take(office.cell, office.footprint);
				OrderInfo order;
				order.name = kOrderNames[m.orders.Count()%10];
				order.amount = rng.Range(params.minOrderAmount/5, params.maxOrderAmount/5)*5;
				order.officeCell = office.cell;
				order.deliveryCells = AdjacentRoadCells(m, office.cell, office.footprint);
				m.orders.Add(order);
				m.buildings.Add(office);
			}

			// houses fill block cells adjacent to roads
			for (int b = 0; b < blocks.Count(); b++)
			{
				if (b == parkIdx)
					continue;
				for (auto& cell : blocks[b].cells)
				{
					if (!occ.IsFree(cell) || rng.Frand() > 0.8f)
						continue;
					if (AdjacentRoadCells(m, cell, Vec2I(1, 1)).IsEmpty())
						continue;

					int typeIdx = rng.Range(0, 9);
					auto& type = kHouses[typeIdx];
					Vec2I fp(type.fw, type.fh);
					if (!FootprintFits(m, occ, blocks[b].cells, cell, fp))
					{
						typeIdx = rng.Range(0, 6); // 1x1 fallback
						fp = Vec2I(1, 1);
					}

					BuildingInfo house;
					house.spriteId = kHouses[typeIdx].sprite;
					house.cell = cell;
					house.footprint = fp;
					occ.Take(cell, fp);
					m.buildings.Add(house);
				}
			}

			// decor on the remaining free block cells
			int kiosks = 0;
			for (int b = 0; b < blocks.Count(); b++)
			{
				bool isPark = b == parkIdx;
				for (auto& cell : blocks[b].cells)
				{
					if (!occ.IsFree(cell))
						continue;

					float roll = rng.Frand();
					Vec2F pos((float)cell.x, (float)cell.y);
					if (isPark)
					{
						if (roll < 0.45f)
							m.decors.Add({ rng.Frand() < 0.6f ? "tree_big" : "tree_small", pos });
						else if (roll < 0.6f)
							m.decors.Add({ "bench", pos });
					}
					else if (roll < 0.06f && kiosks < 2)
					{
						m.decors.Add({ "kiosk", pos });
						occ.Take(cell, Vec2I(1, 1));
						kiosks++;
					}
					else if (roll < 0.28f)
						m.decors.Add({ rng.Frand() < 0.5f ? "tree_big" : "tree_small", pos });
					else if (roll < 0.38f)
						m.decors.Add({ "bench", pos });
					else if (roll < 0.48f)
						m.decors.Add({ "lamp", pos });
				}
			}

			// token source: road cells near the fountain
			for (int radius = 2; radius <= m.size && m.sourceCells.IsEmpty(); radius++)
			{
				for (int j = 0; j < m.size; j++)
				{
					for (int i = 0; i < m.size; i++)
					{
						Vec2I c(i, j);
						if (m.IsRoad(c) &&
							Math::Abs(i - m.fountainCell.x) + Math::Abs(j - m.fountainCell.y) <= radius)
						{
							m.sourceCells.Add(c);
						}
					}
				}
			}

			// player start: source cell nearest to the fountain, heading along the road
			m.playerStart = m.sourceCells.IsEmpty() ? Vec2I(1, 1) : m.sourceCells[0];
			for (auto& c : m.sourceCells)
			{
				int dBest = Math::Abs(m.playerStart.x - m.fountainCell.x) + Math::Abs(m.playerStart.y - m.fountainCell.y);
				int d = Math::Abs(c.x - m.fountainCell.x) + Math::Abs(c.y - m.fountainCell.y);
				if (d < dBest)
					m.playerStart = c;
			}
			for (int d = 0; d < 4; d++)
			{
				if (m.IsRoad(m.playerStart + DirVec((Dir)d)))
				{
					m.playerStartDir = (Dir)d;
					break;
				}
			}

			// traffic spawn cells
			Vector<Vec2I> roadCells;
			for (int j = 0; j < m.size; j++)
			{
				for (int i = 0; i < m.size; i++)
				{
					if (m.IsRoad(Vec2I(i, j)) && Vec2I(i, j) != m.playerStart)
						roadCells.Add(Vec2I(i, j));
				}
			}
			for (int t = 0; t < params.trafficCars && !roadCells.IsEmpty(); t++)
			{
				int idx = rng.Range(0, roadCells.Count() - 1);
				m.trafficStarts.Add(roadCells[idx]);
				roadCells.RemoveAt(idx);
			}

			return m;
		}

		bool Validate(const CityModel& m, const CityGenParams& params)
		{
			if (m.orders.Count() < params.ordersCount || m.sourceCells.IsEmpty())
				return false;

			auto reachable = m.ReachableRoadCells(m.playerStart);
			for (auto& order : m.orders)
			{
				bool anyReachable = false;
				for (auto& c : order.deliveryCells)
					anyReachable |= reachable.Contains(c);
				if (!anyReachable)
					return false;
			}
			return true;
		}
	}

	CityModel GenerateCity(const CityGenParams& params, UInt32 seed)
	{
		CityModel model;
		for (int attempt = 0; attempt < 16; attempt++)
		{
			model = TryGenerate(params, seed + (UInt32)attempt*7919u);
			if (Validate(model, params))
				return model;
		}
		return model;
	}
}
// --- META ---

ENUM_META(td::GroundKind, td__GroundKind)
{
    ENUM_ENTRY(Grass);
    ENUM_ENTRY(Pavement);
    ENUM_ENTRY(Road);
}
END_ENUM_META;
// --- END META ---
