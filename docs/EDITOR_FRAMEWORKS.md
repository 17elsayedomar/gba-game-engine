# Editor UI Framework Comparison (for decision D-003)

Written 2026-11-24 for Omar. Numbers marked *(typical)* are general experience, not
measured on this machine; a hands-on prototype would measure them.

## 1. What the editor needs from a UI framework

Most of our editor is **not** buttons and forms. The hard parts are:

1. **Big custom canvases** — scene viewport, tilemap painter, pixel editor, isometric
   view. These are drawn by our own code no matter which framework we pick.
2. **Inspector / property panels** — many small fields generated from component descriptors.
3. **Node graphs** — state machines and visual scripting (Phases 4 and 8).
4. **Docking windows** — rearrange panels like Unity/Godot.
5. **Talking to the C++ core** — project model, asset pipeline, generator, build.

## 2. The four realistic options

### A. C++ + Dear ImGui (docking branch) + SDL3

**How it thinks: "immediate mode".** Every frame (60 times a second) your code
re-describes the whole UI from your data. There is no separate "UI copy" of the data
to keep in sync.

```cpp
// Inspector for the selected GameObject. Runs every frame.
ImGui::Begin("Inspector");
ImGui::InputText("Name", &object.name);          // edits object.name directly
if (ImGui::DragInt2("Position", object.position)) // returns true when changed
    commands.push(MoveObject{object.id, object.position});
ImGui::End();
```

- **Strengths:** same language as runtime and core — the editor can *link* the core
  libraries and even the runtime's math/collision code; tiny and fast *(typical:
  ~10–40 MB RAM)*; docking, drag-and-drop, tables, multi-viewports built in; easy to
  debug because there's no hidden UI state; MIT license; used by a lot of custom game
  engine editors and studio tools.
- **Weaknesses:** looks like a "tool", not a polished app (themable, though);
  forms, text boxes and accessibility are basic; you design the layout in code (no
  visual designer); complex widgets (tree with rename, multi-select) need some care.
- **Node graphs:** imgui-node-editor (MIT), imnodes (MIT).
- **Install:** a few MB of vendored source; builds with the MSVC + CMake already here.

### B. C# + Avalonia (with CommunityToolkit.Mvvm and Dock)

**How it thinks: "retained mode + data binding".** You describe the window once (in
XAML, an HTML-like language) and *bind* fields to properties; Avalonia updates the
screen when the property changes.

```xml
<!-- Inspector.axaml -->
<TextBox Text="{Binding Name}" />
<NumericUpDown Value="{Binding X}" />
```
```csharp
// GameObjectViewModel.cs
public partial class GameObjectViewModel : ObservableObject {
    [ObservableProperty] private string name = "";
    [ObservableProperty] private int x;
}
```

- **Strengths:** polished, modern-looking desktop UI; very productive for forms,
  trees, lists and dialogs; good text editing (AvaloniaEdit); cross-platform; MIT.
- **Weaknesses:** a **second language** (C#) and a new pattern (MVVM: Model–View–
  ViewModel) to learn; the C++ core must be either rewritten in C# or called through
  a C interface / the CLI as a separate process; can't directly share runtime C++ code
  for previews; heavier *(typical: ~80–200 MB RAM)*.
- **Node graphs:** NodeEditor for Avalonia (MIT).
- **Install:** .NET 8+ SDK (only the .NET 7 runtime is on this machine).

### C. C++ + Qt 6 (Widgets)

**How it thinks: retained mode with signals and slots.**

```cpp
auto* xField = new QSpinBox(this);
connect(xField, &QSpinBox::valueChanged, this, [this](int v) { setObjectX(v); });
```

- **Strengths:** the most complete desktop toolkit; native look; docking built in;
  same language as the core. The **Tiled** map editor is built with Qt — a proven fit
  for tile editing.
- **Weaknesses:** multi-GB install; LGPL obligations (dynamic linking, ship Qt's
  source offer) or paid license; its own build step (`moc`) and conventions; large to learn.
- **Node graphs:** QtNodes (BSD).

### D. Web UI: Electron or Tauri + TypeScript (+ React)

**How it thinks: a web page that runs as a desktop app.** This is what **GB Studio** uses.

```tsx
<input type="number" value={obj.x} onChange={e => update({ x: +e.target.value })} />
```

- **Strengths:** fastest path to a beautiful UI; huge ecosystem (React Flow for node
  graphs, Monaco = VS Code's code editor); GB Studio is a proven precedent.
- **Weaknesses:** most languages (TypeScript + C++, plus Rust for Tauri); Electron is
  heavy *(typical: 200–400 MB RAM)* — a concern on an 8 GB laptop; the C++ core runs as
  a separate process or native addon; Node.js not installed.

## 3. Side-by-side

| Criterion | A. ImGui | B. Avalonia | C. Qt | D. Web |
|---|---|---|---|---|
| Languages you must know | C++ | C++ + C# + XAML | C++ (+ Qt idioms) | C++ + TS (+ Rust) |
| Share C++ core & runtime code in-process | Yes | Via C interface | Yes | Via process/addon |
| Custom canvases (viewport, tile painter) | Excellent | Good | Good | Good (canvas/WebGL) |
| Inspector / forms productivity | Fair | Excellent | Very good | Excellent |
| Looks polished out of the box | Fair | Very good | Very good | Excellent |
| Node-graph libraries | Yes | Yes | Yes | Best |
| Weight on this laptop | Lightest | Medium | Medium-heavy install | Heaviest (Electron) |
| Learning value toward "build engines yourself" | Highest | Medium | Medium | Lower (web skills) |
| License friction | None | None | LGPL care | None |

## 4. Recommendation (unchanged, with reasons stated plainly)

**A (ImGui)** if the main goals are learning engine programming, one language, and
the GBA preview sharing code with the runtime. **B (Avalonia)** if a polished,
Unity-feeling interface matters more and you're happy to learn C#.

Whatever we pick, the project model, asset pipeline, generator and build driver stay
in UI-independent C++ libraries plus the CLI, so a later UI change replaces only the
`editor/` folder.

## 5. Suggested way to decide: a one-afternoon prototype

Build the *same* throwaway mini-editor in A and B: a Hierarchy list, an Inspector with
Name/X/Y, and a viewport showing a 32×32 sprite you can drag. Then compare how the
code reads, how it feels, startup time and memory use (measured). Requires installing
the .NET 8 SDK for B. The prototypes would live in `prototypes/` and would not become engine code.
