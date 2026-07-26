#include "o2/stdafx.h"
#include "TokenDelivery/CityModel.h"

#include "TokenDelivery/BlockCatalog.g.h"

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
		const char* kOrderNames[] = {
			"London", "Paris", "Berlin", "Madrid", "Rome", "Vienna", "Prague", "Amsterdam",
			"Lisbon", "Dublin"
		};

		// --- block layout rules, in grid units --------------------------------------------
		// Mirrors Tools/ArtPipeline/blocks/layout.py. Buildings only ever stand on a plot's
		// perimeter, and only on its south and east edges: a sprite grows upward from its
		// footprint, so one standing on the north or west edge throws its roof straight over
		// the street behind it and hides the road the player drives on. On the south edge the
		// same height falls inside the plot's own courtyard instead. The rest of the plot is
		// deliberately sparse — one composite piece and a lot of open ground.
		const int   kMaxDepth = 4;     // how deep an ordinary building may eat into the plot
		const int   kOfficeDepth = 6;  // an office is the exception, it is the biggest thing
		const int   kCore = 6;         // units of interior kept clear of buildings per axis
		const int   kMinDepth = 3;     // nothing in the vocabulary is shallower
		const float kFrontFill[2] = { 0.46f, 0.68f };  // share of a south/east edge built up
		const float kBackFill[2] = { 0.10f, 0.22f };   // ... and of a north/west one
		const int   kGap[2] = { 2, 4 };                // units left between two buildings
		const float kOpenTarget = 0.38f;               // plot share that must stay free
		const float kHedgeChance = 0.22f;              // a border gap becomes a front garden
		const float kPropDensity = 0.03f;              // single props per free unit
		const int   kBackEdgeMin = 16;                 // a back edge builds only on a big plot

		enum { EdgeS = 0, EdgeE = 1, EdgeN = 2, EdgeW = 3 };

		float FrandRange(Rng& rng, const float range[2])
		{
			return range[0] + rng.Frand()*(range[1] - range[0]);
		}

		Vector<const art::BlockItem*> ItemsOfKind(int kind)
		{
			Vector<const art::BlockItem*> result;
			for (auto& item : art::kBlockItems)
			{
				if (item.kind == kind)
					result.Add(&item);
			}
			return result;
		}

		const art::BlockItem* FindItem(const char* id)
		{
			for (auto& item : art::kBlockItems)
			{
				if (strcmp(item.id, id) == 0)
					return &item;
			}
			return nullptr;
		}

		template<typename T>
		void Shuffle(Vector<T>& v, Rng& rng)
		{
			for (int i = v.Count() - 1; i > 0; i--)
				std::swap(v[i], v[rng.Range(0, i)]);
		}

		// -----------------------------------------------------
		// Free/taken map of one plot, addressed in grid units
		// -----------------------------------------------------
		struct UnitOccupancy
		{
			int          w, h;
			Vector<bool> taken;

			UnitOccupancy(int w, int h): w(w), h(h) { taken.resize(w*h, false); }

			bool Free(int x, int y, int fw, int fh, int margin = 0) const
			{
				if (x < 0 || y < 0 || x + fw > w || y + fh > h)
					return false;

				for (int j = Math::Max(0, y - margin); j < Math::Min(h, y + fh + margin); j++)
				{
					for (int i = Math::Max(0, x - margin); i < Math::Min(w, x + fw + margin); i++)
					{
						if (taken[j*w + i])
							return false;
					}
				}
				return true;
			}

			void Take(int x, int y, int fw, int fh)
			{
				for (int j = y; j < Math::Min(h, y + fh); j++)
				{
					for (int i = x; i < Math::Min(w, x + fw); i++)
						taken[j*w + i] = true;
				}
			}

			Vector<Vec2I> FreeUnits() const
			{
				Vector<Vec2I> result;
				for (int j = 0; j < h; j++)
				{
					for (int i = 0; i < w; i++)
					{
						if (!taken[j*w + i])
							result.Add(Vec2I(i, j));
					}
				}
				return result;
			}

			int FreeCount() const
			{
				int n = 0;
				for (bool t : taken)
					n += t ? 0 : 1;
				return n;
			}
		};

		// Depth allowed on each edge. The south and east edges carry the block; the north and
		// west ones only join in on a plot big enough that a single shallow row there still
		// leaves the street behind it readable.
		void EdgeDepths(int uw, int uh, Rng& rng, int depths[4])
		{
			depths[EdgeS] = Math::Min(kMaxDepth, Math::Max(kMinDepth, uh - kCore));
			depths[EdgeE] = Math::Min(kMaxDepth, Math::Max(kMinDepth, uw - kCore));
			depths[EdgeN] = depths[EdgeW] = 0;

			Vector<int> back;
			if (uh >= kBackEdgeMin) back.Add(EdgeN);
			if (uw >= kBackEdgeMin) back.Add(EdgeW);
			if (!back.IsEmpty() && rng.Frand() < 0.5f)
				depths[back[rng.Range(0, back.Count() - 1)]] = kMinDepth;
		}

		// Footprint of an item on the edge as (frontage along it, depth into the plot). The
		// sprites all face the camera and cannot be rotated, so a building on a west or east
		// edge keeps its footprint and turns its long side into the depth.
		void FrontageDepth(int edge, const Vec2I& size, int& frontage, int& depth)
		{
			bool alongX = edge == EdgeN || edge == EdgeS;
			frontage = alongX ? size.x : size.y;
			depth = alongX ? size.y : size.x;
		}

		Vec2I EdgeAnchor(int edge, const UnitOccupancy& occ, int along, const Vec2I& size)
		{
			switch (edge)
			{
				case EdgeN: return Vec2I(along, 0);
				case EdgeS: return Vec2I(along, occ.h - size.y);
				case EdgeE: return Vec2I(occ.w - size.x, along);
				default:    return Vec2I(0, along);
			}
		}

		void PlaceEdges(UnitOccupancy& occ, Rng& rng, bool wantOffice, float fillScale,
						Vector<BlockObject>& out)
		{
			int depths[4];
			EdgeDepths(occ.w, occ.h, rng, depths);

			// the office needs more depth than any house, so it takes the whole axis it stands
			// on and the opposite edge of that axis gives up building altogether
			int officeEdge = -1;
			int order[2] = { EdgeS, EdgeE };
			if (rng.Frand() < 0.5f)
				std::swap(order[0], order[1]);
			for (int k = 0; wantOffice && k < 2 && officeEdge < 0; k++)
			{
				int edge = order[k];
				int dim = edge == EdgeS ? occ.h : occ.w;
				if (dim - kCore < 5)
					continue;

				officeEdge = edge;
				depths[edge] = Math::Min(kOfficeDepth, dim - kCore);
				depths[edge == EdgeS ? EdgeN : EdgeW] = 0;
			}

			auto houses = ItemsOfKind((int)ObjectKind::House);
			auto offices = ItemsOfKind((int)ObjectKind::Office);

			for (int edge = 0; edge < 4; edge++)
			{
				int limit = depths[edge];
				if (limit < kMinDepth)
					continue;

				int run = (edge == EdgeN || edge == EdgeS) ? occ.w : occ.h;
				float target = FrandRange(rng, edge < 2 ? kFrontFill : kBackFill)*run*fillScale;
				float built = 0.0f;
				int along = rng.Range(0, 2);
				bool officeLeft = edge == officeEdge;

				while (along < run && built < target)
				{
					Vector<const art::BlockItem*> pool = houses;
					Shuffle(pool, rng);
					if (officeLeft)
					{
						auto shuffled = offices;
						Shuffle(shuffled, rng);
						pool.Insert(shuffled, 0);
					}

					const art::BlockItem* placed = nullptr;
					Vec2I at, size;
					int frontage = 0;
					for (auto item : pool)
					{
						size = Vec2I(item->fw, item->fh);
						int depth;
						FrontageDepth(edge, size, frontage, depth);
						if (depth > limit || along + frontage > run)
							continue;

						at = EdgeAnchor(edge, occ, along, size);
						// a clear unit around every building: the sprites' roofs overhang their
						// base, and two flush footprints grow through each other
						if (!occ.Free(at.x, at.y, size.x, size.y, 1))
							continue;

						placed = item;
						break;
					}
					if (!placed)
					{
						along++;
						continue;
					}

					occ.Take(at.x, at.y, size.x, size.y);
					BlockObject object;
					object.spriteId = placed->id;
					object.unit = at;
					object.unitSize = size;
					object.kind = (ObjectKind)placed->kind;
					out.Add(object);

					built += frontage;
					along += frontage + rng.Range(kGap[0], kGap[1]);
					if (placed->kind == (int)ObjectKind::Office)
						officeLeft = false;
				}
			}
		}

		// Picks among the spots nearest the plot's open middle, biased north-west: the
		// buildings sit on the south and east edges and their height reaches back into the
		// plot, so a courtyard piece placed dead center would be half hidden behind them
		Vec2I CentredSpot(const UnitOccupancy& occ, Vector<Vec2I> spots, Rng& rng)
		{
			Vec2F center(occ.w*0.40f, occ.h*0.32f);
			spots.Sort([&](const Vec2I& a, const Vec2I& b) {
				return (Vec2F((float)a.x, (float)a.y) - center).SqrLength() <
					   (Vec2F((float)b.x, (float)b.y) - center).SqrLength();
			});
			return spots[rng.Range(0, Math::Max(0, spots.Count()/3 - 1))];
		}

		// The large ready-made courtyard pieces. These carry the block: everything after them
		// is decoration around what they establish.
		int PlaceComposites(UnitOccupancy& occ, Rng& rng, int want, const char* forced,
							Vector<BlockObject>& out)
		{
			auto left = ItemsOfKind((int)ObjectKind::Composite);
			// the vocabulary of courtyard pieces is small, so the mid-size props stand in where
			// none of them fits — a plot narrower than five units still deserves a centrepiece
			for (auto item : ItemsOfKind((int)ObjectKind::Prop))
			{
				if (item->fw*item->fh >= 9)
					left.Add(item);
			}
			if (forced)
			{
				if (auto item = FindItem(forced))
				{
					left.RemoveAll([&](const art::BlockItem* i) { return i == item; });
					left.Insert(item, 0);
				}
			}

			int placed = 0;
			while (placed < want && !left.IsEmpty())
			{
				Vector<const art::BlockItem*> fitting;
				Vector<Vector<Vec2I>> fittingSpots;
				for (auto item : left)
				{
					for (int margin = 1; margin >= 0; margin--)
					{
						Vector<Vec2I> spots;
						for (int y = 0; y <= occ.h - item->fh; y++)
						{
							for (int x = 0; x <= occ.w - item->fw; x++)
							{
								if (occ.Free(x, y, item->fw, item->fh, margin))
									spots.Add(Vec2I(x, y));
							}
						}
						if (!spots.IsEmpty())
						{
							fitting.Add(item);
							fittingSpots.Add(spots);
							break;
						}
					}
				}
				if (fitting.IsEmpty())
					break;

				// the biggest piece that fits is usually the right one, but always taking it
				// would give every plot of a given size the same courtyard
				int pick = 0;
				for (int i = 1; i < fitting.Count(); i++)
				{
					if (fitting[i]->fw*fitting[i]->fh > fitting[pick]->fw*fitting[pick]->fh)
						pick = i;
				}
				if (!(placed == 0 && forced) && rng.Frand() >= 0.4f)
					pick = rng.Range(0, fitting.Count() - 1);

				auto item = fitting[pick];
				// a composite carries its own kerb, so keeping it off the south and east plot
				// boundary is what stops that kerb from hanging over the road
				Vector<Vec2I> inner;
				for (auto& s : fittingSpots[pick])
				{
					if (s.x + item->fw < occ.w && s.y + item->fh < occ.h && s.x > 0 && s.y > 0)
						inner.Add(s);
				}
				Vec2I at = CentredSpot(occ, inner.IsEmpty() ? fittingSpots[pick] : inner, rng);

				occ.Take(at.x, at.y, item->fw, item->fh);
				BlockObject object;
				object.spriteId = item->id;
				object.unit = at;
				object.unitSize = Vec2I(item->fw, item->fh);
				object.kind = (ObjectKind)item->kind;
				out.Add(object);

				left.RemoveAll([&](const art::BlockItem* i) { return i == item; });
				placed++;
			}
			return placed;
		}

		// Lays a hedge across some of the gaps on the boundary — a front garden line, not a
		// wall, so the plot still breathes where it is left open
		void CloseGaps(UnitOccupancy& occ, Rng& rng, Vector<BlockObject>& out)
		{
			auto hedge = FindItem("hedge_segment");
			if (!hedge)
				return;

			Vector<Vec2I> ring;
			for (int x = 0; x < occ.w; x++)          ring.Add(Vec2I(x, 0));
			for (int y = 0; y < occ.h; y++)          ring.Add(Vec2I(occ.w - 1, y));
			for (int x = occ.w - 1; x >= 0; x--)     ring.Add(Vec2I(x, occ.h - 1));
			for (int y = occ.h - 1; y >= 0; y--)     ring.Add(Vec2I(0, y));

			Vector<Vector<Vec2I>> runs;
			Vector<Vec2I> current;
			for (auto& c : ring)
			{
				if (occ.taken[c.y*occ.w + c.x])
				{
					if (current.Count() >= 3)
						runs.Add(current);
					current.Clear();
				}
				else
					current.Add(c);
			}
			if (current.Count() >= 3)
				runs.Add(current);

			for (auto& run : runs)
			{
				if (rng.Frand() > kHedgeChance)
					continue;

				// a stretch of the gap, not the whole of it: a hedge running the full length of
				// a plot boundary reads as a wall around the city, not as a front garden
				int length = Math::Min(run.Count() - 2, rng.Range(2, 5));
				int from = rng.Range(1, Math::Max(1, run.Count() - 1 - length));
				for (int i = from; i < from + length; i++)
				{
					Vec2I c = run[i];
					if (!occ.Free(c.x, c.y, 1, 1))
						continue;
					if (occ.FreeCount() <= occ.w*occ.h*kOpenTarget)
						return;

					occ.Take(c.x, c.y, 1, 1);
					BlockObject object;
					object.spriteId = hedge->id;
					object.unit = c;
					object.unitSize = Vec2I(1, 1);
					object.kind = ObjectKind::Prop;
					out.Add(object);
				}
			}
		}

		// A few single props around the courtyard piece, stopping while the plot is still open
		void ScatterProps(UnitOccupancy& occ, Rng& rng, Vector<BlockObject>& out)
		{
			Vector<const art::BlockItem*> street, yard;
			for (auto item : ItemsOfKind((int)ObjectKind::Prop))
			{
				if (item->street)
					street.Add(item);
				else if (strcmp(item->id, "hedge_segment") != 0)
					yard.Add(item);
			}
			if (yard.IsEmpty())
				return;

			int total = occ.w*occ.h;
			int want = Math::Max(2, (int)Math::Round(occ.FreeCount()*kPropDensity));
			for (int attempt = 0; attempt < want*6 && want > 0; attempt++)
			{
				if (occ.FreeCount() <= total*kOpenTarget)
					break;

				bool onStreet = !street.IsEmpty() && rng.Frand() < 0.3f;
				auto& pool = onStreet ? street : yard;
				auto item = pool[rng.Range(0, pool.Count() - 1)];

				Vector<Vec2I> spots;
				for (auto& c : occ.FreeUnits())
				{
					if (!occ.Free(c.x, c.y, item->fw, item->fh))
						continue;
					if (item->street && !(c.x == 0 || c.y == 0 || c.x + item->fw >= occ.w ||
										  c.y + item->fh >= occ.h))
					{
						continue;
					}
					spots.Add(c);
				}
				if (spots.IsEmpty())
					continue;

				Vec2I at = spots[rng.Range(0, spots.Count() - 1)];
				occ.Take(at.x, at.y, item->fw, item->fh);
				BlockObject object;
				object.spriteId = item->id;
				object.unit = at;
				object.unitSize = Vec2I(item->fw, item->fh);
				object.kind = ObjectKind::Prop;
				out.Add(object);
				want--;
			}
		}
	}

	// Painter's order for a plot's objects: B is drawn after A when B lies entirely east or
	// entirely south of A — the separating-axis rule, which is what "in front of" means in this
	// projection. Two objects that separate both ways cannot overlap on screen, so their order
	// is free. A plain (x + y) key gets this wrong the moment one footprint is longer than
	// another: a 4x1 hedge at (0, 1) is in front of a 1x1 bin at (2, 0), yet sorts earlier.
	static void SortIsometric(Vector<BlockObject>& objects)
	{
		int n = objects.Count();
		Vector<Vector<int>> after;
		Vector<int> indegree;
		after.resize(n);
		indegree.resize(n, 0);

		for (int a = 0; a < n; a++)
		{
			Vec2I ap = objects[a].unit, as = objects[a].unitSize;
			for (int b = 0; b < n; b++)
			{
				if (a == b)
					continue;

				Vec2I bp = objects[b].unit, bs = objects[b].unitSize;
				bool behind = bp.x >= ap.x + as.x || bp.y >= ap.y + as.y;
				bool ahead = ap.x >= bp.x + bs.x || ap.y >= bp.y + bs.y;
				if (behind && !ahead)
				{
					after[a].Add(b);
					indegree[b]++;
				}
			}
		}

		auto key = [&](int i) { return objects[i].unit.x + objects[i].unit.y; };
		Vector<int> ready, result;
		for (int i = 0; i < n; i++)
		{
			if (indegree[i] == 0)
				ready.Add(i);
		}
		while (!ready.IsEmpty())
		{
			ready.Sort([&](int a, int b) { return key(a) < key(b); });
			int i = ready[0];
			ready.RemoveAt(0);
			result.Add(i);
			for (int j : after[i])
			{
				if (--indegree[j] == 0)
					ready.Add(j);
			}
		}
		for (int i = 0; i < n; i++)
		{
			if (!result.Contains(i))
				result.Add(i);
		}

		Vector<BlockObject> sorted;
		for (int i : result)
			sorted.Add(objects[i]);
		objects = sorted;
	}

	Vector<BlockObject> LayoutBlock(const Vec2I& plotUnits, Rng& rng,
									const BlockLayoutOptions& options)
	{
		Vector<BlockObject> objects;
		UnitOccupancy occ(plotUnits.x, plotUnits.y);

		int want = plotUnits.x*plotUnits.y >= 180 ? 2 : 1;
		// the centrepiece goes down before the buildings: asked for after them it would find
		// the middle of the plot already eaten by the perimeter and end up in a corner
		int placed = options.centrepiece
			? PlaceComposites(occ, rng, 1, options.centrepiece, objects) : 0;

		PlaceEdges(occ, rng, options.wantOffice, options.fillScale, objects);
		PlaceComposites(occ, rng, want - placed, options.preferred, objects);
		CloseGaps(occ, rng, objects);
		ScatterProps(occ, rng, objects);

		SortIsometric(objects);
		return objects;
	}

	namespace
	{
		struct Block
		{
			Vector<Vec2I> cells;
			bool          border = false;

			Vec2F Centroid() const
			{
				Vec2F sum;
				for (auto& c : cells)
					sum += Vec2F((float)c.x, (float)c.y);
				return sum/(float)cells.Count();
			}
		};

		// Road line coordinates from 1 to size-2. The gap between two lines is the block width
		// in cells: wide blocks are what gives the layout room to leave the streets clear.
		Vector<int> MakeRoadLines(int size, Rng& rng)
		{
			Vector<int> lines { 1 };
			int last = size - 2;
			int cur = 1;
			while (true)
			{
				int gap = rng.Range(3, 4);
				int next = cur + gap + 1;
				if (next + 4 > last)
					break;
				lines.Add(next);
				cur = next;
			}
			if (last - cur - 1 < 3 && lines.Count() > 1)
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

		// One plot to lay out: a rectangle of block cells. The interior blocks are already
		// rectangles; the ring around the city is one L-shaped region, and the layout only
		// makes sense per straight strip, so it gets cut into four.
		struct Plot
		{
			Vec2I cell;   // North-west cell
			Vec2I size;   // Size in cells
			bool  border = false;
		};

		Vector<Plot> SplitIntoPlots(const Block& block)
		{
			Vector<Vec2I> left = block.cells;
			Vector<Plot> plots;
			auto has = [&](const Vec2I& c) { return left.Contains(c); };

			while (!left.IsEmpty())
			{
				Vec2I best = left[0];
				for (auto& c : left)
				{
					if (c.y < best.y || (c.y == best.y && c.x < best.x))
						best = c;
				}

				int w = 0;
				while (has(best + Vec2I(w, 0)))
					w++;

				int h = 1;
				while (true)
				{
					bool full = true;
					for (int i = 0; i < w && full; i++)
						full = has(best + Vec2I(i, h));
					if (!full)
						break;
					h++;
				}

				for (int j = 0; j < h; j++)
				{
					for (int i = 0; i < w; i++)
						left.Remove(best + Vec2I(i, j));
				}
				plots.Add({ best, Vec2I(w, h), block.border });
			}
			return plots;
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

		// Cell span of an object's footprint, in cells
		void ObjectCells(const BlockObject& o, Vec2I& cell, Vec2I& span)
		{
			int x0 = o.unit.x/kUnitsPerCell, y0 = o.unit.y/kUnitsPerCell;
			int x1 = (o.unit.x + o.unitSize.x - 1)/kUnitsPerCell;
			int y1 = (o.unit.y + o.unitSize.y - 1)/kUnitsPerCell;
			cell = Vec2I(x0, y0);
			span = Vec2I(x1 - x0 + 1, y1 - y0 + 1);
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

			Vector<Plot> plots;
			for (auto& block : FindBlocks(m))
				plots.Add(SplitIntoPlots(block));

			// the plaza with the token fountain goes on the interior plot closest to the center
			Vec2F center(m.size*0.5f - 0.5f, m.size*0.5f - 0.5f);
			int plazaIdx = -1;
			float plazaDist = 1e9f;
			for (int p = 0; p < plots.Count(); p++)
			{
				auto& plot = plots[p];
				if (plot.border || plot.size.x < 2 || plot.size.y < 2)
					continue;

				Vec2F c((float)plot.cell.x + plot.size.x*0.5f - 0.5f,
						(float)plot.cell.y + plot.size.y*0.5f - 0.5f);
				float dist = (c - center).Length();
				if (dist < plazaDist)
				{
					plazaDist = dist;
					plazaIdx = p;
				}
			}

			// the plaza's fountain is the token source; if the piece cannot be fitted the plot
			// center still stands in, so the source never lands outside the city
			if (plazaIdx >= 0)
			{
				auto& plot = plots[plazaIdx];
				m.fountainCell = Vec2F(plot.cell.x + plot.size.x*0.5f - 0.5f,
									   plot.cell.y + plot.size.y*0.5f - 0.5f);
			}

			// offices go on the interior plots, one order each
			Vector<int> officePlots;
			for (int p = 0; p < plots.Count(); p++)
			{
				if (!plots[p].border && p != plazaIdx && plots[p].size.x >= 2 && plots[p].size.y >= 2)
					officePlots.Add(p);
			}
			Shuffle(officePlots, rng);
			officePlots.Resize(Math::Min(officePlots.Count(), params.ordersCount));

			// the vocabulary of courtyard pieces is short, so each plot asks for the one used
			// least so far — without that a whole city ends up with the same garden everywhere
			Map<String, int> composeUse;
			for (auto item : ItemsOfKind((int)ObjectKind::Composite))
				composeUse[String(item->id)] = 0;

			for (int p = 0; p < plots.Count(); p++)
			{
				auto& plot = plots[p];
				Vec2I units(plot.size.x*kUnitsPerCell, plot.size.y*kUnitsPerCell);
				bool wantOffice = officePlots.Contains(p);

				String rarest;
				for (auto& kv : composeUse)
				{
					if (rarest.IsEmpty() || kv.second < composeUse[rarest])
						rarest = kv.first;
				}

				BlockLayoutOptions options;
				options.wantOffice = wantOffice;
				options.preferred = rarest.Data();
				// the rim strips are one cell deep, so any building on them throws its roof
				// clear over the ring road; they stay mostly greenery and street furniture
				if (plot.border)
					options.fillScale = 0.45f;
				if (p == plazaIdx)
					options.centrepiece = "plaza_fountain";
				auto objects = LayoutBlock(units, rng, options);

				for (auto& object : objects)
				{
					object.unit += plot.cell*kUnitsPerCell;

					if (object.kind == ObjectKind::Office && wantOffice &&
						m.orders.Count() < params.ordersCount)
					{
						Vec2I cell, span;
						ObjectCells(object, cell, span);
						auto roads = AdjacentRoadCells(m, cell, span);
						if (!roads.IsEmpty())
						{
							// the office spans more than one cell, so the navigation arrow and
							// the tooltip aim at the middle of it, not at its north-west corner
							Vec2F middle = object.CenterCell();
							Vec2I centreCell((int)Math::Round(middle.x), (int)Math::Round(middle.y));
							object.orderIndex = m.orders.Count();
							OrderInfo order;
							order.name = kOrderNames[m.orders.Count()%10];
							order.amount = rng.Range(params.minOrderAmount/5,
													 params.maxOrderAmount/5)*5;
							order.officeCell = centreCell;
							order.deliveryCells = roads;
							m.orders.Add(order);
						}
					}
					if (composeUse.ContainsKey(object.spriteId))
						composeUse[object.spriteId]++;
					if (p == plazaIdx && object.spriteId == "plaza_fountain")
						m.fountainCell = object.CenterCell();

					m.objects.Add(object);
				}

				// A cell that stayed half open reads as courtyard lawn. What stands on the rest
				// of it covers the grass with its own ground, so a market or a house on the
				// same cell simply hides the part it occupies.
				Vector<int> covered;
				covered.resize(plot.size.x*plot.size.y, 0);
				for (auto& object : objects)
				{
					Vec2I local = object.unit - plot.cell*kUnitsPerCell;
					for (int uy = local.y; uy < local.y + object.unitSize.y; uy++)
					{
						for (int ux = local.x; ux < local.x + object.unitSize.x; ux++)
						{
							int ci = ux/kUnitsPerCell, cj = uy/kUnitsPerCell;
							if (ci < 0 || cj < 0 || ci >= plot.size.x || cj >= plot.size.y)
								continue;

							covered[cj*plot.size.x + ci]++;
						}
					}
				}
				const int cellUnits = kUnitsPerCell*kUnitsPerCell;
				for (int j = 0; j < plot.size.y; j++)
				{
					for (int i = 0; i < plot.size.x; i++)
					{
						int idx = j*plot.size.x + i;
						if (covered[idx]*3 > cellUnits)
							continue;

						Vec2I cell = plot.cell + Vec2I(i, j);
						m.ground[cell.y*m.size + cell.x] = GroundKind::Grass;
					}
				}
			}

			// token source: road cells near the plaza fountain
			Vec2I fountain((int)Math::Round(m.fountainCell.x), (int)Math::Round(m.fountainCell.y));
			for (int radius = 2; radius <= m.size && m.sourceCells.IsEmpty(); radius++)
			{
				for (int j = 0; j < m.size; j++)
				{
					for (int i = 0; i < m.size; i++)
					{
						Vec2I c(i, j);
						if (m.IsRoad(c) &&
							Math::Abs(i - fountain.x) + Math::Abs(j - fountain.y) <= radius)
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
				int dBest = Math::Abs(m.playerStart.x - fountain.x) +
							Math::Abs(m.playerStart.y - fountain.y);
				int d = Math::Abs(c.x - fountain.x) + Math::Abs(c.y - fountain.y);
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

ENUM_META(td::ObjectKind, td__ObjectKind)
{
    ENUM_ENTRY(Composite);
    ENUM_ENTRY(House);
    ENUM_ENTRY(Office);
    ENUM_ENTRY(Prop);
}
END_ENUM_META;
// --- END META ---
