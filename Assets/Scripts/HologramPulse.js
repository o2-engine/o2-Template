// Soft scale pulsing for the token hologram above the fountain.
// Attached via ScriptableComponent; the class must be a global own property named
// after the file (a top-level `class X {}` is a lexical binding and is not found).
HologramPulse = class HologramPulse extends o2.Component
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
		this._actor.GetTransform().SetScale2D(new Vec2(s, s));
	}
}
