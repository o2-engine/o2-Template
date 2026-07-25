// Token Delivery balance config. Read by GameControllerComponent on level start;
// tweak values here without recompiling.
GameConfig = {
	// car
	maxSpeed: 3.0,        // cells per second
	accel: 2.5,           // cells per second^2
	turnSpeedLoss: 0.18,  // fraction of speed lost on a turn
	boostFactor: 1.5,     // speed cap multiplier while boosting
	driftTime: 0.4,       // seconds of the sideways slide after a turn

	// session
	fuelTime: 60.0,       // seconds of fuel
	boostReserve: 5.0,    // seconds of boost hold
	fillRate: 60.0,       // tokens per second near the fountain
	baseOrders: 3,        // orders on level 1
	maxOrders: 8,
	baseCitySize: 15,     // city grid side on level 1
	maxCitySize: 19
};
