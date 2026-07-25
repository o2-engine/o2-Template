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

TEST(TokenCarSim, BufferedTurnAppliesAtIntersection)
{
	// vertical branch at x=3 going south
	auto city = MakeRoadCity({ Vec2I(3, 3), Vec2I(3, 4), Vec2I(3, 5) });
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(1.0f, 2.0f), Dir::E);

	CarInput input;
	input.hasDesired = true;
	input.desired = Dir::S; // pressed early, must apply exactly at the x=3 intersection
	TickMany(car, city, input, 1.5f);

	EXPECT_EQ(car.GetDir(), Dir::S);
	EXPECT_NEAR(car.GetPos().x, 3.0f, 0.001f);
	EXPECT_GT(car.GetPos().y, 2.0f);
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
	turn.hasDesired = true;
	turn.desired = Dir::S;
	// drive up to just past the intersection with tiny steps, watching for the turn
	bool turned = false;
	for (int i = 0; i < 400 && !turned; i++)
	{
		car.Tick(1.0f/240.0f, turn, city);
		if (car.JustTurned())
		{
			turned = true;
			EXPECT_LT(car.GetSpeed(), speedBefore*(1.0f - tuning.turnSpeedLoss) + 0.01f);
			EXPECT_GT(car.GetDriftIntensity(), 0.0f);
		}
	}
	ASSERT_TRUE(turned);
}

TEST(TokenCarSim, UTurnWorksAnywhere)
{
	auto city = MakeRoadCity();
	CarSim car;
	car.Reset(TestCarTuning(), Vec2F(1.0f, 2.0f), Dir::E);

	TickMany(car, city, CarInput(), 0.3f);
	float posBefore = car.GetPos().x;
	ASSERT_GT(posBefore, 1.0f);

	CarInput reverse;
	reverse.hasDesired = true;
	reverse.desired = Dir::W;
	car.Tick(1.0f/60.0f, reverse, city);
	EXPECT_EQ(car.GetDir(), Dir::W);

	TickMany(car, city, CarInput(), 0.3f);
	EXPECT_LT(car.GetPos().x, posBefore);
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

TEST(TokenCarSim, BoostRaisesSpeedCap)
{
	auto city = MakeRoadCity();
	CarTuning tuning = TestCarTuning();
	CarSim car;
	car.Reset(tuning, Vec2F(1.0f, 2.0f), Dir::E);

	CarInput boost;
	boost.boost = true;
	TickMany(car, city, boost, 1.0f);
	EXPECT_NEAR(car.GetSpeed(), tuning.maxSpeed*tuning.boostFactor, 0.01f);

	TickMany(car, city, CarInput(), 2.0f);
	EXPECT_NEAR(car.GetSpeed(), tuning.maxSpeed, 0.01f);
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
		completed |= session.GetOrderCompletedThisTick() == 0;
	}

	ASSERT_TRUE(completed);
	EXPECT_TRUE(session.IsOrderCompleted(0));
	EXPECT_EQ(session.GetState(), SessionState::Won);
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

TEST(TokenSession, BoostReserveDrainsWhileHeld)
{
	auto city = MakeSessionCity();
	city.sourceCells.Clear(); // no tokens: the order stays open and the session keeps running
	city.sourceCells.Add(Vec2I(0, 0));

	GameSession session;
	session.StartWithCity(city, TestSessionTuning());

	GameInput boost;
	boost.boost = true;
	for (int i = 0; i < 60; i++)
		session.Tick(1.0f/60.0f, boost);

	EXPECT_TRUE(session.IsBoosting());
	EXPECT_LT(session.GetBoostLeft(), session.GetTuning().boostReserve - 0.9f);

	for (int i = 0; i < 600; i++)
		session.Tick(1.0f/60.0f, boost);
	EXPECT_FALSE(session.IsBoosting()); // reserve exhausted
	EXPECT_EQ(session.GetBoostLeft(), 0.0f);
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

#include "o2/Utils/Math/Mesh3DPrimitives.h"

TEST(TokenCarShading, BoxNormalsAndLightFormula)
{
	auto data = Mesh3DPrimitives::BuildBox(Vec3F(1.0f, 1.0f, 1.0f));
	ASSERT_EQ(data.normals.Count(), data.positions.Count());

	bool anyTop = false;
	for (auto& n : data.normals)
		anyTop |= n.z > 0.9f;
	EXPECT_TRUE(anyTop);

	Vec3F lightDir = Vec3F(-0.35f, -0.25f, -0.9f).Normalized();
	float d = Vec3F(0.0f, 0.0f, 1.0f).Dot(lightDir*-1.0f);
	EXPECT_GT(d, 0.8f);

	float ambient = 0.55f;
	float intensity = ambient + (1.0f - ambient)*Math::Max(d, 0.0f);
	EXPECT_GT(intensity, 0.9f);
}
