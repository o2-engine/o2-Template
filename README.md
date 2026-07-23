# o2-Template

Minimal project template for the [o2 engine](https://github.com/zenkovich/o2). Fork or copy it to
start a new game: the engine is wired in as a submodule, CMake builds the game, the editor and the
tests, and the sample scene shows a deferred 3D layer with a 2D overlay on top.

## Getting started

```sh
git clone --recursive https://github.com/zenkovich/o2-Template.git
cd o2-Template

# Configure + build (presets: mac / windows / linux, *-release variants)
cmake --preset mac
cmake --build --preset mac -j 8

# Run
Bin/Mac/Editor   # the o2 editor with the project opened
Bin/Mac/Game     # the game itself (loads Assets/Main.scn)
```

If you cloned without `--recursive`, run `git submodule update --init --recursive`.

`GenerateProject.command` / `GenerateProject.bat` open a small GUI wrapper around the same CMake
presets for generating Xcode / Visual Studio / CLion projects.

## Tests

```sh
ctest --test-dir build --output-on-failure -C Debug --parallel 4
```

Targets: `o2UtilTests`, `o2SystemTests`, `o2RenderTests`, `o2EditorTests`, `o2EditorUITests`
(engine), `GameTests` (headless game tests), `GameUITests` (rendered: real window, screenshots).
Run a single binary with `ctest -R '^GameTests/'`.

## Project layout

- `Assets/` — raw assets: `Main.scn` (sample scene: 3D primitives, lights, perspective camera with
  the deferred pipeline + 2D sprites and a label), sprites, `debugFont.ttf`, `Fox.glb` (glTF sample
  model used by tests; from [glTF-Sample-Models](https://github.com/KhronosGroup/glTF-Sample-Models),
  CC-BY 4.0 by PixelMannen). Built by `AssetsBuilder` into `BuiltAssets/<Platform>/`.
- `Sources/Game/` — game code (`GameLib`): `GameApplication`, sample `RotatorComponent`,
  `GameLib.cpp` reflection aggregator (maintained by the o2 CodeTool).
- `Sources/Editor/` — editor-side extensions (`EditorLib`): custom property viewers etc.
- `Sources/GameTests/`, `Sources/GameUITests/` — game test suites (Google Test).
- `Platforms/` — per-platform entry points: Windows/Linux/Mac (`Windows/`), `iOS/`, `Android/`
  (Gradle project), `WebAssembly/` (shell page + dev server).
- `o2/` — the engine submodule.

## Platforms

Desktop (Windows, Linux, macOS) builds the game, the editor and the tests. Cross targets build the
game only and use pre-built host assets: `cmake --preset ios` / `ios-sim`, `wasm` (needs emsdk;
serve with `Platforms/WebAssembly/serve.py`), Android via the Gradle project in
`Platforms/Android/`.

## Making it yours

1. Rename the `Game` target in `CMakeLists.txt` and `CMakePresets.json` (iOS/WASM/Android build
   presets reference it by name), the bundle id `com.o2.template` and the Android package.
2. Replace `Assets/Main.scn` with your scenes; the boot scene is loaded in
   `Sources/Game/GameApplication.cpp`.
3. Add game components next to `RotatorComponent` — the CodeTool picks them up and registers the
   reflection automatically during the build.
