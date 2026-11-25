# Editor framework prototypes (decision D-003)

Two throwaway mini-editors with the same features, used to choose the editor's UI
framework. **This is not engine code**; when D-003 is decided, the winner's lessons
move into the real `editor/`, and this folder may be deleted.

```
proto_core/     shared, UI-independent: model, undo commands, PNG import,
                JSON save/load, code generator, iso/node math, tests
imgui_editor/   Dear ImGui + SDL3 prototype
qt_editor/      Qt 6 Widgets prototype
third_party/    ImGui, nlohmann/json, doctest, stb_image (+ SDL3, downloaded)
sample_assets/  robot.png, coin.png (original art, made by tools/make_sample_sprites.py)
sample_scene.json
tools/          measure.ps1 (performance), count_lines.py, make_sample_sprites.py
```

Full comparison and measured results: `docs/EDITOR_FRAMEWORKS.md`.

## Build (PowerShell, from the repository root)

Needs: Visual Studio 2022 Build Tools (C++), CMake, and Qt 6.8 at
`C:\Qt\6.8.3\msvc2022_64` (installed with aqtinstall in session 3).

```powershell
# 1. Download SDL3 (only once; about 24 MB, not stored in Git)
powershell -ExecutionPolicy Bypass -File prototypes\fetch_deps.ps1

# 2. Configure and build both prototypes (about 2 minutes the first time)
cmake -S prototypes -B build/prototypes -DPROTO_BUILD_IMGUI=ON -DPROTO_BUILD_QT=ON -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build/prototypes --config Release

# 3. Run the shared-core tests
build\prototypes\proto_core\Release\proto_core_tests.exe
```

## Run

```powershell
build\prototypes\imgui_editor\Release\imgui_editor.exe --open prototypes\sample_scene.json
build\prototypes\qt_editor\Release\qt_editor.exe --open prototypes\sample_scene.json
```

Leave out `--open ...` to start with an empty scene. The first launch of a newly
built program can take a few seconds while Avast scans it.

Other options (used by `tools/measure.ps1`): `--startup-probe`,
`--benchmark-drag <seconds>`, and for Qt only `--opengl`.

## Omar's hands-on test checklist

Do every step in **both** prototypes. Write down anything that feels slow, confusing
or broken: those impressions matter as much as the measured numbers.

| # | Do this | You should see |
|---|---|---|
| 1 | Start with `--open prototypes\sample_scene.json` | Player (robot), two coins and an "Empty Box" inside a yellow 240x160 frame |
| 2 | Drag the Hierarchy, Inspector and Assets panels by their title bars to other places; tab two panels together; float one | Panels dock, tab and float |
| 3 | Click "Player" in the Hierarchy | Yellow outline around the robot; Inspector shows Name, X/Y/Z, Sprite Renderer, Collider |
| 4 | Drag the robot around the Scene view | It follows the mouse; X/Y in the Inspector update live |
| 5 | Ctrl+Z once | The robot jumps back to where the *whole* drag started (one undo step per drag) |
| 6 | Ctrl+Y | The drag is redone |
| 7 | Tick "Snap to 8 px grid" and drag again | A grid appears; the robot moves in 8-pixel steps |
| 8 | Type a new X in the Inspector, press Enter | Robot moves; Ctrl+Z undoes it |
| 9 | Rename: ImGui - edit Name in the Inspector; Qt - double-click the row (or Inspector Name) | Hierarchy and generated source show the new name; Ctrl+Z restores it |
| 10 | "+ Create", then Delete (button or Del key) | A 16x16 box appears, then disappears; Ctrl+Z brings it back in the same list position |
| 11 | Import PNG (Assets panel), choose `prototypes\sample_assets\coin.png` | A second coin asset appears with "8x8, 4 GBA colors" |
| 12 | Drag the coin asset onto "Empty Box" (in the Hierarchy *and* in the Scene view) | Empty Box now shows a coin; Inspector gains a Sprite Renderer |
| 13 | Drag an asset onto empty space in the Scene view | A new object is created there |
| 14 | Inspector: Add Component > Health; change Max Health; remove it | Section appears / value changes / section disappears; each step undoable |
| 15 | Inspector: change Sprite via the drop-down; also drop an asset on the drop-down | Sprite changes |
| 16 | Look at Generated Source while you edit | `gen_scene_main.cpp` text updates (names, positions, comments) |
| 17 | File > Save Scene As..., then File > New Scene, then File > Open Scene... | The saved scene comes back exactly; the same file opens in the *other* prototype too |
| 18 | Isometric Grid tab: left-click tiles, right-click tiles, then Ctrl+Z | Columns rise and fall; hovering highlights the tile you see; undo works |
| 19 | Node Graph tab: drag a node by its title; drag from "Pressed" to "Run"; drag the wire off "Run" | Nodes move; wires connect and disconnect (not undoable yet, in both) |
| 20 | Qt only: open the Undo History panel and click an older line | The scene jumps back to that point |

Things to judge as you go:
- Which one *feels* more responsive (dragging, typing)?
- Which one is nicer to look at and to arrange?
- Which code would you rather read and extend? Compare
  `imgui_editor/editor_app.cpp` (drawInspector) with `qt_editor/inspector_panel.cpp`.
