#include "o2/stdafx.h"

#include <gtest/gtest.h>

#include "DragonDefense/DragonDefenseBootstrap.h"
#include "DragonDefense/DragonInputComponent.h"
#include "DragonDefense/DragonLayout.h"
#include "o2/Application/Application.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Scene.h"
#include "o2/Scripts/ScriptEngine.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

#if IS_SCRIPTING_SUPPORTED

namespace
{
	const String kShotsDir = "../../Work/ScreenShots/";

	ScriptValue EvalChecked(const char* code)
	{
		ScriptValue res = o2Scripts.Eval(code);
		EXPECT_NE(res.GetValueType(), ScriptValue::ValueType::Error) << res.GetError().Data();
		return res;
	}
}

class DragonDefenseUI: public ::testing::Test
{
protected:
	void SetUp() override
	{
		o2Application.SetWindowSize(Vec2I(1280, 800));
		AppTestDriver::PumpFrames(2);

		o2Scripts.Eval("DRAGON_SEED = 7;"); // deterministic battles in tests
		DragonDefenseBootstrap::CreateBootstrapActor();
		AppTestDriver::PumpFrames(5);
	}

	void TearDown() override
	{
		o2Scripts.Eval("DRAGON_SEED = undefined; DRAGON_GAME = undefined;");
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}

	void ClickWorld(const Vec2F& world)
	{
		AppTestDriver::Click(DragonInputComponent::WorldToScreen(world));
		AppTestDriver::PumpFrames(3);
	}

	void StartBattle()
	{
		ClickWorld(dragon::kToBattleBtnPos);
		ASSERT_TRUE(EvalChecked("DRAGON_GAME._model.state === 'battle'").GetValue<bool>());
	}

	// Returns a cell "c_r" holding a unit with a mergeable neighbour, or "" if none
	String FindMergeableCell()
	{
		return EvalChecked(
			"(function() {"
			"    var m = DRAGON_GAME._model;"
			"    for (var r = 0; r < 7; r++)"
			"        for (var c = 0; c < 7; c++) {"
			"            var u = m.grid[c][r];"
			"            if (u !== null && u.kind === 'unit' && u.lvl < m.barracksLvl &&"
			"                m._MergeNeighbor(c, r, u.cls, u.lvl) !== null)"
			"                return c + '_' + r;"
			"        }"
			"    return '';"
			"})()").GetValue<String>();
	}

	bool ClickMergeableCell()
	{
		String cell = FindMergeableCell();
		if (cell.IsEmpty())
			return false;

		auto parts = cell.Split("_");
		ClickWorld(dragon::CellPos((int)parts[0], (int)parts[1]));
		return true;
	}
};

TEST_F(DragonDefenseUI, BuildsBothScreensAndStartsInCastle)
{
	auto battle = o2Scene.FindActor("DragonDefense/Battle");
	auto castle = o2Scene.FindActor("DragonDefense/Castle");
	ASSERT_TRUE(battle);
	ASSERT_TRUE(castle);
	EXPECT_TRUE(o2Scene.FindActor("DragonDefense/Game"));
	EXPECT_TRUE(o2Scene.FindActor("ui camera"));

	EXPECT_TRUE(EvalChecked("DRAGON_GAME !== undefined && DRAGON_GAME._model.state === 'castle'").GetValue<bool>());
	EXPECT_FALSE(battle->IsEnabled());
	EXPECT_TRUE(castle->IsEnabled());

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kShotsDir + "10_castle_screen.png"));
}

TEST_F(DragonDefenseUI, ToBattleClickStartsBattle)
{
	StartBattle();

	EXPECT_TRUE(o2Scene.FindActor("DragonDefense/Battle")->IsEnabled());
	EXPECT_FALSE(o2Scene.FindActor("DragonDefense/Castle")->IsEnabled());
	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.dragons.length").GetValue<int>(), 2);
	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.wave").GetValue<int>(), 1);
	EXPECT_GE(EvalChecked("Object.keys(DRAGON_GAME._views).length").GetValue<int>(), 20);

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kShotsDir + "11_battle_start.png"));
}

TEST_F(DragonDefenseUI, ClickMergesUnitsAndAdvancesTurn)
{
	StartBattle();

	ASSERT_TRUE(ClickMergeableCell());
	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.turn").GetValue<int>(), 1);

	AppTestDriver::Wait(0.5f); // let the tweens settle
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kShotsDir + "12_battle_merge.png"));
}

TEST_F(DragonDefenseUI, BombBoosterDamagesDragons)
{
	StartBattle();

	int dragonsBefore = EvalChecked("DRAGON_GAME._model.dragons.length").GetValue<int>();
	auto dragonCell = EvalChecked(
		"var d = DRAGON_GAME._model.dragons[0]; d.col + '_' + d.row");
	auto parts = dragonCell.GetValue<String>().Split("_");

	ClickWorld(dragon::BoosterBtnPos(0)); // arm the bomb

	// hovering the field shows the 3x3 aim highlight
	AppTestDriver::MoveCursor(DragonInputComponent::WorldToScreen(dragon::CellPos((int)parts[0], (int)parts[1])));
	AppTestDriver::PumpFrames(3);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kShotsDir + "14_bomb_aim.png"));

	ClickWorld(dragon::CellPos((int)parts[0], (int)parts[1]));

	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.boosters.bomb").GetValue<int>(), 1);
	EXPECT_LT(EvalChecked("DRAGON_GAME._model.dragons.length").GetValue<int>(), dragonsBefore);
	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.turn").GetValue<int>(), 0); // boosters keep the turn
}

TEST_F(DragonDefenseUI, BalanceScreenAdjustsAndToggles)
{
	ClickWorld(dragon::kBalanceBtnPos);
	EXPECT_TRUE(EvalChecked("DRAGON_GAME._settingsOpen === true").GetValue<bool>());
	EXPECT_TRUE(o2Scene.FindActor("DragonDefense/Settings")->IsEnabled());
	EXPECT_FALSE(o2Scene.FindActor("DragonDefense/Castle")->IsEnabled());

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kShotsDir + "15_settings.png"));

	// row 0 = castle HP (step 10), row 1 = waves count (step 1)
	ClickWorld(dragon::SettingsParamRowPos(0) + dragon::kParamPlusOffset);
	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.tuning.castleHP").GetValue<int>(), 110);
	ClickWorld(dragon::SettingsParamRowPos(1) + dragon::kParamMinusOffset);
	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.tuning.maxWaves").GetValue<int>(), 9);

	ClickWorld(dragon::UnitTogglePos(0));
	EXPECT_FALSE(EvalChecked("DRAGON_GAME._model.enabledUnits.tank").GetValue<bool>());
	ClickWorld(dragon::DragonTogglePos(4));
	EXPECT_FALSE(EvalChecked("DRAGON_GAME._model.enabledDragons.boss").GetValue<bool>());

	ClickWorld(dragon::kSettingsBackBtnPos);
	EXPECT_FALSE(EvalChecked("DRAGON_GAME._settingsOpen === true").GetValue<bool>());
	EXPECT_TRUE(o2Scene.FindActor("DragonDefense/Castle")->IsEnabled());

	// the tuned value drives the next battle
	ClickWorld(dragon::kToBattleBtnPos);
	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.castleHP").GetValue<int>(), 110);
	EXPECT_EQ(EvalChecked("DRAGON_GAME._model.maxWaves").GetValue<int>(), 9);
}

TEST_F(DragonDefenseUI, PlaysSeveralTurns)
{
	StartBattle();

	int actions = 0;
	for (int i = 0; i < 6; i++)
	{
		if (!ClickMergeableCell())
			break;
		actions++;
	}
	EXPECT_GE(actions, 3);
	EXPECT_GE(EvalChecked("DRAGON_GAME._model.turn").GetValue<int>(), 3);

	AppTestDriver::Wait(0.6f);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kShotsDir + "13_battle_progress.png"));
}

#endif // IS_SCRIPTING_SUPPORTED
