// Soft scale pulsing for the token hologram above the fountain.
// Attached via ScriptableComponent; the class name must match the file name.
class HologramPulse extends o2.Component
{
	constructor()
	{
		super();
		this._time = 0;
		this.baseScale = 0.9;
		this.amplitude = 0.08;
		this.speed = 2.0;
	}

	Update(dt)
	{
		this._time += dt;
		var s = this.baseScale + this.amplitude*Math.sin(this._time*this.speed);
		this._actor.transform.SetScale2D(new Vec2(s, s));
	}
}
