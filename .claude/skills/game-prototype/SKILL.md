---
name: game-prototype
description: Разработка прототипа игры на движке o2 — от концепта и GDD до генерации арта, сборки сцены, JS-логики и тестов. Использовать, когда просят сделать/собрать прототип игры (game prototype) в этом репозитории.
---

# Прототип игры на движке o2

Полная документация движка: `o2/Docs/ru/main.md` (архитектура — `o2/Docs/ru/Architecture/architecture.md`).
При вопросах по подсистеме — сначала доки, потом исходники. Ниже — краткая шпаргалка.

## Движок o2 — шпаргалка

Синглтоны-акцессоры: `o2Application`, `o2Scene`, `o2Assets`, `o2Render`, `o2Input`, `o2UI`,
`o2Sounds`, `o2Physics`, `o2Scripts`, `o2Time`, `o2Debug`, `o2FileSystem`, `o2Events`.
Умные указатели: `Ref<T>` / `WeakRef<T>`, фабрика `mmake<T>(...)`, каст `DynamicCast<T>`.

- **Сцена**: `Actor` — `mmake<Actor>(ActorCreateMode::InScene)`, `AddChild`, `GetChild("a/b")`,
  `AddComponent<T>()`, `transform->position/size/angle/scale`. `Component` — лайфсайкл
  `OnInitialized/OnStart/OnUpdate(dt)/OnDraw/OnEnabled/OnDisabled`. Свой компонент: `SERIALIZABLE(X)` +
  `CLONEABLE_REF(X)`, поля с `@SERIALIZABLE @EDITOR_PROPERTY` в комментарии; META генерирует CodeTool при сборке.
  Поиск: `o2Scene.FindActor("path")`, `FindActorByType<T>()`. Слои `SceneLayer`, порядок отрисовки — `drawDepth`.
  Загрузка сцены: `o2Assets.GetAssetRefByType<SceneAsset>("Main.scn")->Load()`; сохранение — `o2Scene.Save(path)`.
- **UI**: `Widget: Actor`, лэйаут `widget->layout->anchorMin/anchorMax/offsetMin/offsetMax`.
  Виджеты: `Button`, `Label`, `Image`, `Toggle`, `EditBox`, `DropDown`, `List`, `ScrollArea`, `Window`,
  `VerticalLayout`/`HorizontalLayout`/`GridLayout`, прогрессы/скроллбары. Создание со стилем:
  `o2UI.CreateWidget<T>("style")`, `o2UI.CreateButton(caption, onClick)`; стили — `ui_style.json` в ассетах.
- **Спрайты**: `ImageComponent` (наследует `Sprite`): `LoadFromImage(path)`, `SetColor`, `SetMode`, `SetFill`.
  Камера на сцене — `CameraActor`: `SetFittedSize`, `fillBackground`, `fillColor`.
- **Анимации**: `AnimationComponent` — `AddState(name, clip)`, `Play(name)`, `BlendTo(name, dur)`;
  клипы `AnimationClip`/`AnimationAsset`. Стейт-граф: `AnimationStateGraphComponent` +
  `AnimationStateGraphAsset` — `SetGraph(asset)`, `GoToState(name)`.
- **Частицы**: `ParticlesEmitterComponent` (`particlesSource`, `shape`, `Play()/Stop()`).
- **Звук**: `SoundComponent` / `SoundPlayer` — `SetSound(AssetRef<SoundAsset>)`, `Play()`, `volume`, `loop`.
- **Видео**: `VideoComponent`, хромакей-шейдер (ключ #00B140) — для сгенерированных анимаций.
- **Прототипы (префабы)**: `ActorAsset` (.proto) —
  `o2Assets.GetAssetRefByType<ActorAsset>("X.proto")->Instantiate()`.
- **Ассеты**: исходники в `Assets/` (+`.meta` рядом), собранные в `BuiltAssets/`.
  `o2Assets.GetAssetRefByType<T>(String("path"))`, `GetAssetsPath()`, `GetBuiltAssetsPath()`, `RebuildAssets()`.
- **Ввод**: `o2Input.IsKeyPressed/IsKeyDown`, `GetCursorPos()`, `IsCursorPressed/IsCursorDown`.
- **Точка входа**: `Sources/Game/GameApplication.cpp` — `GameApplication::OnStarted()`; здесь размер окна
  и загрузка стартовой сцены.

## JavaScript и ScriptableComponent

Движок JS — QuickJS (бекенды: JerryScript, BrowserJS для WASM). `o2Scripts` (`o2::ScriptEngine`):
`Parse`/`Run`/`Eval`, `GetGlobal()`. Скриптинг работает и в headless.

`ScriptableComponent` (`o2/Framework/Sources/o2/Scene/Components/ScriptableComponent.h`):
свойство `script` = `AssetRef<JavaScriptAsset>` (.js в `Assets/Scripts/`). При загрузке файл исполняется,
в глобале ищется класс **с именем, совпадающим с именем файла** (без расширения), конструируется инстанс.
Вызываются методы инстанса: `OnStart()`, `Update(dt)`, `OnEnabled()`, `OnDisabled()`.
`this._actor` — актор-владелец. Публичные поля инстанса сериализуются и видны в редакторе;
поля с префиксом `_` — приватные. Скелет (`Assets/Scripts/MyThing.js`):

```js
class MyThing extends o2.Component
{
    constructor()
    {
        super();
        this.speed = 2.0;   // сериализуемое, видно в редакторе
        this._time = 0;     // приватное
    }

    OnStart() { }

    Update(dt)
    {
        this._time += dt;
        this._actor.transform.SetScale2D(new Vec2(1, 1));
    }
}
```

Привязка из C++: `actor->AddComponent<ScriptableComponent>()->SetScript(o2Assets.GetAssetRefByType<JavaScriptAsset>("Scripts/MyThing.js"));`

В JS доступно то, что помечено в C++-заголовках атрибутом `@SCRIPTABLE` (комментарий над методом →
CodeTool генерирует `SCRIPTABLE_ATTRIBUTE()` в META при сборке). Базовые скрипты рантайма:
`o2/Framework/Assets/Scripts/` (`o2.js`, `Component.js`, `Math.js` — `Vec2` и т.п.).

## Генерация графики (MCP `imagegen`, Gemini Nano Banana 2)

Ключ API уже лежит в `o2/Tools/ImageGen/api_key.txt` (gitignored; либо `GEMINI_API_KEY`). Инструменты
(детали — `o2/Tools/ImageGen/README.md`):

- `generate_image(prompt, out_path, aspect?, size?, ref_paths?)` — text-to-image, стиль через `ref_paths`.
- `edit_image(image_path, prompt, out_path, ref_paths?)` — точечная правка.
- `generate_transparent_image(...)` — RGBA-спрайт (не работает для светлого на белом — тогда белый фон + кейинг).
- `extract_region(image_path, rect=[x,y,w,h], out_path, transparent?)` — вырезать спрайт из листа/концепта.
- `generate_video(...)` — Veo, анимации; режим `--green` для хромакея под `VideoComponent`.

Промпты — на английском. После каждой генерации **смотри картинку сам** (Read) и проверяй соответствие.
Пиксельные референсы перед подачей апскейль плавно, иначе модель копирует пикселизацию.

## Тесты

- **GameTests** (`Sources/GameTests/`) — headless: логика, скрипты (`o2Scripts.Eval`), без визуальных
  ассетов (`TextureRef` крашится без рендера). Акторы создавать руками, кадры — `TickFrame`/`TickFrames`,
  очистка — `SceneCleanGuard` (хелперы: `#include "Scene/SceneTestHelpers.h"` из `o2/Tests/Sources/Support/`).
- **GameUITests** (`Sources/GameUITests/`) — реальное окно и рендер. `o2::AppTestDriver` (статические методы,
  экранные координаты, центр окна = (0,0), y вверх): `PumpFrames(n)`, `Wait(sec)`, `MoveCursor`, `Click`,
  `Drag`, `PressCursor/ReleaseCursor`, `TakeScreenshot()`, `SaveScreenshot(path)` (папки создаёт сам).
  Клавиатура — `o2Input.OnKeyPressed(VK_...)`. В `SetUp`: задать размер окна, поднять сцену, `PumpFrames(5)`;
  в `TearDown`: `o2Scene.Clear(true); o2Scene.UpdateDestroyingEntities(); AppTestDriver::PumpFrames(2);`.
- Новые .cpp подхватываются GLOB'ом после реконфигурации cmake. Suite = один процесс — чистить глобальное состояние.
- Сборка/запуск: `cmake --build --preset mac --target GameTests -j 8` (и `GameUITests`),
  `ctest --test-dir build --output-on-failure -C Debug --parallel 4 -R '^GameTests/'` (или `'^GameUITests/'`).

## Порядок разработки прототипа

1. Создать директорию `Work/` в корне репо (очистить, если уже была).
2. Создать `Work/worklog.md` — лог проделанной работы, дописывать по ходу.
3. Создать `Work/GDD.md` — гейм-дизайн документ, дописывать по ходу.
4. Создать папки: `Work/Art/` (исходники генерируемого арта), `Work/ScreenShots/` (скриншоты игры при
   разработке, с порядковой нумерацией), `Work/Concepts/` (концепты — сгенерированные и переданные).
5. Изучить описание прототипа, скриншоты и референсы, если переданы (сложить в `Work/Concepts/`).
6. Дописать недостающие части GDD в `Work/GDD.md`.
7. Сгенерировать концепт основного геймплея, затем — остальные экраны и UI **в едином стиле**, передавая
   основной концепт (и референсы) через `ref_paths`. После каждой генерации визуально проверять результат.
8. По картинкам продумать архитектуру игры: игровое поле, бекграунды, UI, эффекты — какие элементы нужны.
9. Нарезать графику на спрайты через `extract_region`. Каждый спрайт открыть и визуально проверить.
10. Написать python-скрипт композинга спрайтов в теоретический игровой экран (просто рендер спрайтов в
    нужных местах). Сравнить результат с концептами; при больших расхождениях перегенерировать спрайты.
    Повторить для всех экранов.
11. Собрать логику игры; архитектуру описать в файл (`Work/Architecture.md`), покрыть весь геймплей.
12. Писать тесты на геймплей (GameTests + GameUITests), запускать игру, проверять по скриншотам, что всё
    работает как задумано. Скриншоты складывать в `Work/ScreenShots/` по порядку.
13. В конце сгенерировать HTML-отчёт о проделанной работе: все этапы, сложности и пути решения.

## Правила работы с движком и построения игры

- Использовать возможности движка: UI (`Widget` и наследники), сцена и компоненты, частицы, спрайты,
  `VideoComponent`, анимации и стейт-графы, звуки, JS-скрипты, прототипы.
- Точка входа — сцена **bootstrap**, которая инициализирует всю игру. При старте из редактора игра
  работает корректно; приложение Game (`GameApplication::OnStarted`) запускает эту же сцену.
- Всё строить на сцене: читаемая, удобная иерархия акторов. Повторяющиеся объекты — через прототипы (префабы).
- Разделять слои логики и отображения.
- Основной язык — JavaScript (`ScriptableComponent`). C++ — для сложной логики и того, что требует
  производительности.
- **Сцену и прототипы собирать кодом** (API движка + `o2Scene.Save` / сохранение ассета), не генерировать
  конечные файлы ассетов (.scn/.proto) руками.
- Багфиксы в движке o2 — можно по необходимости. Доработки/новая функциональность движка — только по
  согласованию с владельцем. Новые JS-биндинги — добавлять атрибут `@SCRIPTABLE` в комментарий метода в
  заголовке (META перегенерирует CodeTool при сборке).
- Сборка и тесты после каждого изменения — по общим правилам репо (см. `CLAUDE.md`): зелёный билд и
  зелёные тесты, иначе не рапортовать о готовности.
