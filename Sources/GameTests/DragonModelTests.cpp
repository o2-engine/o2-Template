#include "o2/stdafx.h"

#include <gtest/gtest.h>

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Scripts/ScriptEngine.h"

using namespace o2;

#if IS_SCRIPTING_SUPPORTED

namespace
{
	ScriptValue EvalChecked(const char* code)
	{
		ScriptValue res = o2Scripts.Eval(code);
		EXPECT_NE(res.GetValueType(), ScriptValue::ValueType::Error) << res.GetError().Data();
		return res;
	}

	void EnsureModelLoaded()
	{
		static bool loaded = false;
		if (loaded)
			return;

		auto script = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/DragonDefense/DragonModel.js"));
		ASSERT_TRUE(script) << "DragonModel.js asset not found";
		o2Scripts.Run(script->Parse());

		// Isolated battle sandbox: rocks block the bottom row (no recruit spawns), an
		// indestructible passive dragon in column 6 pins wave spawning and victory checks.
		EvalChecked(
			"function dgTestModel(seed) {"
			"    var m = new DragonModel(seed === undefined ? 1 : seed);"
			"    m.state = 'battle';"
			"    m.wave = 1;"
			"    for (var c = 0; c < DD.COLS; c++)"
			"        m.grid[c][0] = { kind: 'rock', hp: 100000, maxHp: 100000, id: m._nextId++ };"
			"    m.dragons.push({ id: m._nextId++, type: 'normal', col: 6, row: 9, w: 1, h: 1,"
			"                     hp: 100000, maxHp: 100000, armor: 0, dmg: 0, breach: 0, fireCounter: 0 });"
			"    return m;"
			"}");

		loaded = true;
	}
}

#define DG_TEST_PROLOGUE() \
	EnsureModelLoaded();   \
	SCOPED_TRACE("dragon model")

TEST(DragonModel, MergePairLevelsUpAndConsumesNeighbor)
{
	DG_TEST_PROLOGUE();
	auto lvl = EvalChecked(
		"var m = dgTestModel();"
		"m.grid[2][1] = m._MakeUnit('tank', 1);"
		"m.grid[2][2] = m._MakeUnit('tank', 1);"
		"m._TryMergeAt(2, 1);"
		"m.grid[2][1].lvl");
	EXPECT_EQ(lvl.GetValue<int>(), 2);
	EXPECT_TRUE(EvalChecked("m.grid[2][2] === null").GetValue<bool>());
}

TEST(DragonModel, ClickCellMergeConsumesTurn)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[2][1] = m._MakeUnit('tank', 1);"
		"m.grid[2][2] = m._MakeUnit('tank', 1);"
		"var res = m.ClickCell(2, 1);");
	EXPECT_TRUE(EvalChecked("res.ok").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.turn").GetValue<int>(), 1);
	// after the turn gravity floats the merged unit to the top of the column
	EXPECT_TRUE(EvalChecked("m.grid[2][6] !== null && m.grid[2][6].lvl === 2").GetValue<bool>());
	// clicking a unit with no matching pair is not a turn
	EXPECT_FALSE(EvalChecked("m.ClickCell(2, 6).ok").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.turn").GetValue<int>(), 1);
}

TEST(DragonModel, MergeCascadesThroughLevels)
{
	DG_TEST_PROLOGUE();
	auto lvl = EvalChecked(
		"var m = dgTestModel();"
		"m.barracksLvl = 3;"
		"m.grid[2][1] = m._MakeUnit('tank', 1);"
		"m.grid[2][2] = m._MakeUnit('tank', 1);"
		"m.grid[3][1] = m._MakeUnit('tank', 2);"
		"m._TryMergeAt(2, 1);"
		"m.grid[2][1].lvl");
	EXPECT_EQ(lvl.GetValue<int>(), 3);
	EXPECT_TRUE(EvalChecked("m.grid[2][2] === null && m.grid[3][1] === null").GetValue<bool>());
}

TEST(DragonModel, MergeOverBarracksCapGivesBonusInsteadOfLevel)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[2][1] = m._MakeUnit('tank', 2);"
		"m.grid[2][2] = m._MakeUnit('tank', 2);"
		"m._TryMergeAt(2, 1);");
	EXPECT_EQ(EvalChecked("m.grid[2][1].lvl").GetValue<int>(), 2);
	EXPECT_NEAR(EvalChecked("m.grid[2][1].bonus").GetValue<float>(), 0.2f, 0.001f);
	EXPECT_EQ(EvalChecked("m.grid[2][1].hp").GetValue<int>(), 84); // 70 * 1.2
}

TEST(DragonModel, MergeDamagesAdjacentRock)
{
	DG_TEST_PROLOGUE();
	auto hp = EvalChecked(
		"var m = dgTestModel();"
		"m.grid[2][1] = m._MakeUnit('archer', 1);"
		"m.grid[1][1] = m._MakeUnit('archer', 1);"
		"m.grid[2][2] = { kind: 'rock', hp: 40, maxHp: 40, id: m._nextId++ };"
		"m._TryMergeAt(2, 1);"
		"m.grid[2][2].hp");
	EXPECT_EQ(hp.GetValue<int>(), 20); // 40 - 10 * new level 2
}

TEST(DragonModel, ForgeBoostsRockDamage)
{
	DG_TEST_PROLOGUE();
	auto hp = EvalChecked(
		"var m = dgTestModel();"
		"m.forgeLvl = 1;"
		"m.grid[2][1] = m._MakeUnit('archer', 1);"
		"m.grid[1][1] = m._MakeUnit('archer', 1);"
		"m.grid[2][2] = { kind: 'rock', hp: 40, maxHp: 40, id: m._nextId++ };"
		"m._TryMergeAt(2, 1);"
		"m.grid[2][2].hp");
	EXPECT_EQ(hp.GetValue<int>(), 10); // 40 - 10*2*1.5
}

TEST(DragonModel, GravityFloatsUnitsUpAndSpawnsRecruits)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[0][0] = null;" // open the spawn cell in column 0
		"m.grid[0][1] = m._MakeUnit('tank', 1);"
		"var floaterId = m.grid[0][1].id;"
		"m.DoTurn();");
	EXPECT_TRUE(EvalChecked("m.grid[0][6] !== null && m.grid[0][6].id === floaterId").GetValue<bool>());
	EXPECT_TRUE(EvalChecked("m.grid[0][0] !== null && m.grid[0][0].kind === 'unit'").GetValue<bool>());
}

TEST(DragonModel, RockBlocksGravityAndSpawn)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[1][3] = { kind: 'rock', hp: 40, maxHp: 40, id: m._nextId++ };"
		"m.grid[1][1] = m._MakeUnit('tank', 1);"
		"var unitId = m.grid[1][1].id;"
		"m.DoTurn();");
	EXPECT_TRUE(EvalChecked("m.grid[1][2] !== null && m.grid[1][2].id === unitId").GetValue<bool>());
	EXPECT_TRUE(EvalChecked("m.grid[1][3].kind === 'rock'").GetValue<bool>());
}

TEST(DragonModel, TankBlocksAndTradesWithDragon)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[3][6] = m._MakeUnit('tank', 1);"
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 3, row: 7, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 6, breach: 10, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.dragons[1].row").GetValue<int>(), 7); // blocked by the tank
	EXPECT_EQ(EvalChecked("m.dragons[1].hp").GetValue<int>(), 21); // 25 - 4
	EXPECT_EQ(EvalChecked("m.grid[3][6].hp").GetValue<int>(), 24); // 30 - 6
}

TEST(DragonModel, ArcherShootsAcrossTheColumn)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[2][1] = m._MakeUnit('archer', 1);"
		"m.grid[2][2] = { kind: 'rock', hp: 40, maxHp: 40, id: m._nextId++ };" // pin the archer low
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 2, row: 9, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 6, breach: 10, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.dragons[1].hp").GetValue<int>(), 19); // hit across 8 rows: 25 - 6
	EXPECT_EQ(EvalChecked("m.dragons[1].row").GetValue<int>(), 8); // free fall continues
}

TEST(DragonModel, ArmoredDragonReducesIncomingDamage)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[1][1] = m._MakeUnit('archer', 1);"
		"m.dragons.push({ id: m._nextId++, type: 'tankd', col: 1, row: 9, w: 1, h: 1,"
		"                 hp: 60, maxHp: 60, armor: 3, dmg: 5, breach: 15, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.dragons[1].hp").GetValue<int>(), 57); // 60 - (6-3)
}

TEST(DragonModel, RangedDragonAttacksFromSky)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[4][6] = m._MakeUnit('tank', 1);"
		"m.dragons.push({ id: m._nextId++, type: 'range', col: 4, row: 8, w: 1, h: 1,"
		"                 hp: 20, maxHp: 20, armor: 0, dmg: 7, breach: 10, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.grid[4][6].hp").GetValue<int>(), 23); // 30 - 7, hit from distance 2
	EXPECT_EQ(EvalChecked("m.dragons[1].row").GetValue<int>(), 7);
}

TEST(DragonModel, FireDragonIgnitesCellEverySecondTurn)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.dragons.push({ id: m._nextId++, type: 'fire', col: 5, row: 7, w: 1, h: 1,"
		"                 hp: 30, maxHp: 30, armor: 0, dmg: 5, breach: 12, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("Object.keys(m.fires).length").GetValue<int>(), 0);
	EvalChecked("m.DoTurn();");
	EXPECT_GE(EvalChecked("Object.keys(m.fires).length").GetValue<int>(), 1);
}

TEST(DragonModel, FireCellBurnsStandingUnit)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[2][6] = m._MakeUnit('tank', 1);" // top of the column: gravity keeps it in place
		"m.fires['2_6'] = 3;"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.grid[2][6].hp").GetValue<int>(), 26); // 30 - 4
	EXPECT_EQ(EvalChecked("m.fires['2_6']").GetValue<int>(), 2);
}

TEST(DragonModel, BossOccupies2x2AndIsBlockedByUnit)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[2][6] = m._MakeUnit('tank', 2);"
		"m.dragons.push({ id: m._nextId++, type: 'boss', col: 2, row: 7, w: 2, h: 2,"
		"                 hp: 300, maxHp: 300, armor: 2, dmg: 20, breach: 50, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.dragons[1].row").GetValue<int>(), 7);  // blocked by the tank below
	EXPECT_EQ(EvalChecked("m.grid[2][6].hp").GetValue<int>(), 50);  // 70 - 20
	EXPECT_EQ(EvalChecked("m.dragons[1].hp").GetValue<int>(), 293); // 300 - (9-2)
}

TEST(DragonModel, DragonBreachDamagesCastle)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[5][0] = null;" // clear the sandbox rock so the dragon can reach the bottom
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 5, row: 0, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 6, breach: 10, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.castleHP").GetValue<int>(), 90);
	EXPECT_EQ(EvalChecked("m.dragons.length").GetValue<int>(), 1); // only the anchor remains
}

TEST(DragonModel, BombHitsAreaAndKeepsTurn)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 1, row: 8, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 6, breach: 10, fireCounter: 0 });"
		"var res = m.UseBomb(1, 8);");
	EXPECT_TRUE(EvalChecked("res.ok").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.dragons.length").GetValue<int>(), 1); // dragon died, anchor left
	EXPECT_EQ(EvalChecked("m.boosters.bomb").GetValue<int>(), 1);
	EXPECT_EQ(EvalChecked("m.turn").GetValue<int>(), 0); // boosters do not consume the turn
}

TEST(DragonModel, FreezeStopsDragonsForTwoTurns)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 3, row: 9, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 6, breach: 10, fireCounter: 0 });"
		"m.UseFreeze();"
		"m.DoTurn(); m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.dragons[1].row").GetValue<int>(), 9); // frozen for both turns
	EvalChecked("m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.dragons[1].row").GetValue<int>(), 8);
}

TEST(DragonModel, SwapExchangesUnits)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[1][1] = m._MakeUnit('tank', 1);"
		"m.grid[5][3] = m._MakeUnit('mage', 2);"
		"m.UseSwap(1, 1, 5, 3);");
	EXPECT_TRUE(EvalChecked("m.grid[1][1].cls === 'mage' && m.grid[5][3].cls === 'tank'").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.boosters.swap").GetValue<int>(), 1);
}

TEST(DragonModel, SupportHealsAndBuffsNeighbours)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[2][5] = m._MakeUnit('support', 1);"
		"m.grid[2][6] = m._MakeUnit('tank', 1);"
		"m.grid[2][6].hp = 10;"
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 2, row: 7, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 0, breach: 10, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.grid[2][6].hp").GetValue<int>(), 13); // 10 + 3 heal
	EXPECT_EQ(EvalChecked("m.dragons[1].hp").GetValue<int>(), 20); // 25 - round(4 * 1.15)
}

TEST(DragonModel, MageSplashesNeighbours)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.grid[3][5] = m._MakeUnit('mage', 1);"
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 3, row: 8, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 0, breach: 10, fireCounter: 0 });"
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 4, row: 8, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 0, breach: 10, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_EQ(EvalChecked("m.dragons[1].hp").GetValue<int>(), 20); // direct 5
	EXPECT_EQ(EvalChecked("m.dragons[2].hp").GetValue<int>(), 22); // splash round(2.5)
}

TEST(DragonModel, BuyUnitSpendsCoinsAndTakesTurn)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"var res = m.BuyUnit('mage');");
	EXPECT_TRUE(EvalChecked("res.ok").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.coins").GetValue<int>(), 50);
	EXPECT_EQ(EvalChecked("m.turn").GetValue<int>(), 1);
	// the unit was created level 2 (barracks 2); gravity may have floated it up the column
	EXPECT_GE(EvalChecked(
		"var found = 0;"
		"for (var c = 0; c < DD.COLS; c++)"
		"    for (var r = 0; r < DD.ROWS; r++)"
		"        if (m.IsUnit(m.grid[c][r]) && m.grid[c][r].cls === 'mage' && m.grid[c][r].lvl === 2) found++;"
		"found").GetValue<int>(), 1);
}

TEST(DragonModel, VictoryPaysReward)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = new DragonModel(3);"
		"m.state = 'battle';"
		"m.wave = DD.WAVES.length;"
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 0, row: 9, w: 1, h: 1,"
		"                 hp: 10, maxHp: 10, armor: 0, dmg: 6, breach: 10, fireCounter: 0 });"
		"var coinsBefore = m.coins;"
		"m.UseBomb(0, 9);");
	EXPECT_TRUE(EvalChecked("m.state === 'victory'").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.coins - coinsBefore").GetValue<int>(), 350); // 150 + 20*10
	EXPECT_EQ(EvalChecked("m.materials").GetValue<int>(), 30);
}

TEST(DragonModel, DefeatWhenCastleFalls)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = dgTestModel();"
		"m.castleHP = 5;"
		"m.grid[4][0] = null;"
		"m.dragons.push({ id: m._nextId++, type: 'normal', col: 4, row: 0, w: 1, h: 1,"
		"                 hp: 25, maxHp: 25, armor: 0, dmg: 6, breach: 10, fireCounter: 0 });"
		"m.DoTurn();");
	EXPECT_TRUE(EvalChecked("m.state === 'defeat'").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.castleHP").GetValue<int>(), 0);
}

TEST(DragonModel, StartBattleBuildsField)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = new DragonModel(11);"
		"m.StartBattle();");
	EXPECT_TRUE(EvalChecked("m.state === 'battle'").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.wave").GetValue<int>(), 1);
	EXPECT_EQ(EvalChecked("m.dragons.length").GetValue<int>(), 2);
	EXPECT_EQ(EvalChecked(
		"var rocks = 0;"
		"for (var c = 0; c < DD.COLS; c++)"
		"    for (var r = 0; r < DD.ROWS; r++)"
		"        if (m.IsRock(m.grid[c][r])) rocks++;"
		"rocks").GetValue<int>(), 9);
	EXPECT_GE(EvalChecked(
		"var units = 0;"
		"for (var c = 0; c < DD.COLS; c++)"
		"    for (var r = 0; r < DD.ROWS; r++)"
		"        if (m.IsUnit(m.grid[c][r])) units++;"
		"units").GetValue<int>(), 19);
}

TEST(DragonModel, FullBattleSimulationFinishes)
{
	DG_TEST_PROLOGUE();
	auto state = EvalChecked(
		"var m = new DragonModel(99);"
		"m.coins = 1000;"
		"m.StartBattle();"
		"var guard = 0;"
		"while (m.state === 'battle' && guard++ < 400) {"
		"    var acted = false;"
		"    for (var r = 0; r < 7 && !acted; r++)"
		"        for (var c = 0; c < 7 && !acted; c++) {"
		"            var u = m.grid[c][r];"
		"            if (u !== null && u.kind === 'unit' && m._MergeNeighbor(c, r, u.cls, u.lvl) !== null)"
		"                acted = m.ClickCell(c, r).ok;"
		"        }"
		"    if (!acted && m.coins >= 100)"
		"        acted = m.BuyUnit('tank').ok;"
		"    if (!acted)"
		"        m.DoTurn();"
		"}"
		"m.state");
	String result = state.GetValue<String>();
	EXPECT_TRUE(result == "victory" || result == "defeat") << "state: " << result.Data();
	EXPECT_LE(EvalChecked("m.turn").GetValue<int>(), 400);
}

TEST(DragonModel, TuningAppliesToBattleStart)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = new DragonModel(21);"
		"m.SetTuning('castleHP', 50);"
		"m.SetTuning('rockCount', 3);"
		"m.SetTuning('maxWaves', 1);"
		"m.SetTuning('bombCharges', 5);"
		"m.StartBattle();");
	EXPECT_EQ(EvalChecked("m.castleHP").GetValue<int>(), 50);
	EXPECT_EQ(EvalChecked("m.maxWaves").GetValue<int>(), 1);
	EXPECT_EQ(EvalChecked("m.boosters.bomb").GetValue<int>(), 5);
	EXPECT_EQ(EvalChecked(
		"var rocks = 0;"
		"for (var c = 0; c < DD.COLS; c++)"
		"    for (var r = 0; r < DD.ROWS; r++)"
		"        if (m.IsRock(m.grid[c][r])) rocks++;"
		"rocks").GetValue<int>(), 3);

	// the only wave is out: clearing it ends the battle with a victory
	EvalChecked("m.dragons = []; m._CheckBattleEnd();");
	EXPECT_TRUE(EvalChecked("m.state === 'victory'").GetValue<bool>());
}

TEST(DragonModel, DisabledUnitClassesNotSpawnedAndNotBuyable)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = new DragonModel(22);"
		"m.enabledUnits = { tank: true, archer: false, mage: false, support: false };"
		"m.StartBattle();");
	EXPECT_EQ(EvalChecked(
		"var bad = 0;"
		"for (var c = 0; c < DD.COLS; c++)"
		"    for (var r = 0; r < DD.ROWS; r++)"
		"        if (m.IsUnit(m.grid[c][r]) && m.grid[c][r].cls !== 'tank') bad++;"
		"bad").GetValue<int>(), 0);
	EXPECT_FALSE(EvalChecked("m.BuyUnit('mage').ok").GetValue<bool>());
	EXPECT_TRUE(EvalChecked("m.BuyUnit('tank').ok").GetValue<bool>());
}

TEST(DragonModel, DisabledDragonTypeIsReplacedInWaves)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = new DragonModel(23);"
		"m.ToggleDragonType('normal');" // wave 1 consists of normals -> replaced by tankd
		"m.StartBattle();");
	EXPECT_EQ(EvalChecked("m.dragons.length").GetValue<int>(), 2);
	EXPECT_EQ(EvalChecked("m.dragons.filter(function(d) { return d.type === 'tankd'; }).length").GetValue<int>(), 2);
}

TEST(DragonModel, AllDragonsDisabledLeadsToVictory)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = new DragonModel(24);"
		"m.enabledDragons = { normal: false, tankd: false, range: false, fire: false, boss: false };"
		"m.SetTuning('maxWaves', 1);"
		"m.StartBattle();");
	EXPECT_EQ(EvalChecked("m.dragons.length").GetValue<int>(), 0);
	EvalChecked("m.DoTurn();");
	EXPECT_TRUE(EvalChecked("m.state === 'victory'").GetValue<bool>());
}

TEST(DragonModel, UpgradesSpendResources)
{
	DG_TEST_PROLOGUE();
	EvalChecked(
		"var m = new DragonModel(5);"
		"m.coins = 700; m.materials = 200;"
		"var okBarracks = m.UpgradeBarracks();" // 2 -> 3: 500 coins, 150 materials
		"var okForge = m.UpgradeForge();");     // forge to level 1: 150 coins, 30 materials
	EXPECT_TRUE(EvalChecked("okBarracks").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.barracksLvl").GetValue<int>(), 3);
	EXPECT_TRUE(EvalChecked("okForge").GetValue<bool>());
	EXPECT_EQ(EvalChecked("m.forgeLvl").GetValue<int>(), 1);
	EXPECT_EQ(EvalChecked("m.coins").GetValue<int>(), 50);      // 700 - 500 - 150
	EXPECT_EQ(EvalChecked("m.materials").GetValue<int>(), 20);  // 200 - 150 - 30
	EXPECT_FALSE(EvalChecked("m.UpgradeForge()").GetValue<bool>()); // 400 coins needed
}

#endif // IS_SCRIPTING_SUPPORTED
