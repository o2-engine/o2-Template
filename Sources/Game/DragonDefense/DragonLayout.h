#pragma once

#include "o2/Utils/Math/Vector2.h"

namespace dragon
{
	// World space matches screen pixels: origin at screen center, y up, fitted to 1280x800
	inline const o2::Vec2F kScreenSize(1280.0f, 800.0f);

	constexpr float kCell = 80.0f;
	constexpr int kCols = 7;
	constexpr int kRows = 10;
	constexpr int kSkyRows = 3;
	constexpr int kDefenseRows = 7;

	// The grid matches the engraved floor grid baked into battle_bg.png (bake_grid.py).
	// Keep these formulas in sync with DG_LAYOUT in DragonGame.js and the bake script.
	constexpr float kFieldBottom = -256.0f;
	constexpr float kRowBaseHeight = 56.0f;

	inline float RowXScale(int row) { return 1.0f - 0.024f * row; }
	inline float RowHeight(int row) { return kRowBaseHeight - 1.0f * row; }

	inline float RowBottom(int row)
	{
		return kFieldBottom + kRowBaseHeight * row - 0.5f * row * (row - 1);
	}

	inline o2::Vec2F CellPos(int col, int row)
	{
		return o2::Vec2F((col - 3) * kCell * RowXScale(row), RowBottom(row) + RowHeight(row) * 0.5f);
	}

	inline const o2::Vec2F kFrontLinePos(0.0f, 115.0f);
	inline const o2::Vec2F kFrontLineSize(466.0f, 8.0f);

	// battle HUD
	inline const o2::Vec2F kNextWaveBtnPos(532.0f, 364.0f);
	inline const o2::Vec2F kNextWaveBtnSize(170.0f, 58.0f);

	inline const o2::Vec2F kBuyBtnSize(52.0f, 52.0f);
	inline o2::Vec2F BuyBtnPos(int idx) { return o2::Vec2F(-470.0f + idx * 64.0f, -364.0f); }

	inline const o2::Vec2F kBoosterBtnSize(52.0f, 52.0f);
	inline o2::Vec2F BoosterBtnPos(int idx) { return o2::Vec2F(310.0f + idx * 64.0f, -364.0f); }

	// castle screen
	inline const o2::Vec2F kBarracksBtnPos(-340.0f, -240.0f);
	inline const o2::Vec2F kForgeBtnPos(340.0f, -240.0f);
	inline const o2::Vec2F kUpgradeBtnSize(250.0f, 60.0f);
	inline const o2::Vec2F kToBattleBtnPos(0.0f, -322.0f);
	inline const o2::Vec2F kToBattleBtnSize(260.0f, 88.0f);
	inline const o2::Vec2F kBalanceBtnPos(540.0f, 356.0f);
	inline const o2::Vec2F kBalanceBtnSize(180.0f, 58.0f);

	// balance settings screen: two columns of parameter rows, then type toggles
	constexpr int kSettingsParamRows = 13;
	constexpr int kSettingsParamsPerColumn = 7;

	inline o2::Vec2F SettingsParamRowPos(int idx)
	{
		int column = idx / kSettingsParamsPerColumn;
		int row = idx % kSettingsParamsPerColumn;
		return o2::Vec2F(column == 0 ? -290.0f : 290.0f, 240.0f - row * 52.0f);
	}

	inline const o2::Vec2F kParamMinusOffset(15.0f, 0.0f);
	inline const o2::Vec2F kParamPlusOffset(145.0f, 0.0f);
	inline const o2::Vec2F kParamBtnSize(36.0f, 36.0f);

	inline o2::Vec2F UnitTogglePos(int idx) { return o2::Vec2F(-390.0f + idx * 60.0f, -152.0f); }
	inline o2::Vec2F DragonTogglePos(int idx) { return o2::Vec2F(-390.0f + idx * 60.0f, -222.0f); }
	inline const o2::Vec2F kToggleBtnSize(48.0f, 48.0f);

	inline const o2::Vec2F kSettingsBackBtnPos(0.0f, -320.0f);
	inline const o2::Vec2F kSettingsBackBtnSize(220.0f, 64.0f);
}
