#pragma once

#include "TokenDelivery/GameSession.h"

namespace td
{
	// Overrides the default tuning with values from Assets/Scripts/GameConfig.js when the
	// script asset is available; silently keeps defaults otherwise (e.g. headless tests)
	SessionTuning LoadTuningFromJS();
}
