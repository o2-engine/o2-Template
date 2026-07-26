// Smooth follow for the world camera: exponentially approaches the target point fed by
// the game controller every frame (_targetX/_targetY), clamped to the city bounds
// (_minX/_maxX/_minY/_maxY, set once per level).
CameraFollow = class CameraFollow extends o2.Component
{
	constructor()
	{
		super();
		this.speed = 4.0; // exponential approach rate, higher is snappier
		this._targetX = 0;
		this._targetY = 0;
		this._minX = 0;
		this._maxX = 0;
		this._minY = 0;
		this._maxY = 0;
	}

	Update(dt)
	{
		var tx = Math.min(Math.max(this._targetX, this._minX), this._maxX);
		var ty = Math.min(Math.max(this._targetY, this._minY), this._maxY);

		var transform = this._actor.GetTransform();
		var pos = transform.GetPosition2D();
		var k = Math.min(1.0, dt*this.speed);
		transform.SetPosition2D(new Vec2(pos.x + (tx - pos.x)*k, pos.y + (ty - pos.y)*k));
	}
}
