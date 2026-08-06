# Dragon Defense — архитектура прототипа

## Слои и разделение ответственности

```
C++ (Sources/Game/DragonDefense/)          JS (Assets/Scripts/DragonDefense/)
──────────────────────────────────         ─────────────────────────────────────
DragonDefenseBootstrap : Component          DragonModel.js  — чистая модель игры,
  строит ОБА экрана кодом в OnStart:          без единого обращения к движку
  слои, камеру, фоны, клетки, HUD,            (тестируется headless через Eval)
  панели, лейблы; исполняет DragonModel.js;
  вешает DragonInput + DragonGame.js        DragonGame.js — ScriptableComponent-
                                              контроллер: владеет DragonModel,
DragonInputComponent : Component              создаёт/обновляет вью-акторов
  @SCRIPTABLE мост ввода:                     (спрайты юнитов/драконов/камней),
  IsClicked/IsRightClicked/GetCursorWorld     поллит DragonInput, крутит твины,
  (screen→world для FittedSize камеры)        пишет тексты в Label'ы HUD
```

Правило: **модель не знает о сцене**, вью читает модель после каждого шага и досоздаёт/удаляет/
двигает акторов. Ввод — единый путь: клик → мировые координаты → хит-ректы (клетки и все кнопки).
UI-виджеты движка не используются для кликов (Button не нужен) — только Label для текста.

## Сцена (собирается кодом, bootstrap)

```
Scene layers (порядок отрисовки): BG, Field, Objects, Dragons, FX, UI
Актор-иерархия:
  ui camera            CameraActor FittedSize(1280×800), рисует BG..UI
  Bootstrap            актор с DragonDefenseBootstrap (из .scn) — строит остальное
  Game                 DragonInputComponent + ScriptableComponent(DragonGame.js)
  Battle               (0,0), size 0 — экран боя
    BG                 battle_bg (BG)
    Cells/c{col}_{row} 70 клеток cell.png (Field), небо подкрашено голубым
    FrontLine          front_line.png (Field), y=134
    Hud/...            панели (UI-спрайты), Label'ы: CastleHP, Fort, Wave, Coins,
                       NextWave (кнопка-спрайт + Label), Message (центр, крупный)
    BuyPanel/...       4 кнопки-спрайта с иконками юнитов + Label цены
    BoosterPanel/...   3 кнопки-спрайта (бомба/заморозка/рокировка) + Label зарядов
  Castle               экран замка (выключен на старте боя)
    BG, Barracks, Forge, лейблы валют, кнопки апгрейдов, кнопка "В БОЙ"
```

Динамические акторы (создаёт DragonGame.js): юниты (Objects), драконы (Dragons), камни (Objects),
огонь (FX), разовые эффекты (FX). Каждый — Actor + ImageComponent (`LoadFromImage(path)`), размер
актора задаётся из таблицы SPRITES (пиксельные размеры зашиты в JS-конфиг).

## Координаты

Мир = пиксели, центр экрана (0,0), y вверх, экран 1280×800 (FittedSize камера).
Поле 7×10, клетка 64: `worldX(col) = (col-3)*64`, `worldY(row) = -282 + row*64` (row 0 — низ).
Небо: rows 7..9. Линия фронта: y=134. Эти же константы — в C++ билдере и в JS (продублированы).

## Модель (DragonModel.js)

Состояние: `grid[col][row]` (юнит {cls,lvl,hp,maxHp,buff} | камень {hp}), `dragons[]`
({type,col,row,w,h,hp,armor,frozen}), `fires{col,row:turns}`, `castleHP`, `wave/maxWave`,
`boosters{bomb,freeze,swap}`, `freezeTurns`, мета: `coins, materials, barracksLvl, forgeLvl`,
`seed` (свой LCG — детерминизм для тестов), `state: 'castle'|'battle'|'victory'|'defeat'`.

Ход (`DoTurn()` вызывается после успешного действия игрока):
1. Gravity: юниты сдвигаются вверх в своей колонке (rows 0..6, камни не двигаются),
   потом спавн рекрутов L1 в пустые клетки row 0.
2. Юниты атакуют (танк — дракон на клетке выше; лучник — ближний дракон в колонке; маг — ближний
   в колонке ≤5 + сплэш соседям; лекарь — лечит+бафф соседей). Без цели — бьют камень в колонке.
3. Драконы (если не заморожены): дальнобойный стреляет по ближнему юниту в колонке ≤4; остальные
   бьют юнита прямо под собой; поджигатель раз в 2 хода жжёт клетку под собой; движение на 1 вниз,
   если свободно (босс 2×2 — обе клетки); выход за row 0 → breachDamage замку, дракон исчезает.
4. Огонь: урон стоящим, таймеры.
5. Волна: если драконов нет и волны остались — спавн следующей.
6. Проверка победы/поражения.

API модели: `StartBattle()`, `ClickCell(col,row)` → {ok, kind}, `BuyUnit(cls)`, `UseBomb(c,r)`,
`UseFreeze()`, `UseSwap(c1,r1,c2,r2)`, `CallNextWave()`, `UpgradeBarracks()`, `UpgradeForge()`,
`EndBattleToCastle()`. Уровень слияния ограничен `barracksLvl` (иначе heal+бафф). Кузница:
множитель урона по камням, ур.2 — снос случайного камня на старте боя.

## Вью/контроллер (DragonGame.js)

- `_view` — словарь id→{actor, target:{x,y}, …}; позиции твинятся (lerp) в Update.
- После каждого вызова модели — `SyncView()`:디создать недостающих, удалить мёртвых
  (`actor.Destroy()`), обновить позиции/спрайты (смена уровня юнита = LoadFromImage нового пути),
  SetFill у HP-баров, тексты Label'ов.
- Режимы клика: `normal` (клетка → merge), `bomb` (клик-прицел), `swap` (два клика). Правый клик
  или повторный клик по иконке — отмена.
- Экраны: `model.state`: Battle root enabled ⟺ battle/victory/defeat, Castle ⟺ castle.

## Тесты

- **GameTests (headless)**: `o2Scripts.Run(DragonModel.js)` + Eval-скрипты: детерминированные
  сценарии (merge/каскад/кап казарм, gravity+спавн, атаки всех классов, броня/дальнобойный/
  поджигатель/босс 2×2, бустеры, волны, победа/поражение, камни/кузница, экономика).
  Плюс тест, который собирает bootstrap-сцену билдером и сохраняет `DragonDefense.scn` в Assets
  (сцена без визуальных ассетов — только Bootstrap-актор; это "сцена кодом" по правилам скилла).
- **GameUITests (rendered)**: загрузка сцены, PumpFrames; проверка, что поле построено; клики
  AppTestDriver'ом (merge реального юнита, покупка, бомба), скриншоты в Work/ScreenShots.

## Ключевые грабли (из исследования API)

- JS: только методы (`GetTransform()`, `SetColor(...)`), поля-проперти недоступны; аргументы по
  умолчанию = 0/false — передавать все; `AddChild(a, idx)`; класс скрипта —
  `Name = class Name extends o2.Component {}`.
- Порядок отрисовки — слоями сцены (BG..UI), не drawDepth.
- Label под актором с нулевым боксом: offsetMin/Max = мировые координаты.
- Headless не трогает картинки/шрифты — модель отделена от вью.
