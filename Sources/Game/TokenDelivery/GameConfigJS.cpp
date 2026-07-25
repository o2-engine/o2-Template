#include "o2/stdafx.h"
#include "TokenDelivery/GameConfigJS.h"

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/JavaScriptAsset.h"
#include "o2/Scripts/ScriptEngine.h"

namespace td
{
	static void ReadFloat(const ScriptValue& config, const char* name, float& target)
	{
		auto value = config.GetProperty(name);
		if (value.GetValueType() == ScriptValue::ValueType::Number)
			target = value.GetValue<float>();
	}

	static void ReadInt(const ScriptValue& config, const char* name, int& target)
	{
		auto value = config.GetProperty(name);
		if (value.GetValueType() == ScriptValue::ValueType::Number)
			target = value.GetValue<int>();
	}

	SessionTuning LoadTuningFromJS()
	{
		SessionTuning tuning;

		auto script = o2Assets.GetAssetRefByType<JavaScriptAsset>(String("Scripts/GameConfig.js"));
		if (!script)
			return tuning;

		script->Run();
		auto config = o2Scripts.GetGlobal().GetProperty("GameConfig");
		if (!config.IsObject())
			return tuning;

		ReadFloat(config, "maxSpeed", tuning.car.maxSpeed);
		ReadFloat(config, "accel", tuning.car.accel);
		ReadFloat(config, "turnSpeedLoss", tuning.car.turnSpeedLoss);
		ReadFloat(config, "boostFactor", tuning.car.boostFactor);
		ReadFloat(config, "driftTime", tuning.car.driftTime);
		ReadFloat(config, "fuelTime", tuning.fuelTime);
		ReadFloat(config, "boostReserve", tuning.boostReserve);
		ReadFloat(config, "fillRate", tuning.fillRate);
		ReadInt(config, "baseOrders", tuning.baseOrders);
		ReadInt(config, "maxOrders", tuning.maxOrders);
		ReadInt(config, "baseCitySize", tuning.baseCitySize);
		ReadInt(config, "maxCitySize", tuning.maxCitySize);
		return tuning;
	}
}
