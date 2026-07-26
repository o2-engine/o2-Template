#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "TokenDelivery/CarSim.h"
#include "TokenDelivery/CityModel.h"
#include "TokenDelivery/GameSession.h"

using namespace o2;
using namespace td;

namespace
{
	// straight EW road at j=2 with x in [1..5] on a 7x7 grid; extra cells added by tests
	CityModel MakeRoadCity(const Vector<Vec2I>& extraRoad = {})
	{
		CityModel m;
		m.size = 7;
		m.ground.resize(m.size*m.size, GroundKind::Pavement);
		for (int x = 1; x <= 5; x++)
			m.ground[2*m.size + x] = GroundKind::Road;
		for (auto& c : extraRoad)
			m.ground[c.y*m.size + c.x] = GroundKind::Road;
		m.playerStart = Vec2I(1, 2);
		m.playerStartDir = Dir::E;
		return m;
	}

	CarTuning TestCarTuning()
	{
		CarTuning t;
		t.maxSpeed = 2.0f;
		t.accel = 100.0f; // instant acceleration keeps distance math simple
		return t;
	}

	void TickMany(CarSim& car, const CityModel& city, const CarInput& input, float time,
				  float dt = 1.0f/60.0f)
	{
		for (float elapsed = 0.0f; elapsed < time; elapsed += dt)
			car.Tick(dt, input, city);
	}
}

TEST(TokenCityGen, GeneratesValidCitiesForManySeeds)
{
	for (UInt32 seed = 1; seed <= 20; seed++)
	{
		CityGenParams params;
		auto city = GenerateCity(params, seed);

		ASSERT_EQ(city.orders.Count(), params.ordersCount) << "seed " << seed;
		ASSERT_FALSE(city.sourceCells.IsEmpty()) << "seed " << seed;
		ASSERT_TRUE(city.IsRoad(city.playerStart)) << "seed " << seed;

		auto reachable = city.ReachableRoadCells(city.playerStart);
		for (auto& order : city.orders)
		{
			ASSERT_FALSE(order.deliveryCells.IsEmpty()) << "seed " << seed;
			bool anyReachable = false;
			for (auto& c : order.deliveryCells)
				anyReachable |= reachable.Contains(c);
			ASSERT_TRUE(anyReachable) << "seed " << seed << " order " << order.name.Data();

			ASSERT_GE(order.amount, params.minOrderAmount);
			ASSERT_LE(order.amount, params.maxOrderAmount);
			ASSERT_EQ(order.amount%5, 0);
		}

		ASSERT_TRUE(city.sourceCells.Contains(city.playerStart));
	}
}

TEST(TokenCityGen, RoadMaskMatchesNeighbours)
{
	auto city = GenerateCity(CityGenParams(), 5);
	for (int j = 0; j < city.size; j++)
	{
		for (int i = 0; i < city.size; i++)
		{
			int mask = city.RoadMask(Vec2I(i, j));
			for (int d = 0; d < 4; d++)
			{
				bool neighbourRoad = city.IsRoad(Vec2I(i, j) + DirVec((Dir)d));
				EXPECT_EQ((mask & (1 << d)) != 0, neighbourRoad);
			}
		}
	}
}

TEST(TokenCityGen, BuildingsDoNotOverlap)
{
	for (UInt32 seed = 1; seed <= 10; seed++)
	{
		auto city = GenerateCity(CityGenParams(), seed*31);
		Vector<Vec2I> taken;
		for (auto& b : city.buildings)
		{
			for (int j = 0; j < b.footprint.y; j++)
			{
				for (int i = 0; i < b.footprint.x; i++)
				{
					Vec2I c = b.cell + Vec2I(i, j);
					ASSERT_TRUE(city.InBounds(c));
					ASSERT_FALSE(city.IsRoad(c)) << "seed " << seed;
					ASSERT_FALSE(taken.Contains(c)) << "seed " << seed;
					taken.Add(c);
				}
			}
		}
	}
}

TEST(TokenCityGen, DeterministicBySeed)
{
	auto a = GenerateCity(CityGenParams(), 42);
	auto b = GenerateCity(CityGenParams(), 42);
	ASSERT_EQ(a.ground, b.ground);
	ASSERT_EQ(a.buildings.Count(), b.buildings.Count());
	ASSERT_EQ(a.orders.Count(), b.orders.Count());
	for (int i = 0; i < a.orders.Count(); i++)
		EXPECT_EQ(a.orders[i].amount, b.orders[i].amount);
	EXPECT_EQ(a.playerStart, b.playerStart);
}

TEST(TokenCityGen, LevelProgressionGrowsOrders)
{
	SessionTuning tuning;
	auto level1 = GameSession::ParamsForLevel(1, tuning);
	auto level4 = GameSession::ParamsForLevel(4, tuning);
	auto level10 = GameSession::ParamsForLevel(10, tuning);
	EXPECT_EQ(level1.ordersCount, 3);
	EXPECT_EQ(level4.ordersCount, 6);
	EXPECT_EQ(level10.ordersCount, tuning.maxOrders);
	EXPECT_EQ(level1.size, tuning.baseCitySize);
	EXPECT_LE(level10.size, tuning.maxCitySize);
}

TEST(TokenCarSim, AcceleratesToMaxSpeedAndDrives)
{
	auto city = MakeRoadCity();
	CarTuning tuning; // real acceleration curve
	CarSim car;
	car.Reset(tuning, Vec2F(1.0f, 2.0f), Dir::E);

	TickMany(car, city, CarInput(), 3.0f);
	EXPECT_NEAR(car.GetSpeed(), tuning.maxSpeed, 0.01f);
	EXPECT_GT(car.GetPos().x, 1.5f);
	EXPECT_EQ(car.GetPos().y, 2.0f); // stays on the road center line
}

TEST(TokenCarSim, HeldRightTurnAppliesAtIntersection)
{
	// vertical branch at x=3 going south (the right side of an eastbound car)
	auto city = MakeRoadCity({ Vec2I(3, 3), Vec2I(3, 4), Vec2I(3, 5) });
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(1.0f, 2.0f), Dir::E);

	CarInput input;
	input.turnRight = true; // held early, must apply exactly at the x=3 intersection
	TickMany(car, city, input, 1.5f);

	EXPECT_EQ(car.GetDir(), Dir::S);
	EXPECT_NEAR(car.GetPos().x, 3.0f, 0.001f);
	EXPECT_GT(car.GetPos().y, 2.0f);
}

TEST(TokenCarSim, HeldLeftTurnAppliesAtIntersection)
{
	// vertical branch at x=3 going north (the left side of an eastbound car)
	auto city = MakeRoadCity({ Vec2I(3, 1), Vec2I(3, 0) });
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(1.0f, 2.0f), Dir::E);

	CarInput input;
	input.turnLeft = true;
	TickMany(car, city, input, 1.5f);

	EXPECT_EQ(car.GetDir(), Dir::N);
	EXPECT_NEAR(car.GetPos().x, 3.0f, 0.001f);
	EXPECT_LT(car.GetPos().y, 2.0f);
}

TEST(TokenCarSim, TurnLandsShortlyAfterTheCenterButNotFarther)
{
	auto city = MakeRoadCity({ Vec2I(3, 3), Vec2I(3, 4) });
	CarSim car;

	// just past the intersection, inside the window: pivots back at the center and turns
	car.Reset(TestCarTuning(), Vec2F(3.2f, 2.0f), Dir::E);
	CarInput input;
	input.turnRight = true;
	car.Tick(1.0f/60.0f, input, city);
	EXPECT_EQ(car.GetDir(), Dir::S);
	EXPECT_NEAR(car.GetPos().x, 3.0f, 0.001f);
	EXPECT_GT(car.GetPos().y, 2.19f);

	// farther than the window: the command does not land on the passed intersection
	car.Reset(TestCarTuning(), Vec2F(3.55f, 2.0f), Dir::E);
	car.Tick(1.0f/60.0f, input, city);
	EXPECT_EQ(car.GetDir(), Dir::E);
}

TEST(TokenCarSim, AutoTurnPrefersLeftOfTwoSides)
{
	// both branches at x=3: space takes the left one (north for an eastbound car)
	auto city = MakeRoadCity({ Vec2I(3, 1), Vec2I(3, 0), Vec2I(3, 3), Vec2I(3, 4) });
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(2.5f, 2.0f), Dir::E);

	CarInput input;
	input.turnAuto = true;
	TickMany(car, city, input, 0.5f);
	EXPECT_EQ(car.GetDir(), Dir::N);
}

TEST(TokenCarSim, AutoTurnTakesTheOnlySide)
{
	auto city = MakeRoadCity({ Vec2I(3, 3), Vec2I(3, 4) });
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(2.5f, 2.0f), Dir::E);

	CarInput input;
	input.turnAuto = true;
	TickMany(car, city, input, 0.5f);
	EXPECT_EQ(car.GetDir(), Dir::S);
}

TEST(TokenCarSim, TurnLosesSpeed)
{
	auto city = MakeRoadCity({ Vec2I(3, 3), Vec2I(3, 4) });
	CarTuning tuning = TestCarTuning();
	tuning.accel = 100.0f;
	CarSim car;
	car.Reset(tuning, Vec2F(1.0f, 2.0f), Dir::E);

	CarInput straight;
	TickMany(car, city, straight, 0.3f); // reach max speed before the intersection
	float speedBefore = car.GetSpeed();
	ASSERT_NEAR(speedBefore, tuning.maxSpeed, 0.01f);

	CarInput turn;
	turn.turnRight = true;
	// drive up to just past the intersection with tiny steps, watching for the turn
	bool turned = false;
	for (int i = 0; i < 400 && !turned; i++)
	{
		car.Tick(1.0f/240.0f, turn, city);
		if (car.JustTurned())
		{
			turned = true;
			EXPECT_LT(car.GetSpeed(), speedBefore*(1.0f - tuning.turnSpeedLoss) + 0.01f);
		}
	}
	ASSERT_TRUE(turned);
}

TEST(TokenCarSim, TurnShiftsCarSidewaysAndStays)
{
	auto city = MakeRoadCity({ Vec2I(3, 3), Vec2I(3, 4), Vec2I(3, 5) });
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(1.0f, 2.0f), Dir::E);

	CarInput input;
	input.turnRight = true;
	bool turned = false;
	for (int i = 0; i < 200 && !turned; i++)
	{
		car.Tick(1.0f/60.0f, input, city);
		turned = car.JustTurned();
	}
	ASSERT_TRUE(turned);
	ASSERT_EQ(car.GetDir(), Dir::S);

	TickMany(car, city, CarInput(), 0.8f); // key released, long after the turn
	Vec2F offset = car.GetVisualPos() - car.GetPos();
	EXPECT_GT(offset.Length(), 0.05f);          // the shift persists, no spring back
	EXPECT_LE(offset.Length(), 0.28f + 0.001f); // and stays within the road bounds
}

TEST(TokenCarSim, HeldKeyShiftsSidewaysAndBouncesAtTheEdge)
{
	auto city = MakeRoadCity(); // straight road: nowhere to turn right
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(1.0f, 2.0f), Dir::E);

	// heading E: right of the car is S (+y), so the visual offset grows in +y
	CarInput input;
	input.turnRight = true;
	bool edgeReached = false;
	float reboundMin = 10.0f;
	for (int i = 0; i < 90; i++)
	{
		car.Tick(1.0f/60.0f, input, city);
		float d = car.GetVisualPos().y - car.GetPos().y;
		if (!edgeReached && d >= 0.27f)
			edgeReached = true;
		else if (edgeReached)
			reboundMin = Math::Min(reboundMin, d);
	}
	EXPECT_TRUE(edgeReached);          // the lunge crosses the road to the edge
	EXPECT_LT(reboundMin, 0.24f);      // and visibly bounces off it
}

TEST(TokenCarSim, ForcedTurnPrefersRight)
{
	// dead end at x=5 with branches both north and south: the car picks the right one
	auto city = MakeRoadCity({ Vec2I(5, 1), Vec2I(5, 0), Vec2I(5, 3), Vec2I(5, 4) });
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(4.0f, 2.0f), Dir::E);

	bool turned = false;
	for (int i = 0; i < 300 && !turned; i++)
	{
		car.Tick(1.0f/60.0f, CarInput(), city);
		turned = car.JustTurned();
	}
	ASSERT_TRUE(turned);
	EXPECT_EQ(car.GetDir(), Dir::S); // right of E
}

TEST(TokenCarSim, ForcedTurnAtDeadEnd)
{
	// road ends at x=5; branch to the south from (5,2)
	auto city = MakeRoadCity({ Vec2I(5, 3), Vec2I(5, 4) });
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(4.0f, 2.0f), Dir::E);

	// tick until the forced turn at (5,2) happens
	bool turned = false;
	for (int i = 0; i < 300 && !turned; i++)
	{
		car.Tick(1.0f/60.0f, CarInput(), city);
		turned = car.JustTurned();
	}
	ASSERT_TRUE(turned);
	EXPECT_EQ(car.GetDir(), Dir::S); // the only continuation
	EXPECT_NEAR(car.GetPos().x, 5.0f, 0.001f);
}

TEST(TokenCarSim, DeadEndWithNoExitsReversesBack)
{
	auto city = MakeRoadCity();
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(4.0f, 2.0f), Dir::E);

	TickMany(car, city, CarInput(), 1.5f);
	EXPECT_EQ(car.GetDir(), Dir::W); // bounced back from the dead end at x=5
}

TEST(TokenCarSim, FuelEmptyStopsTheCar)
{
	auto city = MakeRoadCity();
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(1.0f, 2.0f), Dir::E);
	TickMany(car, city, CarInput(), 0.5f);
	ASSERT_GT(car.GetSpeed(), 0.0f);

	CarInput noFuel;
	noFuel.fuelEmpty = true;
	TickMany(car, city, noFuel, 2.0f);
	EXPECT_TRUE(car.IsStopped());
}

namespace
{
	// road city with a source at the start and one office at cell (4,1) delivering from (4,2)
	CityModel MakeSessionCity()
	{
		CityModel m = MakeRoadCity();
		m.sourceCells.Add(Vec2I(1, 2));
		m.fountainCell = Vec2I(1, 1);

		BuildingInfo office;
		office.spriteId = "office_classic";
		office.cell = Vec2I(4, 1);
		office.footprint = Vec2I(1, 1);
		office.orderIndex = 0;
		m.buildings.Add(office);

		OrderInfo order;
		order.name = "London";
		order.amount = 50;
		order.officeCell = office.cell;
		order.deliveryCells.Add(Vec2I(4, 2));
		m.orders.Add(order);
		return m;
	}

	SessionTuning TestSessionTuning()
	{
		SessionTuning t;
		t.car = TestCarTuning();
		t.fillRate = 200.0f;
		return t;
	}
}

TEST(TokenSession, FillsTokensNearSourceAndDeliversOrder)
{
	GameSession session;
	session.StartWithCity(MakeSessionCity(), TestSessionTuning());

	// stand still is impossible — the car pulls away, but the fill radius covers the first
	// moments; with a high fill rate the car leaves the source with enough tokens
	GameInput input;
	bool completed = false;
	for (int i = 0; i < 600 && !completed; i++)
	{
		session.Tick(1.0f/60.0f, input);
		completed |= session.ConsumeCompletedOrder() == 0;
	}

	ASSERT_TRUE(completed);
	EXPECT_TRUE(session.IsOrderCompleted(0));
	EXPECT_EQ(session.GetState(), SessionState::Won);
}

// the win freezes the session, so a non-consuming read would replay the last delivery
// event on every frame behind the win window
TEST(TokenSession, CompletedOrderEventDoesNotRepeatAfterTheWin)
{
	GameSession session;
	session.StartWithCity(MakeSessionCity(), TestSessionTuning());

	GameInput input;
	int completed = -1;
	for (int i = 0; i < 600 && completed < 0; i++)
	{
		session.Tick(1.0f/60.0f, input);
		completed = session.ConsumeCompletedOrder();
	}

	ASSERT_EQ(completed, 0);
	ASSERT_EQ(session.GetState(), SessionState::Won);
	EXPECT_EQ(session.ConsumeCompletedOrder(), -1);
}

TEST(TokenSession, NotEnoughTokensLeavesOrderActive)
{
	auto city = MakeSessionCity();
	city.sourceCells.Clear(); // no refill: the car never gathers tokens
	city.sourceCells.Add(Vec2I(0, 0));

	GameSession session;
	session.StartWithCity(city, TestSessionTuning());

	GameInput input;
	for (int i = 0; i < 300; i++)
		session.Tick(1.0f/60.0f, input);

	EXPECT_FALSE(session.IsOrderCompleted(0));
	EXPECT_EQ(session.GetState(), SessionState::Playing);
	EXPECT_EQ(session.GetTokens(), 0);
}

TEST(TokenSession, FuelRunsOutAndLosesWhenOrdersRemain)
{
	auto city = MakeSessionCity();
	city.sourceCells.Clear();
	city.sourceCells.Add(Vec2I(0, 0));

	SessionTuning tuning = TestSessionTuning();
	tuning.fuelTime = 0.5f;

	GameSession session;
	session.StartWithCity(city, tuning);

	GameInput input;
	for (int i = 0; i < 400; i++)
		session.Tick(1.0f/60.0f, input);

	EXPECT_EQ(session.GetState(), SessionState::Lost);
	EXPECT_LE(session.GetFuel(), 0.0f);
	EXPECT_TRUE(session.GetCar().IsStopped());
}

TEST(TokenSession, GeneratedLevelStartIsOnSource)
{
	GameSession session;
	session.Start(1, 7);

	GameInput input;
	session.Tick(1.0f/60.0f, input);
	EXPECT_TRUE(session.IsFilling()); // player always starts at the token source
	EXPECT_EQ(session.GetState(), SessionState::Playing);
}
