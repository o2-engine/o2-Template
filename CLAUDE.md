# o2-Template — Claude project memory

Shared rules for any Claude session in this repo. Auto-loaded. Commit changes here; private/host
notes stay in `~/.claude/...`, not in this file.

## Editor Actions refactor (ongoing in the o2 editor)

Every mutation of scene/assets/state in the o2 editor must flow through an `Editor::IAction`
(undo/redo), with the mutation owned by the action (runtime path == Redo path) and covered by tests.
The editor is mid-migration — old direct mutations and IAction code coexist. Migrate features one at
a time, only when the owner asks; don't refactor the rest speculatively.

## Build & test after every change

1. Build the affected target: `cmake --build --preset mac --target <Target> -j 8` (`windows` /
   `linux` on those hosts). Fix compile errors before reporting.
2. Run `ctest --test-dir build --output-on-failure -C Debug --parallel 4`. Pick the target you
   touched: `o2UtilTests` (value types), `o2SystemTests` (scene/assets, headless), `o2RenderTests`
   (render / UI widgets), `o2EditorTests` (editor, headless), `o2EditorUITests` (non-headless — for
   tests that build real property/viewer widgets, which headless can't), `GameTests` (headless game
   tests) / `GameUITests` (rendered: real window, cursor injection and screenshots via
   AppTestDriver).
3. Report done only on a green build + green run — never "should compile, tests unaffected".

For a reported bug, work test-first: write a failing test that reproduces it, fix until it passes,
then run the rest of the suite. Add a test for any new code path, or flag the gap.

Headless tests must not load scenes with visual assets (images/fonts) — there is no Render in
headless mode and `TextureRef` crashes. Scenes with visuals are covered by the rendered
`GameUITests`; headless suites test logic and visual-free scenes only.

### Batch test mode (default)

CTest registers one entry per gtest *suite*; the suite runs whole in one process
(`--gtest_filter=Suite.*`), so Application/render init is paid per suite, not per test case.
Entries are named `<Target>/<Suite>`: `ctest -R '^GameTests/'` runs one binary, `ctest -R '/Rotator$'`
one suite. Implemented in `o2/CMake/O2TestSuites.cmake` + `O2TestSuitesDiscovery.cmake` (discovery
at ctest startup); call sites use `o2_gtest_discover_tests(...)`. Configure with
`-DO2_TESTS_BATCH=OFF` for the classic one-process-per-test-case registration (names `Suite.Case`)
when hunting cross-test pollution. Consequence: tests of one suite share a process — clean up global
state (scene, subscriptions to `o2Scene`/global signals) via guards/destructors.

New tests go in the matching tier; shared helpers live in `o2/Tests/Sources/Support/`
(`SceneCleanGuard`, `TickFrame`/`TickFrames` in `Scene/SceneTestHelpers.h`).

## Image generation tools (CLI, `o2/Tools/ImageGen/`)

For generating game sprites/assets run the CLI scripts in `o2/Tools/ImageGen/` from the
terminal (model Gemini Nano Banana 2). Do NOT use them via MCP — the tool previews eat too
much context; run the scripts and inspect the output files with Read only when needed:

- `generate_image.py "prompt" --out img.png [--aspect 16:9] [--size 1K|2K|4K] [--ref style.png]`
- `edit_image.py input.png "instruction" --out img.png [--ref style.png]`
- `generate_transparent.py "prompt" --out sprite.png [--ref style.png]` — RGBA via
  white/black double render; fails on near-white subjects (key those out by flood fill).
- `extract_region.py atlas.png --rect x,y,w,h --out part.png [--transparent]`

Prompts in English. Details: `o2/Tools/ImageGen/README.md`. API key:
`o2/Tools/ImageGen/api_key.txt` (gitignored) or `GEMINI_API_KEY`.

## Use engine facilities first

Before writing any helper/subsystem, check that o2 doesn't already provide it — the engine has
tools for nearly everything (UI = Widget descendants via `o2UI` + styles, not custom sprite/text
components; particles, tweens, layouts, etc.). Full markdown documentation lives in `o2/Docs/en`
(and `ru`) — consult it first, then the sources.

## Communication with the contributor

On significant problems (blockers, broken assumptions) or when a real design choice comes up —
ask the contributor instead of silently picking an option.

## Comments

Default: no comment. No multi-line rationale/history/ABI essays above code — that goes in the PR. A
short single-line comment only when "why" is non-obvious (hidden invariant, workaround). Same for
tests: a one-line header at most.

Never write a comment that restates the code. If it mirrors the line (`value = nullptr; // clear it`),
delete it. Even a "why" comment must not narrate what the code does.

## Communication

Write the final summary at the end of work in Russian, concise. Record any persistent guidance the
contributor asks me to remember here in this file, not in host/private memory.

## Version control

Don't run `git commit` / `push` / `add` / `gh pr create` etc. by default — make changes and stop at
"files modified"; the contributor reviews and commits. Git authorization is per-session only, never
carried to future sessions.
