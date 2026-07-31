// Turn decisions for a traffic car: every few seconds rolls a chance to turn left or
// right at the next crossroad. TrafficCarComponent consumes the pulsed _turnLeft/_turnRight
// flags and feeds them into the car simulation for one tick.
TrafficAI = class TrafficAI extends o2.Component
{
	constructor()
	{
		super();
		this._timer = 1.0 + Math.random()*2.0;
		this._turnLeft = false;
		this._turnRight = false;
		this.decisionMin = 1.5;  // seconds between decisions, lower bound
		this.decisionMax = 4.0;  // seconds between decisions, upper bound
		this.turnChance = 0.4;   // probability that a decision turns at all, split evenly
	}

	Update(dt)
	{
		this._timer -= dt;
		if (this._timer > 0)
			return;

		this._timer = this.decisionMin + Math.random()*(this.decisionMax - this.decisionMin);
		var roll = Math.random();
		this._turnLeft = roll < this.turnChance*0.5;
		this._turnRight = !this._turnLeft && roll < this.turnChance;
	}
}
