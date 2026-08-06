// Dragon Defense: Merge & Blast — view/controller. Owns a DragonModel, builds sprite
// actors for model entities, wires UI Button widgets, renders combat/merge effects,
// move hints and booster aim highlights. Field cells are clicked through
// DragonInputComponent (world position -> perspective cell).
// DragonModel.js is executed by the bootstrap before this component is created.

DG_SPRITES = {
    unit_tank_1:    { p: "DragonDefense/Sprites/unit_tank_1.png",    w: 108, h: 128 },
    unit_tank_2:    { p: "DragonDefense/Sprites/unit_tank_2.png",    w: 84,  h: 128 },
    unit_tank_3:    { p: "DragonDefense/Sprites/unit_tank_3.png",    w: 121, h: 128 },
    unit_archer_1:  { p: "DragonDefense/Sprites/unit_archer_1.png",  w: 118, h: 128 },
    unit_archer_2:  { p: "DragonDefense/Sprites/unit_archer_2.png",  w: 91,  h: 128 },
    unit_archer_3:  { p: "DragonDefense/Sprites/unit_archer_3.png",  w: 105, h: 128 },
    unit_mage_1:    { p: "DragonDefense/Sprites/unit_mage_1.png",    w: 84,  h: 128 },
    unit_mage_2:    { p: "DragonDefense/Sprites/unit_mage_2.png",    w: 94,  h: 128 },
    unit_mage_3:    { p: "DragonDefense/Sprites/unit_mage_3.png",    w: 102, h: 128 },
    unit_support_1: { p: "DragonDefense/Sprites/unit_support_1.png", w: 57,  h: 128 },
    unit_support_2: { p: "DragonDefense/Sprites/unit_support_2.png", w: 111, h: 128 },
    unit_support_3: { p: "DragonDefense/Sprites/unit_support_3.png", w: 113, h: 128 },
    dragon_normal:  { p: "DragonDefense/Sprites/dragon_normal.png",  w: 192, h: 184 },
    dragon_tankd:   { p: "DragonDefense/Sprites/dragon_tank.png",    w: 192, h: 181 },
    dragon_range:   { p: "DragonDefense/Sprites/dragon_range.png",   w: 192, h: 191 },
    dragon_fire:    { p: "DragonDefense/Sprites/dragon_fire.png",    w: 192, h: 189 },
    dragon_boss:    { p: "DragonDefense/Sprites/dragon_boss.png",    w: 384, h: 363 },
    rock:           { p: "DragonDefense/Sprites/rock.png",           w: 128, h: 116 },
    fire:           { p: "DragonDefense/Sprites/fire.png",           w: 96,  h: 128 },
    bar:            { p: "DragonDefense/Sprites/bar.png",            w: 64,  h: 14 },
    glow:           { p: "DragonDefense/Sprites/cell_glow.png",      w: 96,  h: 96 },
    spark:          { p: "DragonDefense/Sprites/spark.png",          w: 64,  h: 64 },
    comet:          { p: "DragonDefense/Sprites/comet.png",          w: 96,  h: 32 }
};

// Grid matching the engraved floor baked into battle_bg.png;
// keep in sync with DragonLayout.h and bake_grid.py
DG_LAYOUT = {
    xScale: function(row) { return 1 - 0.024 * row; },
    rowH: function(row) { return 56 - row; },
    rowBottom: function(row) { return -256 + 56 * row - 0.5 * row * (row - 1); },
    cellX: function(col, row) { return (col - 3) * 80 * this.xScale(row); },
    cellY: function(row) { return this.rowBottom(row) + this.rowH(row) / 2; },
    unitScale: function(row) { return 0.5 * (1 + this.xScale(row)); },
    rowAt: function(y) {
        if (y < this.rowBottom(0))
            return -1;
        for (var r = 0; r < 10; r++)
            if (y < this.rowBottom(r + 1))
                return r;
        return -1;
    },
    colAt: function(x, row) { return Math.floor(x / (80 * this.xScale(row)) + 3.5); }
};

// balance settings screen rows; keys refer to model.tuning
DG_PARAMS = [
    { key: "castleHP",      label: "HP замка",          min: 20, max: 500, step: 10 },
    { key: "maxWaves",      label: "Волн в бою",        min: 1,  max: 10,  step: 1 },
    { key: "rockCount",     label: "Камней на поле",    min: 0,  max: 20,  step: 1 },
    { key: "rockHp",        label: "HP камня",          min: 10, max: 200, step: 10 },
    { key: "startUnitRows", label: "Стартовые ряды",    min: 1,  max: 6,   step: 1 },
    { key: "buyCost",       label: "Цена юнита",        min: 20, max: 500, step: 20 },
    { key: "buyLevel",      label: "Уровень покупки",   min: 1,  max: 3,   step: 1 },
    { key: "bombCharges",   label: "Заряды бомбы",      min: 0,  max: 9,   step: 1 },
    { key: "freezeCharges", label: "Заряды заморозки",  min: 0,  max: 9,   step: 1 },
    { key: "swapCharges",   label: "Заряды рокировки",  min: 0,  max: 9,   step: 1 },
    { key: "bombDmg",       label: "Урон бомбы",        min: 10, max: 300, step: 10 },
    { key: "freezeTurns",   label: "Ходы заморозки",    min: 1,  max: 5,   step: 1 },
    { key: "fireDmg",       label: "Урон огня",         min: 1,  max: 20,  step: 1 }
];

DragonGame = class DragonGame extends o2.Component
{
    constructor()
    {
        super();
        this.seed = 12345;      // serialized, overridable from the editor
        this._model = null;
        this._input = null;
        this._battle = null;
        this._castle = null;
        this._hud = {};
        this._views = {};       // "u<id>"/"d<id>"/"r<id>"/"f<c>_<r>" -> view entry
        this._fx = [];          // fading sprites
        this._flights = [];     // sprites flying from -> to (merge, projectiles)
        this._flashes = [];     // damage tint flashes on entity views
        this._hintGlows = [];   // merge move hints pool
        this._aimGlows = [];    // booster aim highlight pool (9 for bomb 3x3 + 2 for swap)
        this._pendingPulse = {};
        this._mode = "normal";  // normal | bomb | swapA | swapB
        this._swapFirst = null;
        this._settingsOpen = false;
        this._settingsUi = null;
        this._time = 0;
        this._fxSeed = 777;     // visual-only RNG: never touch the model's seed
        this._started = false;
    }

    FxRand(n)
    {
        this._fxSeed = (this._fxSeed * 1103515245 + 12345) & 0x7fffffff;
        return ((this._fxSeed >> 16) & 0x7fff) % n;
    }

    OnStart()
    {
        var seed = (typeof DRAGON_SEED !== "undefined") ? DRAGON_SEED : this.seed;
        this._model = new DragonModel(seed);
        this._input = this._actor.GetComponent("DragonInputComponent");
        this._battle = this._actor.GetChild("../Battle");
        this._castle = this._actor.GetChild("../Castle");
        this._settings = this._actor.GetChild("../Settings");

        this._hud = {
            castleHp: this._actor.GetChild("../Battle/Hud/CastleHpLabel"),
            hpBar: this.MakeBarRef("../Battle/Hud/HpBarFill", -410, 338, 360, 12),
            fortWave: this._actor.GetChild("../Battle/Hud/FortWaveLabel"),
            waveBar: this.MakeBarRef("../Battle/Hud/WaveBarFill", 15, 338, 360, 12),
            message: this._actor.GetChild("../Battle/Hud/Message"),
            messageShadow: this._actor.GetChild("../Battle/Hud/MessageShadow"),
            coins: this._actor.GetChild("../Battle/BuyPanel/CoinsLabel"),
            charges: [
                this._actor.GetChild("../Battle/BoosterPanel/Charge0"),
                this._actor.GetChild("../Battle/BoosterPanel/Charge1"),
                this._actor.GetChild("../Battle/BoosterPanel/Charge2")
            ],
            currency: this._actor.GetChild("../Castle/CurrencyLabel"),
            barracksInfo: this._actor.GetChild("../Castle/BarracksInfo"),
            barracksBtnText: this._actor.GetChild("../Castle/BarracksBtn").GetLayer("caption").drawable,
            forgeInfo: this._actor.GetChild("../Castle/ForgeInfo"),
            forgeBtnText: this._actor.GetChild("../Castle/ForgeBtn").GetLayer("caption").drawable
        };

        this.WireButtons();
        this.WireSettingsScreen();

        this._started = true;
        this.ShowScreens();
        this.UpdateHud();
        this.UpdateSettingsUi();

        globalThis.DRAGON_GAME = this; // test hook
    }

    WireSettingsScreen()
    {
        var self = this;
        var ui = { names: [], values: [], unitToggles: [], dragonToggles: [] };

        for (var i = 0; i < DG_PARAMS.length; i++)
        {
            (function(idx) {
                ui.names.push(self._settings.GetChild("Params/ParamName" + idx));
                ui.values.push(self._settings.GetChild("Params/ParamValue" + idx));
                self._settings.GetChild("Params/ParamMinus" + idx).onClick = function() { self.AdjustParam(idx, -1); };
                self._settings.GetChild("Params/ParamPlus" + idx).onClick = function() { self.AdjustParam(idx, 1); };
            })(i);
            ui.names[i].SetText(DG_PARAMS[i].label);
        }

        for (var i = 0; i < DD.UNIT_CLASSES.length; i++)
        {
            (function(idx) {
                var btn = self._settings.GetChild("Toggles/UnitToggle" + idx);
                ui.unitToggles.push(btn.GetLayer("icon").drawable);
                btn.onClick = function() {
                    self._model.ToggleUnitType(DD.UNIT_CLASSES[idx]);
                    self.UpdateSettingsUi();
                };
            })(i);
        }

        var dragonTypes = ["normal", "tankd", "range", "fire", "boss"];
        for (var i = 0; i < dragonTypes.length; i++)
        {
            (function(idx) {
                var btn = self._settings.GetChild("Toggles/DragonToggle" + idx);
                ui.dragonToggles.push(btn.GetLayer("icon").drawable);
                btn.onClick = function() {
                    self._model.ToggleDragonType(dragonTypes[idx]);
                    self.UpdateSettingsUi();
                };
            })(i);
        }
        ui.dragonTypes = dragonTypes;

        this._settings.GetChild("BackBtn").onClick = function() { self.OnUiButton("back"); };
        this._settingsUi = ui;
    }

    AdjustParam(idx, dir)
    {
        var p = DG_PARAMS[idx];
        var m = this._model;
        var value = m.tuning[p.key] + dir * p.step;
        value = Math.max(p.min, Math.min(p.max, value));
        m.SetTuning(p.key, value);
        this.UpdateSettingsUi();
    }

    UpdateSettingsUi()
    {
        if (this._settingsUi === null)
            return;

        var m = this._model;
        for (var i = 0; i < DG_PARAMS.length; i++)
            this._settingsUi.values[i].SetText("" + m.tuning[DG_PARAMS[i].key]);

        for (var i = 0; i < DD.UNIT_CLASSES.length; i++)
        {
            var on = m.enabledUnits[DD.UNIT_CLASSES[i]] === true;
            this._settingsUi.unitToggles[i].color = on ? new Color4(255, 255, 255, 255) : new Color4(90, 90, 90, 120);
        }
        for (var i = 0; i < this._settingsUi.dragonTypes.length; i++)
        {
            var on = m.enabledDragons[this._settingsUi.dragonTypes[i]] === true;
            this._settingsUi.dragonToggles[i].color = on ? new Color4(255, 255, 255, 255) : new Color4(90, 90, 90, 120);
        }
    }

    MakeBarRef(path, x, y, w, h)
    {
        return { layout: this._actor.GetChild(path).GetLayout(), widget: this._actor.GetChild(path), x: x, y: y, w: w, h: h };
    }

    SetBarFill(bar, frac)
    {
        if (frac <= 0.001)
        {
            bar.widget.SetEnabled(false);
            return;
        }
        bar.widget.SetEnabled(true);
        var width = Math.max(18, bar.w * Math.min(1, frac));
        bar.layout.SetOffsetMax(new Vec2(bar.x - bar.w / 2 + width, bar.y + bar.h / 2));
    }

    WireButtons()
    {
        var self = this;

        this._actor.GetChild("../Battle/Hud/NextWaveBtn").onClick = function() { self.OnUiButton("nextWave"); };

        var classes = ["tank", "archer", "mage", "support"];
        for (var i = 0; i < 4; i++)
        {
            (function(idx) {
                self._actor.GetChild("../Battle/BuyPanel/Buy" + idx).onClick =
                    function() { self.OnUiButton("buy_" + classes[idx]); };
            })(i);
        }

        var boosters = ["bomb", "freeze", "swap"];
        for (var i = 0; i < 3; i++)
        {
            (function(idx) {
                self._actor.GetChild("../Battle/BoosterPanel/Booster" + idx).onClick =
                    function() { self.OnUiButton("booster_" + boosters[idx]); };
            })(i);
        }

        this._actor.GetChild("../Castle/BarracksBtn").onClick = function() { self.OnUiButton("barracks"); };
        this._actor.GetChild("../Castle/ForgeBtn").onClick = function() { self.OnUiButton("forge"); };
        this._actor.GetChild("../Castle/ToBattleBtn").onClick = function() { self.OnUiButton("toBattle"); };
        this._actor.GetChild("../Castle/BalanceBtn").onClick = function() { self.OnUiButton("balance"); };
    }

    Update(dt)
    {
        if (!this._started)
            return;

        this._time += dt;

        if (this._input !== null)
        {
            if (this._input.IsClicked())
            {
                var p = this._input.GetCursorWorld();
                this.HandleWorldClick(p.x, p.y);
            }
            if (this._input.IsRightClicked())
                this.CancelMode();
        }

        this.UpdateAimHighlight();
        this.UpdateHintPulse();
        this.UpdateTweens(dt);
        this.UpdateFlights(dt);
        this.UpdateFlashes(dt);
        this.UpdateFx(dt);
    }

    // ---- input ----

    OnUiButton(id)
    {
        var m = this._model;

        if (m.state === "victory" || m.state === "defeat")
        {
            this.ReturnToCastle();
            return;
        }

        if (id === "balance" && m.state === "castle")
        {
            this._settingsOpen = true;
            this.UpdateSettingsUi();
            this.ShowScreens();
            return;
        }

        if (id === "back")
        {
            this._settingsOpen = false;
            this.ShowScreens();
            return;
        }

        if (this._settingsOpen)
            return; // the settings screen only reacts to its own buttons

        if (id === "toBattle" && m.state === "castle")
        {
            m.StartBattle();
            this.ClearBattleViews();
            this._mode = "normal";
            this.SetMessage("");
            this.ShowScreens();
            this.AfterModelAction();
            return;
        }

        if (id === "barracks" && m.state === "castle")
        {
            m.UpgradeBarracks();
            this.UpdateHud();
            return;
        }

        if (id === "forge" && m.state === "castle")
        {
            m.UpgradeForge();
            this.UpdateHud();
            return;
        }

        if (m.state !== "battle")
            return;

        if (id === "nextWave")
        {
            m.CallNextWave();
            this.AfterModelAction();
        }
        else if (id.indexOf("buy_") === 0)
        {
            m.BuyUnit(id.substring(4));
            this.AfterModelAction();
        }
        else if (id.indexOf("booster_") === 0)
            this.OnBoosterClick(id.substring(8));
    }

    OnBoosterClick(kind)
    {
        var m = this._model;
        if (this._mode === kind || (kind === "swap" && (this._mode === "swapA" || this._mode === "swapB")))
        {
            this.CancelMode();
            return;
        }

        if (kind === "freeze")
        {
            if (m.UseFreeze().ok)
                this.AfterModelAction();
            return;
        }

        if (kind === "bomb" && m.boosters.bomb > 0)
        {
            this._mode = "bomb";
            this.SetMessage("Клик по полю — взрыв 3х3 (ПКМ — отмена)");
        }
        else if (kind === "swap" && m.boosters.swap > 0)
        {
            this._mode = "swapA";
            this.SetMessage("Рокировка: выберите первого солдата");
        }
    }

    HandleWorldClick(x, y)
    {
        var m = this._model;

        if (m.state === "victory" || m.state === "defeat")
        {
            this.ReturnToCastle();
            return;
        }

        if (m.state !== "battle")
            return;

        var row = DG_LAYOUT.rowAt(y);
        if (row < 0 || row >= DD.ROWS)
            return;
        var col = DG_LAYOUT.colAt(x, row);
        if (col >= 0 && col < DD.COLS)
            this.OnCellClick(col, row);
    }

    OnCellClick(col, row)
    {
        var m = this._model;

        if (this._mode === "bomb")
        {
            this._mode = "normal";
            this.SetMessage("");
            if (m.UseBomb(col, row).ok)
            {
                this.SpawnGlowFx(col, row, 200, new Color4(255, 160, 60, 255), 0.5);
                for (var i = 0; i < 6; i++)
                {
                    this.SpawnSpark(DG_LAYOUT.cellX(col, row) + (this.FxRand(60) - 30),
                                    DG_LAYOUT.cellY(row) + (this.FxRand(60) - 30),
                                    new Color4(255, 200 - this.FxRand(80), 60, 255), 40 + this.FxRand(30));
                }
            }
            this.AfterModelAction();
            return;
        }

        if (this._mode === "swapA")
        {
            if (m.IsUnit(m.Cell(col, row)))
            {
                this._swapFirst = { col: col, row: row };
                this._mode = "swapB";
                this.SetMessage("Рокировка: выберите второго солдата");
            }
            return;
        }

        if (this._mode === "swapB")
        {
            var first = this._swapFirst;
            this._mode = "normal";
            this._swapFirst = null;
            this.SetMessage("");
            m.UseSwap(first.col, first.row, col, row);
            this.AfterModelAction();
            return;
        }

        m.ClickCell(col, row);
        this.AfterModelAction();
    }

    ReturnToCastle()
    {
        if (this._model.EndBattleToCastle())
        {
            this.ClearBattleViews();
            this._mode = "normal";
            this.SetMessage("");
            this.ShowScreens();
            this.UpdateHud();
        }
    }

    CancelMode()
    {
        this._mode = "normal";
        this._swapFirst = null;
        if (this._model.state === "battle")
            this.SetMessage("");
    }

    // ---- model events -> effects ----

    AfterModelAction()
    {
        var events = this._model.TakeEvents();
        for (var i = 0; i < events.length; i++)
            this.PlayEvent(events[i]);

        this.SyncView();
        this.RefreshHints();
        this.UpdateHud();
        this.ShowScreens();
    }

    PlayEvent(e)
    {
        var L = DG_LAYOUT;
        switch (e.type)
        {
            case "merge":
            case "mergeOvercap":
            {
                var key = "u" + e.consumedId;
                var view = this._views[key];
                var tx = L.cellX(e.col, e.row), ty = L.cellY(e.row);
                if (view !== undefined)
                {
                    delete this._views[key];
                    this.StartFlight(view.actor, view.x, view.y, tx, ty, 0.16, new Color4(255, 226, 120, 255));
                }
                else
                    this.SpawnSpark(tx, ty, new Color4(255, 226, 120, 255), 46);
                this.SpawnGlowFx(e.col, e.row, 90, new Color4(255, 226, 120, 255), 0.4);
                for (var k = 0; k < 3; k++)
                    this.SpawnSpark(tx + (this.FxRand(40) - 20), ty + (this.FxRand(40) - 20),
                                    new Color4(255, 240, 160, 255), 26 + this.FxRand(18));
                break;
            }
            case "unitAttack":
            {
                var target = this._views["d" + e.targetId];
                var fx = L.cellX(e.col, e.row), fy = L.cellY(e.row);
                if (e.cls === "archer" || e.cls === "mage")
                {
                    var color = e.cls === "archer" ? new Color4(255, 244, 190, 255) : new Color4(214, 140, 255, 255);
                    var tx = target !== undefined ? target.tx : fx;
                    var ty = target !== undefined ? target.ty : fy + 128;
                    this.SpawnProjectile(fx, fy + 14, tx, ty, color, "d" + e.targetId);
                }
                else if (target !== undefined)
                {
                    this.SpawnSpark(target.tx, target.ty - 10, new Color4(255, 255, 255, 255), 34);
                    this.FlashView("d" + e.targetId);
                }
                break;
            }
            case "dragonAttack":
            {
                var tx = L.cellX(e.col, e.row), ty = L.cellY(e.row);
                if (e.dtype === "range")
                {
                    var fxp = L.cellX(e.fromCol, e.fromRow), fyp = L.cellY(e.fromRow);
                    this.SpawnProjectile(fxp, fyp, tx, ty, new Color4(140, 255, 120, 255), this.UnitKeyAt(e.col, e.row));
                }
                else
                {
                    this.SpawnSpark(tx, ty + 10, new Color4(255, 120, 90, 255), 36);
                    this.FlashView(this.UnitKeyAt(e.col, e.row));
                }
                break;
            }
            case "dragonHit":
                this.FlashView("d" + e.id);
                break;
            case "unitDied":
                this.SpawnSpark(this.ViewX("u" + e.id, e.col, e.row), this.ViewY("u" + e.id, e.row),
                                new Color4(255, 90, 70, 255), 42);
                break;
            case "dragonDied":
            {
                var view = this._views["d" + e.id];
                var x = view !== undefined ? view.tx : L.cellX(e.col, e.row);
                var y = view !== undefined ? view.ty : L.cellY(e.row);
                this.SpawnGlowFx2(x, y, 120, new Color4(255, 170, 70, 255), 0.45);
                for (var k = 0; k < 4; k++)
                    this.SpawnSpark(x + (this.FxRand(50) - 25), y + (this.FxRand(50) - 25),
                                    new Color4(255, 200, 90, 255), 30 + this.FxRand(22));
                break;
            }
            case "rockHit":
                this.SpawnSpark(L.cellX(e.col, e.row), L.cellY(e.row), new Color4(210, 190, 160, 255), 26);
                break;
            case "rockDestroyed":
                this.SpawnGlowFx(e.col, e.row, 80, new Color4(210, 180, 140, 255), 0.35);
                for (var k = 0; k < 3; k++)
                    this.SpawnSpark(L.cellX(e.col, e.row) + (this.FxRand(36) - 18),
                                    L.cellY(e.row) + (this.FxRand(30) - 15),
                                    new Color4(200, 175, 140, 255), 24 + this.FxRand(14));
                break;
            case "fireBurn":
                this.SpawnSpark(L.cellX(e.col, e.row), L.cellY(e.row) + 12, new Color4(255, 150, 40, 255), 26);
                break;
            case "fireStarted":
                this.SpawnGlowFx(e.col, e.row, 90, new Color4(255, 130, 40, 255), 0.4);
                break;
            case "breach":
            {
                var bx = L.cellX(e.col, 0);
                this.SpawnGlowFx2(bx, -340, 200, new Color4(255, 60, 40, 255), 0.6);
                break;
            }
            case "unitSpawned":
                this.SpawnGlowFx(e.col, e.row, 60, new Color4(140, 220, 255, 200), 0.3);
                break;
            case "unitBought":
                this.SpawnGlowFx(e.col, e.row, 90, new Color4(140, 255, 140, 255), 0.4);
                break;
            case "swap":
                this.SpawnGlowFx(e.c1, e.r1, 80, new Color4(140, 255, 170, 255), 0.35);
                this.SpawnGlowFx(e.c2, e.r2, 80, new Color4(140, 255, 170, 255), 0.35);
                break;
            case "supportAura":
                this.SpawnGlowFx(e.col, e.row, 100, new Color4(255, 236, 130, 130), 0.45);
                break;
        }

        if (e.type === "merge" || e.type === "mergeOvercap")
        {
            var mergedUnit = this._model.Cell(e.col, e.row);
            if (mergedUnit !== null)
                this._pendingPulse["u" + mergedUnit.id] = true;
        }
    }

    UnitKeyAt(col, row)
    {
        var cell = this._model.Cell(col, row);
        return (cell !== null && cell.kind === "unit") ? "u" + cell.id : null;
    }

    ViewX(key, col, row) { var v = this._views[key]; return v !== undefined ? v.tx : DG_LAYOUT.cellX(col, row); }
    ViewY(key, row) { var v = this._views[key]; return v !== undefined ? v.ty : DG_LAYOUT.cellY(row); }

    // ---- screens ----

    ShowScreens()
    {
        var m = this._model;
        var inBattle = m.state !== "castle";
        var settingsOpen = !inBattle && this._settingsOpen === true;
        this._battle.SetEnabled(inBattle);
        this._castle.SetEnabled(!inBattle && !settingsOpen);
        this._settings.SetEnabled(settingsOpen);
    }

    SetMessage(text)
    {
        this._hud.message.SetText(text);
        this._hud.messageShadow.SetText(text);
    }

    // ---- view sync ----

    ClearBattleViews()
    {
        var keys = Object.keys(this._views);
        for (var i = 0; i < keys.length; i++)
            this._views[keys[i]].actor.Destroy();
        this._views = {};

        for (var i = 0; i < this._fx.length; i++)
            this._fx[i].actor.Destroy();
        this._fx = [];

        for (var i = 0; i < this._flights.length; i++)
            this._flights[i].actor.Destroy();
        this._flights = [];

        this._flashes = [];
        this._pendingPulse = {};

        for (var i = 0; i < this._hintGlows.length; i++)
            this._hintGlows[i].actor.Destroy();
        this._hintGlows = [];

        for (var i = 0; i < this._aimGlows.length; i++)
            this._aimGlows[i].actor.Destroy();
        this._aimGlows = [];
    }

    MakeSpriteActor(name, sprKey, layer, x, y, fitSize)
    {
        var spr = DG_SPRITES[sprKey];
        var scale = fitSize / Math.max(spr.w, spr.h);
        var w = spr.w * scale, h = spr.h * scale;

        var a = new o2.Actor(0);
        a.SetName(name);
        this._battle.AddChild(a, this._battle.GetChildren().length);
        a.SetLayer(layer);

        var img = new o2.ImageComponent();
        a.AddComponent(img);
        img.LoadFromImage(spr.p, false);

        var t = a.GetTransform();
        t.SetSize2D(new Vec2(w, h));
        t.SetPosition2D(new Vec2(x, y));

        return { actor: a, img: img, x: x, y: y, tx: x, ty: y, w: w, h: h, sprKey: sprKey, fit: fitSize, pulse: 0 };
    }

    AddHpBar(view, color)
    {
        var bar = new o2.Actor(0);
        bar.SetName("hp");
        view.actor.AddChild(bar, 0);
        bar.SetLayer("FXBar");

        var back = new o2.ImageComponent();
        bar.AddComponent(back);
        back.LoadFromImage(DG_SPRITES.bar.p, false);
        back.SetColor(new Color4(30, 28, 34, 255));

        var fillActor = new o2.Actor(0);
        fillActor.SetName("fill");
        bar.AddChild(fillActor, 0);
        fillActor.SetLayer("FXBarFill");
        var fill = new o2.ImageComponent();
        fillActor.AddComponent(fill);
        fill.LoadFromImage(DG_SPRITES.bar.p, false);
        fill.SetMode("FillLeftToRight");
        fill.SetColor(color);

        bar.GetTransform().SetSize2D(new Vec2(44, 6));
        bar.GetTransform().SetPosition2D(new Vec2(0, -view.h / 2 - 5));
        fillActor.GetTransform().SetSize2D(new Vec2(42, 4));
        fillActor.GetTransform().SetPosition2D(new Vec2(0, 0));

        view.hpFill = fill;
        view.hpBack = back;
    }

    SetViewSprite(view, sprKey, fitSize)
    {
        if (view.sprKey === sprKey && (fitSize === undefined || Math.abs(fitSize - view.fit) < 0.5))
            return;
        if (fitSize !== undefined)
            view.fit = fitSize;
        var spr = DG_SPRITES[sprKey];
        var scale = view.fit / Math.max(spr.w, spr.h);
        view.w = spr.w * scale;
        view.h = spr.h * scale;
        if (view.sprKey !== sprKey)
            view.img.LoadFromImage(spr.p, false);
        view.actor.GetTransform().SetSize2D(new Vec2(view.w, view.h));
        view.sprKey = sprKey;
    }

    SyncView()
    {
        var m = this._model;
        var L = DG_LAYOUT;
        var seen = {};

        for (var c = 0; c < DD.COLS; c++)
        {
            for (var r = 0; r < DD.ROWS; r++)
            {
                var cell = m.grid[c][r];
                if (cell === null)
                    continue;

                var x = L.cellX(c, r);
                var y = L.cellY(r);
                var ps = L.unitScale(r);

                if (cell.kind === "unit")
                {
                    var key = "u" + cell.id;
                    seen[key] = true;
                    var sprKey = "unit_" + cell.cls + "_" + cell.lvl;
                    var view = this._views[key];
                    if (view === undefined)
                    {
                        view = this.MakeSpriteActor(key, sprKey, "Objects", x, y - 2, 62 * ps);
                        this.AddHpBar(view, new Color4(90, 220, 90, 255));
                        this._views[key] = view;
                    }
                    this.SetViewSprite(view, sprKey, 62 * ps);
                    view.tx = x;
                    view.ty = y - 2;
                    if (view.hpFill !== undefined)
                    {
                        var maxHp = m.UnitMaxHp(cell);
                        var showBar = cell.hp < maxHp - 0.5; // full-hp bars are just noise
                        view.hpFill.SetFill(cell.hp / maxHp);
                        view.hpFill.SetTransparency(showBar ? 1 : 0);
                        view.hpBack.SetTransparency(showBar ? 1 : 0);
                    }
                    if (this._pendingPulse[key] === true)
                    {
                        view.pulse = 0.35;
                        delete this._pendingPulse[key];
                    }
                }
                else if (cell.kind === "rock")
                {
                    var key = "r" + cell.id;
                    seen[key] = true;
                    var view = this._views[key];
                    if (view === undefined)
                    {
                        view = this.MakeSpriteActor(key, "rock", "Objects", x, y - 4, 60 * ps);
                        this._views[key] = view;
                    }
                    this.SetViewSprite(view, "rock", 60 * ps);
                    var hpFrac = cell.hp / cell.maxHp;
                    var shade = Math.round(160 + 95 * hpFrac);
                    view.img.SetColor(new Color4(shade, shade, shade, 255));
                    view.tx = x;
                    view.ty = y - 4;
                }
            }
        }

        for (var i = 0; i < m.dragons.length; i++)
        {
            var d = m.dragons[i];
            var key = "d" + d.id;
            seen[key] = true;
            var midRow = d.row + (d.h - 1) / 2;
            var cx = (L.cellX(d.col, d.row) + L.cellX(d.col + d.w - 1, d.row)) / 2;
            var cy = (L.cellY(d.row) + L.cellY(d.row + d.h - 1)) / 2;
            var ps = L.unitScale(d.row);
            var view = this._views[key];
            if (view === undefined)
            {
                view = this.MakeSpriteActor(key, "dragon_" + d.type, "Dragons", cx, cy, (d.w > 1 ? 148 : 66) * ps);
                this.AddHpBar(view, new Color4(230, 80, 60, 255));
                this._views[key] = view;
            }
            this.SetViewSprite(view, "dragon_" + d.type, (d.w > 1 ? 148 : 66) * ps);
            view.tx = cx;
            view.ty = cy;
            if (view.hpFill !== undefined)
                view.hpFill.SetFill(d.hp / d.maxHp);
            if (!this.IsFlashing(key))
                view.img.SetColor(m.freezeTurns > 0 ? new Color4(140, 190, 255, 255) : new Color4(255, 255, 255, 255));
        }

        var fireKeys = Object.keys(m.fires);
        for (var i = 0; i < fireKeys.length; i++)
        {
            var parts = fireKeys[i].split("_");
            var c = parseInt(parts[0]), r = parseInt(parts[1]);
            var key = "f" + fireKeys[i];
            seen[key] = true;
            if (this._views[key] === undefined)
                this._views[key] = this.MakeSpriteActor(key, "fire", "FX", L.cellX(c, r), L.cellY(r), 56 * L.unitScale(r));
        }

        var keys = Object.keys(this._views);
        for (var i = 0; i < keys.length; i++)
        {
            if (seen[keys[i]] !== true)
            {
                this._views[keys[i]].actor.Destroy();
                delete this._views[keys[i]];
            }
        }
    }

    // ---- move hints ----

    // pooled glows are hidden via transparency: SetEnabled on a fresh actor within the
    // same frame leaves its drawable out of the layer, so avoid toggling actors
    EnsureGlowPool(pool, count, layer)
    {
        while (pool.length < count)
        {
            var view = this.MakeSpriteActor("hint", "glow", layer, 0, 2000, 64);
            view.img.SetTransparency(0);
            view.active = false;
            pool.push(view);
        }
    }

    HideGlow(glow)
    {
        glow.active = false;
        glow.img.SetTransparency(0);
    }

    RefreshHints()
    {
        var m = this._model;
        var cells = [];
        if (m.state === "battle")
        {
            for (var c = 0; c < DD.COLS; c++)
                for (var r = 0; r < DD.DEFENSE_ROWS; r++)
                {
                    var u = m.grid[c][r];
                    if (u !== null && u.kind === "unit" && m._MergeNeighbor(c, r, u.cls, u.lvl) !== null)
                        cells.push({ c: c, r: r });
                }
        }

        this.EnsureGlowPool(this._hintGlows, cells.length, "Field");
        for (var i = 0; i < this._hintGlows.length; i++)
        {
            var glow = this._hintGlows[i];
            if (i < cells.length)
            {
                var s = DG_LAYOUT.xScale(cells[i].r);
                glow.active = true;
                glow.actor.GetTransform().SetPosition2D(new Vec2(DG_LAYOUT.cellX(cells[i].c, cells[i].r),
                                                                 DG_LAYOUT.cellY(cells[i].r) - 8));
                glow.actor.GetTransform().SetSize2D(new Vec2(100 * s, 74 * s));
                glow.img.SetColor(new Color4(255, 190, 40, 255));
            }
            else
                this.HideGlow(glow);
        }
    }

    UpdateHintPulse()
    {
        var a = 0.72 + 0.24 * Math.sin(this._time * 5);
        for (var i = 0; i < this._hintGlows.length; i++)
        {
            if (this._hintGlows[i].active)
                this._hintGlows[i].img.SetTransparency(a);
        }
    }

    // ---- booster aim highlight ----

    UpdateAimHighlight()
    {
        var m = this._model;
        var aiming = m.state === "battle" && (this._mode === "bomb" || this._mode === "swapA" || this._mode === "swapB");

        this.EnsureGlowPool(this._aimGlows, aiming ? 11 : 0, "Field");

        var used = 0;
        if (aiming && this._input !== null)
        {
            var p = this._input.GetCursorWorld();
            var row = DG_LAYOUT.rowAt(p.y);
            var col = row >= 0 ? DG_LAYOUT.colAt(p.x, row) : -1;
            var inField = row >= 0 && row < DD.ROWS && col >= 0 && col < DD.COLS;

            if (this._mode === "bomb" && inField)
            {
                for (var dc = -1; dc <= 1; dc++)
                {
                    for (var dr = -1; dr <= 1; dr++)
                    {
                        var cc = col + dc, rr = row + dr;
                        if (cc < 0 || cc >= DD.COLS || rr < 0 || rr >= DD.ROWS)
                            continue;
                        this.PlaceAimGlow(used++, cc, rr, new Color4(255, 90, 60, 255), 0.4);
                    }
                }
            }
            else if ((this._mode === "swapA" || this._mode === "swapB") && inField)
            {
                var cell = m.Cell(col, row);
                if (cell !== null && cell.kind === "unit")
                    this.PlaceAimGlow(used++, col, row, new Color4(120, 255, 150, 255), 0.45);
            }

            if (this._mode === "swapB" && this._swapFirst !== null)
                this.PlaceAimGlow(used++, this._swapFirst.col, this._swapFirst.row, new Color4(255, 226, 110, 255), 0.5);
        }

        for (var i = used; i < this._aimGlows.length; i++)
            this.HideGlow(this._aimGlows[i]);
    }

    PlaceAimGlow(idx, col, row, color, alpha)
    {
        var glow = this._aimGlows[idx];
        var s = DG_LAYOUT.xScale(row);
        glow.active = true;
        glow.actor.GetTransform().SetPosition2D(new Vec2(DG_LAYOUT.cellX(col, row), DG_LAYOUT.cellY(row)));
        glow.actor.GetTransform().SetSize2D(new Vec2(96 * s, 72 * s));
        glow.img.SetColor(color);
        glow.img.SetTransparency(Math.min(1, alpha + 0.3));
    }

    // ---- HUD ----

    UpdateHud()
    {
        var m = this._model;

        this._hud.castleHp.SetText("ЗАМОК HP: " + m.castleHP + "/" + DD.CASTLE_HP);
        this.SetBarFill(this._hud.hpBar, m.castleHP / DD.CASTLE_HP);

        var freezeNote = m.freezeTurns > 0 ? "   ЗАМОРОЗКА (" + m.freezeTurns + ")" : "";
        this._hud.fortWave.SetText("ФОРТ " + m.barracksLvl + "   ВОЛНА: " + m.wave + "/" + m.maxWaves + freezeNote);
        this.SetBarFill(this._hud.waveBar, m.wave / m.maxWaves);

        this._hud.coins.SetText("Монеты: " + m.coins);
        var chargeValues = [m.boosters.bomb, m.boosters.freeze, m.boosters.swap];
        for (var i = 0; i < 3; i++)
            this._hud.charges[i].SetText("x" + chargeValues[i]);

        if (m.state === "victory")
            this.SetMessage("ПОБЕДА! Клик — в замок");
        else if (m.state === "defeat")
            this.SetMessage("ПОРАЖЕНИЕ... Клик — в замок");

        // castle screen
        this._hud.currency.SetText("Монеты: " + m.coins + "   Материалы: " + m.materials);
        this._hud.barracksInfo.SetText("КАЗАРМЫ УР. " + m.barracksLvl);
        if (m.barracksLvl >= 3)
            this._hud.barracksBtnText.text = "МАКС. УРОВЕНЬ";
        else
        {
            var cost = DD.BARRACKS_COSTS[m.barracksLvl];
            this._hud.barracksBtnText.text = "УЛУЧШИТЬ (" + cost.coins + "з " + cost.materials + "м)";
        }
        this._hud.forgeInfo.SetText("КУЗНИЦА УР. " + m.forgeLvl);
        if (m.forgeLvl >= 2)
            this._hud.forgeBtnText.text = "МАКС. УРОВЕНЬ";
        else
        {
            var fcost = DD.FORGE_COSTS[m.forgeLvl];
            this._hud.forgeBtnText.text = "УЛУЧШИТЬ (" + fcost.coins + "з " + fcost.materials + "м)";
        }
    }

    // ---- effects ----

    SpawnFxSprite(sprKey, layer, x, y, size, color, ttl, grow)
    {
        var view = this.MakeSpriteActor("fx", sprKey, layer, x, y, size);
        if (color !== undefined)
            view.img.SetColor(color);
        this._fx.push({ actor: view.actor, img: view.img, ttl: ttl, maxTtl: ttl, grow: grow === true, baseW: view.w, baseH: view.h });
        return view;
    }

    SpawnGlowFx(col, row, size, color, ttl)
    {
        this.SpawnFxSprite("glow", "FX", DG_LAYOUT.cellX(col, row), DG_LAYOUT.cellY(row), size, color, ttl, true);
    }

    SpawnGlowFx2(x, y, size, color, ttl)
    {
        this.SpawnFxSprite("glow", "FX", x, y, size, color, ttl, true);
    }

    SpawnSpark(x, y, color, size)
    {
        this.SpawnFxSprite("spark", "FX", x, y, size, color, 0.32, true);
    }

    SpawnProjectile(x0, y0, x1, y1, color, targetKey)
    {
        var view = this.MakeSpriteActor("proj", "comet", "FX", x0, y0, 52);
        view.img.SetColor(color);
        var dx = x1 - x0, dy = y1 - y0;
        view.actor.GetTransform().SetAngle(Math.atan2(dy, dx));
        this._flights.push({
            actor: view.actor, img: view.img, x0: x0, y0: y0, x1: x1, y1: y1,
            t: 0, dur: Math.max(0.14, Math.sqrt(dx * dx + dy * dy) / 1600),
            sparkColor: color, targetKey: targetKey, projectile: true
        });
    }

    StartFlight(actor, x0, y0, x1, y1, dur, sparkColor)
    {
        this._flights.push({ actor: actor, img: null, x0: x0, y0: y0, x1: x1, y1: y1,
                             t: 0, dur: dur, sparkColor: sparkColor, targetKey: null, projectile: false });
    }

    FlashView(key)
    {
        if (key === null)
            return;
        var view = this._views[key];
        if (view === undefined)
            return;
        this._flashes.push({ key: key, img: view.img, t: 0.22, dur: 0.22 });
    }

    IsFlashing(key)
    {
        for (var i = 0; i < this._flashes.length; i++)
            if (this._flashes[i].key === key)
                return true;
        return false;
    }

    // ---- animation ----

    UpdateTweens(dt)
    {
        var k = Math.min(1, dt * 10);
        var keys = Object.keys(this._views);
        for (var i = 0; i < keys.length; i++)
        {
            var v = this._views[keys[i]];

            if (v.pulse > 0)
            {
                v.pulse = Math.max(0, v.pulse - dt);
                var phase = 1 - v.pulse / 0.35;
                var s = 1 + 0.28 * Math.sin(Math.PI * phase);
                v.actor.GetTransform().SetScale2D(new Vec2(s, s));
                if (v.pulse === 0)
                    v.actor.GetTransform().SetScale2D(new Vec2(1, 1));
            }

            var dx = v.tx - v.x, dy = v.ty - v.y;
            if (dx * dx + dy * dy < 0.25)
            {
                if (v.x !== v.tx || v.y !== v.ty)
                {
                    v.x = v.tx;
                    v.y = v.ty;
                    v.actor.GetTransform().SetPosition2D(new Vec2(v.x, v.y));
                }
                continue;
            }
            v.x += dx * k;
            v.y += dy * k;
            v.actor.GetTransform().SetPosition2D(new Vec2(v.x, v.y));
        }
    }

    UpdateFlights(dt)
    {
        for (var i = this._flights.length - 1; i >= 0; i--)
        {
            var f = this._flights[i];
            f.t += dt;
            var k = Math.min(1, f.t / f.dur);
            var x = f.x0 + (f.x1 - f.x0) * k;
            var y = f.y0 + (f.y1 - f.y0) * k;
            f.actor.GetTransform().SetPosition2D(new Vec2(x, y));

            if (k >= 1)
            {
                if (f.sparkColor !== undefined && f.sparkColor !== null)
                    this.SpawnSpark(f.x1, f.y1, f.sparkColor, 34);
                if (f.targetKey !== null)
                    this.FlashView(f.targetKey);
                f.actor.Destroy();
                this._flights.splice(i, 1);
            }
        }
    }

    UpdateFlashes(dt)
    {
        for (var i = this._flashes.length - 1; i >= 0; i--)
        {
            var fl = this._flashes[i];
            fl.t -= dt;
            var view = this._views[fl.key];
            if (view === undefined)
            {
                this._flashes.splice(i, 1);
                continue;
            }
            if (fl.t <= 0)
            {
                view.img.SetColor(this._model.freezeTurns > 0 && fl.key.charAt(0) === "d"
                    ? new Color4(140, 190, 255, 255) : new Color4(255, 255, 255, 255));
                this._flashes.splice(i, 1);
            }
            else
            {
                var k = fl.t / fl.dur;
                view.img.SetColor(new Color4(255, Math.round(255 - 165 * k), Math.round(255 - 175 * k), 255));
            }
        }
    }

    UpdateFx(dt)
    {
        for (var i = this._fx.length - 1; i >= 0; i--)
        {
            var fx = this._fx[i];
            fx.ttl -= dt;
            if (fx.ttl <= 0)
            {
                fx.actor.Destroy();
                this._fx.splice(i, 1);
            }
            else
            {
                var k = fx.ttl / fx.maxTtl;
                fx.img.SetTransparency(k);
                if (fx.grow)
                {
                    var s = 1 + 0.6 * (1 - k);
                    fx.actor.GetTransform().SetSize2D(new Vec2(fx.baseW * s, fx.baseH * s));
                }
            }
        }
    }
};
