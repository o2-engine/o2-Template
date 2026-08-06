// Dragon Defense: Merge & Blast — pure game model. No engine calls: testable headless.
// Globals are assigned (not declared) so they become own-properties of the JS global.

DD = {
    COLS: 7,
    ROWS: 10,
    DEFENSE_ROWS: 7,

    UNIT_CLASSES: ["tank", "archer", "mage", "support"],
    MAX_UNIT_LEVEL: 3,
    UNIT_STATS: {
        tank:    [{ hp: 30,  dmg: 4  }, { hp: 70, dmg: 9  }, { hp: 150, dmg: 18 }],
        archer:  [{ hp: 12,  dmg: 6  }, { hp: 26, dmg: 14 }, { hp: 55,  dmg: 30 }],
        mage:    [{ hp: 10,  dmg: 5  }, { hp: 22, dmg: 12 }, { hp: 48,  dmg: 26 }],
        support: [{ hp: 14,  dmg: 6  }, { hp: 30, dmg: 14 }, { hp: 60,  dmg: 30 }]
    },
    SUPPORT_HEAL: [3, 7, 15],
    SUPPORT_BUFF: [0.15, 0.25, 0.40],
    MAGE_RANGE: 5,
    MAGE_SPLASH: 0.5,

    DRAGON_STATS: {
        normal: { hp: 25,  dmg: 6,  armor: 0, breach: 10, w: 1, h: 1 },
        tankd:  { hp: 60,  dmg: 5,  armor: 3, breach: 15, w: 1, h: 1 },
        range:  { hp: 20,  dmg: 7,  armor: 0, breach: 10, w: 1, h: 1, range: 4 },
        fire:   { hp: 30,  dmg: 5,  armor: 0, breach: 12, w: 1, h: 1 },
        boss:   { hp: 300, dmg: 20, armor: 2, breach: 50, w: 2, h: 2 }
    },

    WAVES: [
        ["normal", "normal"],
        ["normal", "normal", "normal"],
        ["normal", "tankd"],
        ["range", "normal", "normal"],
        ["tankd", "range", "normal"],
        ["fire", "normal", "normal", "normal"],
        ["tankd", "tankd", "range"],
        ["fire", "range", "normal", "tankd"],
        ["range", "range", "tankd", "fire", "normal"],
        ["boss", "range", "tankd"]
    ],

    ROCK_HP: 40,
    ROCK_COUNT: 9,
    START_UNIT_ROWS: 4,
    FIRE_DMG: 4,
    FIRE_TURNS: 3,
    FIRE_PERIOD: 2,
    MERGE_ROCK_DMG: 10,
    OVERCAP_BONUS: 0.2,

    CASTLE_HP: 100,
    BOOSTER_CHARGES: { bomb: 2, freeze: 1, swap: 2 },
    BOMB_DMG: 60,
    FREEZE_TURNS: 2,

    BUY_UNIT_COST: 100,
    BUY_UNIT_LEVEL: 2,
    BARRACKS_COSTS: [null, { coins: 200, materials: 50 }, { coins: 500, materials: 150 }],
    FORGE_COSTS: [{ coins: 150, materials: 30 }, { coins: 400, materials: 100 }],
    FORGE_ROCK_MULT: 1.5,
    WIN_COINS_BASE: 150,
    WIN_COINS_PER_WAVE: 20,
    WIN_MATERIALS: 30,
    LOSE_COINS_PER_WAVE: 10,
    START_COINS: 150,
    START_MATERIALS: 0
};

DragonModel = class DragonModel
{
    constructor(seed)
    {
        this._seed = (seed === undefined ? 12345 : seed) | 0;
        this.coins = DD.START_COINS;
        this.materials = DD.START_MATERIALS;
        this.barracksLvl = 2; // barracks level 2 is prebuilt: merges up to L2 work from the start
        this.forgeLvl = 0;
        this.state = "castle"; // castle | battle | victory | defeat

        // balance settings screen edits these; they apply to the next battle
        this.tuning = {
            castleHP: DD.CASTLE_HP,
            maxWaves: DD.WAVES.length,
            rockCount: DD.ROCK_COUNT,
            rockHp: DD.ROCK_HP,
            startUnitRows: DD.START_UNIT_ROWS,
            buyCost: DD.BUY_UNIT_COST,
            buyLevel: DD.BUY_UNIT_LEVEL,
            bombCharges: DD.BOOSTER_CHARGES.bomb,
            freezeCharges: DD.BOOSTER_CHARGES.freeze,
            swapCharges: DD.BOOSTER_CHARGES.swap,
            bombDmg: DD.BOMB_DMG,
            freezeTurns: DD.FREEZE_TURNS,
            fireDmg: DD.FIRE_DMG
        };
        this.enabledUnits = { tank: true, archer: true, mage: true, support: true };
        this.enabledDragons = { normal: true, tankd: true, range: true, fire: true, boss: true };

        this.events = [];
        this._ResetBattleData();
    }

    SetTuning(key, value)
    {
        this.tuning[key] = value;
    }

    ToggleUnitType(cls)
    {
        this.enabledUnits[cls] = !this.enabledUnits[cls];
        return this.enabledUnits[cls];
    }

    ToggleDragonType(type)
    {
        this.enabledDragons[type] = !this.enabledDragons[type];
        return this.enabledDragons[type];
    }

    _RandomUnitClass()
    {
        var pool = [];
        for (var i = 0; i < DD.UNIT_CLASSES.length; i++)
            if (this.enabledUnits[DD.UNIT_CLASSES[i]] === true)
                pool.push(DD.UNIT_CLASSES[i]);
        if (pool.length === 0)
            return null;
        return pool[this.Rand(pool.length)];
    }

    _ResolveDragonType(type)
    {
        if (this.enabledDragons[type] === true)
            return type;
        var fallback = ["normal", "tankd", "range", "fire"];
        for (var i = 0; i < fallback.length; i++)
            if (this.enabledDragons[fallback[i]] === true)
                return fallback[i];
        return null;
    }

    // ---- utils ----

    Rand(n)
    {
        this._seed = (this._seed * 1103515245 + 12345) & 0x7fffffff;
        return ((this._seed >> 16) & 0x7fff) % n; // high bits: LCG low bits cycle
    }

    _ResetBattleData()
    {
        this.grid = [];
        for (var c = 0; c < DD.COLS; c++)
        {
            this.grid.push([]);
            for (var r = 0; r < DD.ROWS; r++)
                this.grid[c].push(null);
        }
        var t = this.tuning;
        this.dragons = [];
        this.fires = {};
        this.castleHP = t !== undefined ? t.castleHP : DD.CASTLE_HP;
        this.maxWaves = t !== undefined ? t.maxWaves : DD.WAVES.length;
        this.wave = 0;
        this.turn = 0;
        this.freezeTurns = 0;
        this.boosters = t !== undefined
            ? { bomb: t.bombCharges, freeze: t.freezeCharges, swap: t.swapCharges }
            : { bomb: DD.BOOSTER_CHARGES.bomb, freeze: DD.BOOSTER_CHARGES.freeze, swap: DD.BOOSTER_CHARGES.swap };
        this._nextId = 1;
    }

    Cell(c, r)
    {
        if (c < 0 || c >= DD.COLS || r < 0 || r >= DD.ROWS)
            return null;
        return this.grid[c][r];
    }

    IsUnit(cell) { return cell !== null && cell.kind === "unit"; }
    IsRock(cell) { return cell !== null && cell.kind === "rock"; }

    UnitDamage(u)
    {
        var base = DD.UNIT_STATS[u.cls][u.lvl - 1].dmg * (1 + u.bonus);
        return base * (1 + u.buff);
    }

    UnitMaxHp(u) { return Math.round(DD.UNIT_STATS[u.cls][u.lvl - 1].hp * (1 + u.bonus)); }

    DragonAt(c, r)
    {
        for (var i = 0; i < this.dragons.length; i++)
        {
            var d = this.dragons[i];
            if (c >= d.col && c < d.col + d.w && r >= d.row && r < d.row + d.h)
                return d;
        }
        return null;
    }

    FireAt(c, r) { return this.fires[c + "_" + r] !== undefined; }

    _Emit(type, data)
    {
        data = data || {};
        data.type = type;
        this.events.push(data);
    }

    // ---- battle setup ----

    StartBattle()
    {
        this._ResetBattleData();
        this.state = "battle";
        this.events = [];

        var placedRocks = 0;
        var guard = 0;
        while (placedRocks < this.tuning.rockCount && guard++ < 500)
        {
            var c = this.Rand(DD.COLS);
            var r = 1 + this.Rand(5); // rows 1..5
            if (this.grid[c][r] === null)
            {
                this.grid[c][r] = { kind: "rock", hp: this.tuning.rockHp, maxHp: this.tuning.rockHp, id: this._nextId++ };
                placedRocks++;
            }
        }

        if (this.forgeLvl >= 2)
        {
            var rocks = this._AllRocks();
            if (rocks.length > 0)
            {
                var rock = rocks[this.Rand(rocks.length)];
                this.grid[rock.c][rock.r] = null;
                this._Emit("rockDestroyed", { col: rock.c, row: rock.r });
            }
        }

        for (var r = 0; r < this.tuning.startUnitRows; r++)
        {
            for (var c = 0; c < DD.COLS; c++)
            {
                var cls = this._RandomUnitClass();
                if (this.grid[c][r] === null && cls !== null)
                    this.grid[c][r] = this._MakeUnit(cls, 1);
            }
        }

        this.SpawnWave();
    }

    _MakeUnit(cls, lvl)
    {
        var stats = DD.UNIT_STATS[cls][lvl - 1];
        return { kind: "unit", cls: cls, lvl: lvl, hp: stats.hp, bonus: 0, buff: 0, id: this._nextId++ };
    }

    _AllRocks()
    {
        var res = [];
        for (var c = 0; c < DD.COLS; c++)
            for (var r = 0; r < DD.ROWS; r++)
                if (this.IsRock(this.grid[c][r]))
                    res.push({ c: c, r: r, rock: this.grid[c][r] });
        return res;
    }

    SpawnWave()
    {
        if (this.wave >= this.maxWaves)
            return false;

        var comp = DD.WAVES[this.wave];
        this.wave++;
        for (var i = 0; i < comp.length; i++)
        {
            var type = this._ResolveDragonType(comp[i]);
            if (type === null)
                continue; // every dragon type is disabled
            var stats = DD.DRAGON_STATS[type];
            var startCol = this.Rand(DD.COLS - stats.w + 1);
            var col = -1;
            for (var probe = 0; probe < DD.COLS; probe++)
            {
                var tryCol = (startCol + probe) % (DD.COLS - stats.w + 1);
                var free = true;
                for (var cc = tryCol; cc < tryCol + stats.w; cc++)
                    for (var rr = DD.ROWS - stats.h; rr < DD.ROWS; rr++)
                        if (this.DragonAt(cc, rr) !== null)
                            free = false;
                if (free) { col = tryCol; break; }
            }
            if (col < 0)
                continue; // no room this turn — skipped for simplicity

            this.dragons.push({
                id: this._nextId++, type: type, col: col, row: DD.ROWS - stats.h,
                w: stats.w, h: stats.h, hp: stats.hp, maxHp: stats.hp,
                armor: stats.armor, dmg: stats.dmg, breach: stats.breach,
                fireCounter: 0
            });
        }
        this._Emit("waveSpawned", { wave: this.wave });
        return true;
    }

    // ---- player actions ----

    ClickCell(c, r)
    {
        if (this.state !== "battle")
            return { ok: false, reason: "notInBattle" };
        var cell = this.Cell(c, r);
        if (!this.IsUnit(cell))
            return { ok: false, reason: "noUnit" };

        var merged = this._TryMergeAt(c, r);
        if (!merged)
            return { ok: false, reason: "noPair" };

        this.DoTurn();
        return { ok: true };
    }

    _MergeNeighbor(c, r, cls, lvl)
    {
        var dirs = [[0, 1], [-1, 0], [1, 0], [0, -1]]; // up, left, right, down
        for (var i = 0; i < dirs.length; i++)
        {
            var nc = c + dirs[i][0], nr = r + dirs[i][1];
            if (nr >= DD.DEFENSE_ROWS)
                continue;
            var cell = this.Cell(nc, nr);
            if (this.IsUnit(cell) && cell.cls === cls && cell.lvl === lvl)
                return { c: nc, r: nr };
        }
        return null;
    }

    _TryMergeAt(c, r)
    {
        var mergedOnce = false;
        for (var guard = 0; guard < 10; guard++)
        {
            var unit = this.Cell(c, r);
            var pair = this._MergeNeighbor(c, r, unit.cls, unit.lvl);
            if (pair === null)
                break;

            var consumedId = this.grid[pair.c][pair.r].id;
            this.grid[pair.c][pair.r] = null;
            mergedOnce = true;

            if (unit.lvl < DD.MAX_UNIT_LEVEL && unit.lvl < this.barracksLvl)
            {
                unit.lvl++;
                unit.hp = this.UnitMaxHp(unit);
                this._Emit("merge", { col: c, row: r, fromCol: pair.c, fromRow: pair.r, lvl: unit.lvl, consumedId: consumedId });
            }
            else
            {
                unit.bonus += DD.OVERCAP_BONUS;
                unit.hp = this.UnitMaxHp(unit);
                this._Emit("mergeOvercap", { col: c, row: r, fromCol: pair.c, fromRow: pair.r, bonus: unit.bonus, consumedId: consumedId });
                break; // overcap merge does not cascade
            }
        }

        if (mergedOnce)
            this._DamageRocksAround(c, r, DD.MERGE_ROCK_DMG * this.Cell(c, r).lvl * this._RockMult());

        return mergedOnce;
    }

    _RockMult() { return this.forgeLvl >= 1 ? DD.FORGE_ROCK_MULT : 1; }

    _DamageRocksAround(c, r, dmg)
    {
        var dirs = [[0, 1], [-1, 0], [1, 0], [0, -1]];
        for (var i = 0; i < dirs.length; i++)
            this._DamageRockAt(c + dirs[i][0], r + dirs[i][1], dmg);
    }

    _DamageRockAt(c, r, dmg)
    {
        var cell = this.Cell(c, r);
        if (!this.IsRock(cell))
            return;
        cell.hp -= dmg;
        this._Emit("rockHit", { col: c, row: r, dmg: dmg });
        if (cell.hp <= 0)
        {
            this.grid[c][r] = null;
            this._Emit("rockDestroyed", { col: c, row: r });
        }
    }

    BuyUnit(cls)
    {
        if (this.state !== "battle")
            return { ok: false, reason: "notInBattle" };
        if (this.enabledUnits[cls] !== true)
            return { ok: false, reason: "classDisabled" };
        if (this.coins < this.tuning.buyCost)
            return { ok: false, reason: "noCoins" };

        var spot = null;
        for (var r = 0; r < DD.DEFENSE_ROWS && spot === null; r++)
        {
            var freeCols = [];
            for (var c = 0; c < DD.COLS; c++)
                if (this.grid[c][r] === null && this.DragonAt(c, r) === null)
                    freeCols.push(c);
            if (freeCols.length > 0)
                spot = { c: freeCols[this.Rand(freeCols.length)], r: r };
        }
        if (spot === null)
            return { ok: false, reason: "noSpace" };

        this.coins -= this.tuning.buyCost;
        this.grid[spot.c][spot.r] = this._MakeUnit(cls, Math.min(this.tuning.buyLevel, this.barracksLvl));
        this._Emit("unitBought", { col: spot.c, row: spot.r, cls: cls });
        this.DoTurn();
        return { ok: true, col: spot.c, row: spot.r };
    }

    UseBomb(c, r)
    {
        if (this.state !== "battle" || this.boosters.bomb <= 0)
            return { ok: false };
        this.boosters.bomb--;
        var bombToken = this._nextId++;
        for (var dc = -1; dc <= 1; dc++)
        {
            for (var dr = -1; dr <= 1; dr++)
            {
                var cc = c + dc, rr = r + dr;
                this._DamageRockAt(cc, rr, this.tuning.bombDmg);
                var d = this.DragonAt(cc, rr);
                if (d !== null && d._bombed !== bombToken)
                {
                    d._bombed = bombToken; // one hit per bomb even for 2x2 boss
                    this._HitDragon(d, this.tuning.bombDmg, true);
                }
            }
        }
        this._Emit("bomb", { col: c, row: r });
        this._Cleanup();
        this._CheckBattleEnd();
        return { ok: true };
    }

    UseFreeze()
    {
        if (this.state !== "battle" || this.boosters.freeze <= 0)
            return { ok: false };
        this.boosters.freeze--;
        this.freezeTurns = this.tuning.freezeTurns;
        this._Emit("freeze", {});
        return { ok: true };
    }

    UseSwap(c1, r1, c2, r2)
    {
        if (this.state !== "battle" || this.boosters.swap <= 0)
            return { ok: false };
        var a = this.Cell(c1, r1), b = this.Cell(c2, r2);
        if (!this.IsUnit(a) || !this.IsUnit(b) || (c1 === c2 && r1 === r2))
            return { ok: false };
        this.boosters.swap--;
        this.grid[c1][r1] = b;
        this.grid[c2][r2] = a;
        this._Emit("swap", { c1: c1, r1: r1, c2: c2, r2: r2 });
        return { ok: true };
    }

    CallNextWave()
    {
        if (this.state !== "battle")
            return false;
        return this.SpawnWave();
    }

    // ---- turn phases ----

    DoTurn()
    {
        this.turn++;
        this._GravityAndSpawn();
        this._UnitsAct();
        this._DragonsAct();
        this._FireTick();
        this._Cleanup();
        if (this.dragons.length === 0 && this.wave < this.maxWaves && this.state === "battle")
            this.SpawnWave();
        this._CheckBattleEnd();
    }

    _CellBlocked(c, r) { return this.grid[c][r] !== null || this.DragonAt(c, r) !== null; }

    _GravityAndSpawn()
    {
        for (var c = 0; c < DD.COLS; c++)
        {
            // units float up inside rock/dragon-free segments of the column
            for (var r = DD.DEFENSE_ROWS - 1; r >= 0; r--)
            {
                if (this.grid[c][r] !== null || this.DragonAt(c, r) !== null)
                    continue;
                for (var below = r - 1; below >= 0; below--)
                {
                    var cell = this.grid[c][below];
                    if (this.IsRock(cell) || this.DragonAt(c, below) !== null)
                        break;
                    if (this.IsUnit(cell))
                    {
                        this.grid[c][r] = cell;
                        this.grid[c][below] = null;
                        this._Emit("unitMoved", { id: cell.id, fromCol: c, fromRow: below, col: c, row: r });
                        break;
                    }
                }
            }

            if (this.grid[c][0] === null && this.DragonAt(c, 0) === null)
            {
                var cls = this._RandomUnitClass();
                if (cls !== null)
                {
                    this.grid[c][0] = this._MakeUnit(cls, 1);
                    this._Emit("unitSpawned", { col: c, row: 0 });
                }
            }
        }
    }

    _NearestDragonInCol(c, fromRow, maxDist)
    {
        var best = null, bestRow = -1;
        for (var r = fromRow + 1; r < DD.ROWS; r++)
        {
            if (maxDist !== undefined && r - fromRow > maxDist)
                break;
            var d = this.DragonAt(c, r);
            if (d !== null)
                return { dragon: d, row: r };
        }
        return null;
    }

    _NearestRockInCol(c, fromRow, maxDist)
    {
        for (var r = fromRow + 1; r < DD.DEFENSE_ROWS; r++)
        {
            if (maxDist !== undefined && r - fromRow > maxDist)
                break;
            if (this.IsRock(this.grid[c][r]))
                return r;
        }
        return null;
    }

    _HitDragon(d, dmg, ignoreArmor)
    {
        var effective = Math.max(1, Math.round(dmg - (ignoreArmor ? 0 : d.armor)));
        d.hp -= effective;
        this._Emit("dragonHit", { id: d.id, dmg: effective });
        if (d.hp <= 0)
            this._Emit("dragonDied", { id: d.id, col: d.col, row: d.row });
    }

    _UnitsAct()
    {
        var c, r, u;

        // support: heal and buff first so attacks this turn benefit
        for (c = 0; c < DD.COLS; c++)
        {
            for (r = 0; r < DD.DEFENSE_ROWS; r++)
            {
                u = this.grid[c][r];
                if (this.IsUnit(u))
                    u.buff = 0;
            }
        }
        for (c = 0; c < DD.COLS; c++)
        {
            for (r = 0; r < DD.DEFENSE_ROWS; r++)
            {
                u = this.grid[c][r];
                if (!this.IsUnit(u) || u.cls !== "support")
                    continue;
                var heal = DD.SUPPORT_HEAL[u.lvl - 1] * (1 + u.bonus);
                var buff = DD.SUPPORT_BUFF[u.lvl - 1];
                for (var dc = -1; dc <= 1; dc++)
                {
                    for (var dr = -1; dr <= 1; dr++)
                    {
                        if (dc === 0 && dr === 0)
                            continue;
                        var ally = this.Cell(c + dc, r + dr);
                        if (this.IsUnit(ally))
                        {
                            ally.hp = Math.min(this.UnitMaxHp(ally), ally.hp + heal);
                            ally.buff += buff;
                        }
                    }
                }
                this._Emit("supportAura", { col: c, row: r });
            }
        }

        for (c = 0; c < DD.COLS; c++)
        {
            for (r = 0; r < DD.DEFENSE_ROWS; r++)
            {
                u = this.grid[c][r];
                if (!this.IsUnit(u))
                    continue;

                var dmg = this.UnitDamage(u);
                if (u.cls === "tank" || u.cls === "support")
                {
                    var target = this.DragonAt(c, r + 1);
                    if (target !== null && u.cls === "tank")
                    {
                        this._HitDragon(target, dmg);
                        this._Emit("unitAttack", { id: u.id, cls: u.cls, col: c, row: r, targetId: target.id });
                    }
                    else if (this.IsRock(this.Cell(c, r + 1)))
                        this._DamageRockAt(c, r + 1, dmg * this._RockMult());
                }
                else if (u.cls === "archer" || u.cls === "mage")
                {
                    var maxDist = u.cls === "mage" ? DD.MAGE_RANGE : undefined;
                    var found = this._NearestDragonInCol(c, r, maxDist);
                    if (found !== null)
                    {
                        this._HitDragon(found.dragon, dmg);
                        this._Emit("unitAttack", { id: u.id, cls: u.cls, col: c, row: r, targetId: found.dragon.id });
                        if (u.cls === "mage")
                        {
                            var splash = dmg * DD.MAGE_SPLASH;
                            var main = found.dragon;
                            for (var i = 0; i < this.dragons.length; i++)
                            {
                                var d = this.dragons[i];
                                if (d === main)
                                    continue;
                                var dx = Math.max(main.col - (d.col + d.w - 1), d.col - (main.col + main.w - 1), 0);
                                var dy = Math.max(main.row - (d.row + d.h - 1), d.row - (main.row + main.h - 1), 0);
                                if (dx <= 1 && dy <= 1)
                                    this._HitDragon(d, splash);
                            }
                        }
                    }
                    else
                    {
                        var rockRow = this._NearestRockInCol(c, r, maxDist);
                        if (rockRow !== null)
                            this._DamageRockAt(c, rockRow, dmg * this._RockMult());
                    }
                }
            }
        }

        this.dragons = this.dragons.filter(function(d) { return d.hp > 0; });
    }

    _DragonsAct()
    {
        if (this.freezeTurns > 0)
        {
            this.freezeTurns--;
            this._Emit("frozenTurn", {});
            return;
        }

        for (var i = 0; i < this.dragons.length; i++)
        {
            var d = this.dragons[i];
            var stats = DD.DRAGON_STATS[d.type];

            if (d.type === "range")
            {
                var hit = false;
                for (var r = d.row - 1; r >= 0 && d.row - r <= stats.range; r--)
                {
                    var cell = this.Cell(d.col, r);
                    if (this.IsUnit(cell))
                    {
                        cell.hp -= d.dmg;
                        this._Emit("dragonAttack", { id: d.id, dtype: d.type, fromCol: d.col, fromRow: d.row, col: d.col, row: r });
                        hit = true;
                        break;
                    }
                    if (this.IsRock(cell))
                        break;
                }
            }
            else
            {
                for (var c = d.col; c < d.col + d.w; c++)
                {
                    var below = this.Cell(c, d.row - 1);
                    if (this.IsUnit(below))
                    {
                        below.hp -= d.dmg;
                        this._Emit("dragonAttack", { id: d.id, dtype: d.type, fromCol: c, fromRow: d.row, col: c, row: d.row - 1 });
                    }
                }
            }

            if (d.type === "fire")
            {
                d.fireCounter++;
                if (d.fireCounter % DD.FIRE_PERIOD === 0)
                {
                    var fr = d.row - 1;
                    if (fr >= 0 && fr < DD.DEFENSE_ROWS && !this.IsRock(this.Cell(d.col, fr)))
                    {
                        this.fires[d.col + "_" + fr] = DD.FIRE_TURNS;
                        this._Emit("fireStarted", { col: d.col, row: fr });
                    }
                }
            }

            // move down one cell if all cells below are free
            if (d.row === 0)
            {
                this.castleHP -= d.breach;
                d.hp = 0;
                this._Emit("breach", { id: d.id, dmg: d.breach, col: d.col });
                continue;
            }
            var canMove = true;
            for (var c = d.col; c < d.col + d.w; c++)
                if (this._CellBlocked(c, d.row - 1))
                    canMove = false;
            if (canMove)
            {
                d.row--;
                this._Emit("dragonMoved", { id: d.id, col: d.col, row: d.row });
            }
        }

        this.dragons = this.dragons.filter(function(d) { return d.hp > 0; });
    }

    _FireTick()
    {
        var keys = Object.keys(this.fires);
        for (var i = 0; i < keys.length; i++)
        {
            var parts = keys[i].split("_");
            var c = parseInt(parts[0]), r = parseInt(parts[1]);
            var cell = this.Cell(c, r);
            if (this.IsUnit(cell))
            {
                cell.hp -= this.tuning.fireDmg;
                this._Emit("fireBurn", { col: c, row: r });
            }
            this.fires[keys[i]]--;
            if (this.fires[keys[i]] <= 0)
                delete this.fires[keys[i]];
        }
    }

    _Cleanup()
    {
        for (var c = 0; c < DD.COLS; c++)
        {
            for (var r = 0; r < DD.ROWS; r++)
            {
                var cell = this.grid[c][r];
                if (this.IsUnit(cell) && cell.hp <= 0)
                {
                    this._Emit("unitDied", { id: cell.id, col: c, row: r });
                    this.grid[c][r] = null;
                }
            }
        }
        this.dragons = this.dragons.filter(function(d) { return d.hp > 0; });
    }

    _CheckBattleEnd()
    {
        if (this.state !== "battle")
            return;
        if (this.castleHP <= 0)
        {
            this.castleHP = 0;
            this.state = "defeat";
            this.coins += DD.LOSE_COINS_PER_WAVE * Math.max(0, this.wave - 1);
            this._Emit("defeat", {});
        }
        else if (this.dragons.length === 0 && this.wave >= this.maxWaves)
        {
            this.state = "victory";
            this.coins += DD.WIN_COINS_BASE + DD.WIN_COINS_PER_WAVE * this.wave;
            this.materials += DD.WIN_MATERIALS;
            this._Emit("victory", {});
        }
    }

    // ---- meta ----

    EndBattleToCastle()
    {
        if (this.state === "victory" || this.state === "defeat")
        {
            this.state = "castle";
            this._ResetBattleData();
            return true;
        }
        return false;
    }

    UpgradeBarracks()
    {
        if (this.state !== "castle" || this.barracksLvl >= 3)
            return false;
        var cost = DD.BARRACKS_COSTS[this.barracksLvl];
        if (this.coins < cost.coins || this.materials < cost.materials)
            return false;
        this.coins -= cost.coins;
        this.materials -= cost.materials;
        this.barracksLvl++;
        return true;
    }

    UpgradeForge()
    {
        if (this.state !== "castle" || this.forgeLvl >= 2)
            return false;
        var cost = DD.FORGE_COSTS[this.forgeLvl];
        if (this.coins < cost.coins || this.materials < cost.materials)
            return false;
        this.coins -= cost.coins;
        this.materials -= cost.materials;
        this.forgeLvl++;
        return true;
    }

    TakeEvents()
    {
        var ev = this.events;
        this.events = [];
        return ev;
    }
};
