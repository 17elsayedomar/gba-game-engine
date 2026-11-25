# Editor UI Framework Comparison: Dear ImGui vs Qt 6 (decision D-003)

**Status: D-003 is NOT decided.** Two working prototypes exist and were measured on
Omar's laptop. Omar tests both by hand (checklist in `prototypes/README.md`), then we
discuss. Only after that discussion is D-003 finalized.

Last updated 2026-11-25. Earlier versions of this file also compared Avalonia (C#)
and Electron/Tauri (web); Omar narrowed the choice to ImGui and Qt, so those
sections were removed (still in Git history).

---

## 1. What was built

Two small, independent editor prototypes with the **same features**, both built on
the **same UI-independent C++ library** (`prototypes/proto_core`):

```
prototypes/
  proto_core/    model, components, undo commands, PNG import, JSON save/load,
                 C++ code generator, isometric + node-graph math, 13 tests
                 -> knows nothing about ImGui or Qt
  imgui_editor/  Dear ImGui 1.93 (docking) + SDL3 3.4   (3 files)
  qt_editor/     Qt 6.8 LTS Widgets                     (13 files)
```

Neither prototype contains any scene logic of its own. Both create the same
`proto_core` command objects for every edit, so undo/redo, saving and the generated
source are identical. This is the separation the real editor must keep whatever we
choose: **the UI only draws and turns clicks into commands.**

| # | Feature | Dear ImGui prototype | Qt prototype |
|---|---|---|---|
| 1 | Dockable hierarchy | `ImGui::Begin("Hierarchy")` in a DockSpace | `QDockWidget` + `QTreeView` + `SceneTreeModel` (`QAbstractItemModel`) |
| 2 | Dockable Inspector | Widgets generated from component descriptors each frame | Widgets generated from descriptors, rebuilt or updated on change |
| 3 | Viewport with 240x160 boundary | Draw list on an invisible button | `QGraphicsView` / `QGraphicsScene`, 1 unit = 1 GBA pixel |
| 4 | Import PNG | SDL3 native file dialog (async) + `proto_core::importPng` | `QFileDialog` + `proto_core::importPng` |
| 5 | Create / select / rename / delete | Selectable rows, InputText, Delete key | Tree rows (F2/double-click rename via `setData`), Delete key |
| 6 | Drag sprites in viewport | Mouse drag sends `SetPositionCommand` | Mouse drag sends `SetPositionCommand` (Qt items follow the model) |
| 7 | Edit X/Y/Z | `DragInt` | `QSpinBox` |
| 8 | Drag asset onto object | ImGui drag-drop payload | MIME drag-drop (`application/x-gbe-asset-id`) onto tree row, viewport or Inspector |
| 9 | Add/remove component | Popup menu / header close button | `QMenu` / "Remove" button |
| 10 | Undo/redo moves & properties | `proto_core::CommandHistory` | `QUndoStack` wrapping the same commands (+ free **Undo History** panel) |
| 11 | Save/load scene | `proto_core` JSON | `proto_core` JSON (identical files) |
| 12 | Generated source panel | Custom highlighter + list clipper | `QPlainTextEdit` + `QSyntaxHighlighter` |
| 13 | Isometric tile grid | Draw list polygons | `QPainter` polygons |
| 14 | Node graph (2 nodes, wires) | Draw list + Bezier curves | `QPainter` + `QPainterPath` |

Status of the 14 features in both prototypes: **Implemented, untested** (manual
testing is Omar's checklist). What *was* checked automatically: both start, load the
sample scene, render it (screenshots inspected), and run a scripted 60-moves-per-second
drag through the real command path without errors. The shared core is **Automatically
tested** (13 doctest cases, 199 assertions, all passing).

Known gaps, equal in both: node-graph edits (moving nodes, re-wiring) are not
undoable yet; only one scene; no multi-select.

---

## 2. Measured results (Omar's laptop)

Machine: Intel Core i5-7300U (2 cores / 4 threads, 2017), 7.8 GB RAM, Intel HD
Graphics 620, Windows 10 Pro, display at **150 % scaling**, MSVC 2022, Release builds.
Script: `prototypes/tools/measure.ps1` (results in `build/measurements.json`).
Measured 2026-11-25. Each startup number is the median of 5 runs after a discarded
warm-up run (Avast scans every newly built `.exe` on first launch).

| Measurement | Dear ImGui + SDL3 | Qt 6 Widgets | Notes |
|---|---|---|---|
| Startup to first full frame | **391 ms** (246–624) | **416 ms** (365–485) | Practically the same |
| Idle CPU (scene open, no input) | **0.0 %** | **0.0 %** | ImGui sleeps after 3 quiet frames; Qt sleeps by design |
| CPU while dragging (60 moves/s) | **16–20 %** of one core | **28–30 %** (Generated Source refreshed ≤10×/s) | Qt refreshing the source on *every* move: **47–55 %** |
| Memory, working set (idle) | **27.5 MB** | **40.0 MB** | Private memory: 10.0 vs 14.4 MB |
| Memory while dragging | 27.6 MB | 41.5 MB | No growth in either |
| Clean build (incl. proto_core) | **58 s** | **76 s** | Configure: 14 s each |
| Rebuild after editing one UI file | **7.7 s** | **9.3 s** | `editor_app.cpp` vs `inspector_panel.cpp` |
| `.exe` size | 0.90 MB | 0.46 MB | ImGui is compiled in; Qt is in DLLs |
| Files a user needs to run it | **3.6 MB**, 2 files | **41.0 MB**, 21 files | Qt could be trimmed to about 23 MB (see below) |
| SDK install on this PC | ~30 MB (sources + SDL3 dev package) | **1,959 MB** (Qt 6.8.3 MSVC, via aqtinstall) | |
| UI code (lines of code, excluding comments/blank lines) | **975** in 3 files | **1,307** in 13 files | Shared core: 1,104 lines, used by both |

Extra Qt experiment, `--opengl` (Scene viewport drawn by `QOpenGLWidget` instead of
the CPU): startup **806 ms**, idle memory **149 MB**, CPU while dragging higher than
without it. For our 2D 240x160 viewport, OpenGL makes Qt *worse* on this Intel GPU;
plain `QGraphicsView` is the right choice.

**Why the Qt folder is 41 MB:** Qt6Core + Qt6Gui + Qt6Widgets + the Windows platform
plugin are about 22 MB. `windeployqt` also copies a Direct3D shader compiler
(`dxcompiler.dll`, 12 MB), the network/TLS modules and image plugins we don't use.
Excluding those is possible but takes trial and error.

**What the drag numbers mean, in plain words:** in ImGui, a drag redraws the window
60 times a second, but every redraw is cheap. In Qt, each movement sends a "scene
changed" signal, and every panel updates its own widgets. The first Qt version
replaced the whole generated-source document on every movement (that meant
re-highlighting every line), which cost about 20 % of a core by itself. Limiting
that panel to 10 refreshes per second fixed most of it. That kind of tuning is
normal, everyday work in retained-mode UIs. In ImGui, the list clipper skipped the
off-screen lines automatically.

Not measured: GPU load, battery drain, behavior with thousands of objects (both
prototypes were only tested with a handful).

---

## 3. The core difference, shown with the same feature

The X/Y/Z fields in the Inspector, taken from the two prototypes (shortened):

**Dear ImGui (immediate mode):** this code runs every frame. There are no widget
objects; the field shows `object->x` *because this line says so this frame*.

```cpp
int values[3] = {object->x, object->y, object->z};
for (int i = 0; i < 3; ++i) {
    bool changed = ImGui::DragInt(labels[i], &values[i]);
    if (ImGui::IsItemActivated()) positionSession_ = nextSession_++;  // new undo step
    if (changed)
        execute(std::make_unique<SetPositionCommand>(object->id, values[0], values[1],
                                                     values[2], positionSession_));
}
```

**Qt (retained mode):** the spin box is an object that keeps its own number. We
need (1) code to create it, (2) a connection for user edits, and (3) an *updater*
that copies the model's value back into it after an undo, a drag, or loading a file.
While doing (3), signals must be blocked, or the update would look like a user
edit and create a new command (an endless loop).

```cpp
auto* spin = new QSpinBox;                                     // (1) create once
connect(spin, &QSpinBox::valueChanged, this, [=](int value) {  // (2) user edits
    ... context_->execute(std::make_unique<SetPositionCommand>(...));
});
updaters_.push_back([=] {                                      // (3) model -> widget
    QSignalBlocker block(spin);
    if (spin->value() != current) spin->setValue(current);
});
```

The InspectorPanel also has to decide when to **rebuild** all of its widgets
(another object selected, a component added) and when to **update** them in place
(a value changed), so that the field you are typing in isn't destroyed mid-edit.

**Bugs found while writing the prototypes,** a good sample of each style's typical problems:

| Qt (keeping copies in sync) | ImGui (doing everything yourself) |
|---|---|
| A model reset briefly reported "nothing selected", which cleared the editor's selection | Display scaling (150 %) is not automatic: text and the viewport were too small until scaled manually |
| Merged undo commands could send two "changed" signals | The built-in font is a small pixel font; Segoe UI had to be loaded |
| Dock sizes were ignored until the window had been shown | SDL file dialogs answer on another thread, so results need a mutex |
| Generated-source refresh was too expensive during drags | (none comparable) |

---

## 4. Qt facilities, examined

| Qt facility | Used in the prototype? | What it gives us | Caveats found |
|---|---|---|---|
| `QGraphicsView` / `QGraphicsScene` | Yes: Scene viewport | Item-based 2D canvas: zoom by transform, scrolling, hit-testing, items that stay a fixed size on screen (labels), and a spatial index for thousands of items. The **Tiled** map editor is built on it. | We kept the model as the source of truth (items don't move themselves), which needs a `sync()` step |
| `QOpenGLWidget` | Yes, optional (`--opengl`) | GPU drawing for the viewport, custom shaders | +390 ms startup, +110 MB memory on this Intel GPU. Not useful at our scale |
| `QTreeView` + `QAbstractItemModel` | Yes: Hierarchy | Real trees, lazy loading, several views of one model, drag/drop, in-place rename, keyboard navigation | Lots of required methods (`index`, `parent`, `rowCount`...); a full reset loses scroll and selection (incremental insert/remove signals are more work) |
| `QDockWidget` | Yes: all side panels | Floating, tabbing, `saveState()`/`restoreState()` layouts | Docks surround one fixed central widget (the Scene/Iso/Graph tabs can't be docked). Fully dockable layouts need the third-party Qt Advanced Docking System (LGPL) |
| `QUndoStack` (+ `QUndoView`) | Yes, wrapping core commands (~20-line adapter) | Undo/Redo actions that enable themselves and say "Undo Move Player"; clickable history; macros for compound edits; "clean" state for the unsaved-changes marker | None. This was the nicest part of Qt |
| `QProcess` | Not yet | Run `make`, devkitARM tools and mGBA; read their output through signals without threads | ImGui side would use SDL3's `SDL_CreateProcess` or Win32 pipes plus a thread |
| `QImage` | Yes: RGBA to pixmap | Pixel access, format conversion, PNG load/save; with `QPainter`, a ready canvas for a sprite editor | We still decode PNGs in the core (stb_image) so both editors agree |
| `QPainter` | Yes: iso grid, node graph | Antialiased paths, polygons, text | About the same effort as ImGui's draw list |
| `QSyntaxHighlighter` | Yes: Generated Source | Incremental per-line coloring, plus `QPlainTextEdit` = a real text editor (selection, copy, scrolling, undo) | Replacing the whole document is expensive; must be throttled |

---

## 5. Dear ImGui: what's built in, what needs a library, what we write ourselves

| Need | ImGui answer | License |
|---|---|---|
| Docking, tabs, multi-viewport windows | Built in (docking branch, used) | MIT |
| Tables, trees, drag-drop, popups, menus | Built in (used) | MIT |
| Node editor | **imnodes** or **imgui-node-editor**; the prototype draws its own (~110 lines) | MIT |
| Source code editor | **ImGuiColorTextEdit** (maintained forks exist); the prototype has a read-only highlighter | MIT |
| Graphs for the profiler | **ImPlot** (excellent) | MIT |
| Memory / VRAM viewer for emulator debugging | **imgui_memory_editor** | MIT |
| Timeline | **ImSequencer** (part of ImGuizmo) or custom | MIT |
| Undo/redo | None; our `CommandHistory` (~60 lines, already written and tested) | ours |
| Tree models for huge hierarchies | None; loop over the data, use `ImGuiListClipper` for long lists | ours |
| File dialogs | SDL3 native dialogs (used) | zlib |
| Run external programs | SDL3 `SDL_CreateProcess` | zlib |
| Text input for non-Latin languages (IME), screen readers | Limited | |

Qt equivalents for comparison: node editor QtNodes (BSD); code editor
`QPlainTextEdit` (built in) or KSyntaxHighlighting (MIT); plots **Qt Charts is
GPL/commercial only, not LGPL**, so for profiler graphs we'd use custom
`QPainter` code or QCustomPlot (GPL).

---

## 6. Long-term editor needs

| Need | ImGui | Qt | Comment |
|---|---|---|---|
| Complex hierarchies (parent/child, 1000s of objects) | Good with clipping; we write expand/collapse state | **Very good** (model/view) | Qt's biggest structural advantage |
| Dynamic inspectors from component metadata | **Very good**: one loop over descriptors | Good: rebuild/update logic | Measured: ImGui version is shorter and simpler |
| Prefabs, drag-drop references | Good | Good | Both proved drag-drop in the prototype |
| Undo/redo | Good (core command classes) | **Very good** (`QUndoStack` + history view) | Same core commands either way |
| Sprite/bitmap editing | **Very good**: textures + draw list, quick iteration | Good: `QImage` + `QPainter` | Both custom code; GBA palette logic lives in the core |
| Isometric tilemap editing | **Very good** | Good (`QGraphicsView` scales well) | Prototype iso grids are nearly identical |
| Animation timelines | Good (ImSequencer or custom) | Good (custom `QGraphicsView`) | Custom work in both |
| State machines, visual scripting | **Very good** (imnodes / imgui-node-editor) | Good (QtNodes or custom) | |
| Viewing generated source | Good (ImGuiColorTextEdit) | **Very good** (`QPlainTextEdit`) | |
| Editing user scripts in the editor | Fair | **Good** | Most developers will use VS Code / Programmer's Notepad anyway; "open in external editor" is a few lines in either |
| Emulator integration (mGBA) | **Very good**: frame as texture, memory viewer, runs in the same loop | Good: `QProcess` + image display | Both can launch mGBA; embedding libmgba is easier in a game-loop app |
| Profiling views | **Very good** (ImPlot) | Fair (Qt Charts license; custom painting) | |
| Background asset processing / compilation | Our own worker thread + result queue (same pattern as the file dialogs) | `QThreadPool`, `QProcess`, signals across threads | Qt makes this tidier |
| Polished, "native" look | Fair (themable, looks like a tool) | **Good** | Matters if non-programmers will use it |

---

## 7. Evaluation criteria

| Criterion | Dear ImGui + SDL3 | Qt 6 Widgets |
|---|---|---|
| Readability | One file reads top to bottom like the screen | Familiar OOP; logic spread over 13 files and signal connections |
| Complexity | Low; state lives in the scene plus a few UI variables | Medium; refresh/rebuild/sync rules, signal blocking, model indexes |
| Dependency count | 2 (ImGui, SDL3), both vendored/downloaded | 1 big SDK (Qt), 2 GB install, `moc` build step |
| Framework-specific code | 975 lines | 1,307 lines |
| Development effort (these prototypes) | Lower; 3 ImGui-specific issues fixed | About 35 % more code; 4 Qt-specific issues fixed (section 3) |
| Startup / idle CPU | ~0.4 s / 0 % | ~0.4 s / 0 % |
| CPU during interaction | **Lower** (16–20 %) | 28–30 % after tuning |
| Memory | **27 MB** | 40 MB |
| Build time | **58 s** clean, 7.7 s incremental | 76 s clean, 9.3 s incremental |
| Distribution size | **3.6 MB** | 41 MB (about 23 MB trimmed) |
| Responsiveness (felt) | Pending Omar's test | Pending Omar's test |
| Ease of adding components | **Very easy** (descriptor + nothing else) | Easy (descriptor; Inspector code is generic) |
| Ease of undo/redo | Easy (core commands) | **Easiest** (`QUndoStack`, history panel free) |
| Graphics tools suitability | **Excellent** | Good |
| Licensing | MIT + zlib: no obligations beyond keeping notices | LGPLv3: link dynamically (we do), ship license texts, allow users to replace the Qt DLLs; some modules (Qt Charts) GPL-only |
| Maintainability | Small and explicit; we own more widgets | Mature, well-documented framework; more boilerplate per feature |
| Educational value | Teaches the "game loop" way of thinking used by the GBA runtime itself | Teaches industry-standard desktop patterns (model/view, signals/slots) |

---

## 8. Current leaning (to discuss, not a decision)

The measurements and the code both favor **Dear ImGui + SDL3**:
- It is lighter on this 8 GB laptop: about 2/3 of the memory, about 60 % of the
  CPU while dragging, and 1/11 of the distribution size.
- It needed about 25 % less code, with fewer "keep two copies in sync" bugs.
- The editor's hard parts are custom canvases: viewport, sprite and tile painters,
  isometric view, node graphs, timelines, emulator view. Those are where ImGui is
  strongest and where Qt gives the least help.
- It runs the same frame-by-frame loop as a GBA game, which suits the learning goal.

Qt's real advantages are `QUndoStack`, model/view trees for very large
hierarchies, full text editing, background-task plumbing, and a more polished look.

**What would change the leaning:**
- Omar finds the ImGui prototype's look or feel unpleasant in daily use.
- Editing code *inside* the editor becomes a core requirement.
- Hierarchies with thousands of nested objects, and the polish that brings, matter
  more than the custom tools.

**Questions for our discussion after Omar's test:**
1. Which one *felt* better: dragging, typing in fields, docking panels around?
2. Does the "tool-like" ImGui look bother you, or is it fine for a game-dev tool?
3. Do you want to write game code inside our editor, or in your usual editor?
4. Did you notice any lag or bugs in either one?
