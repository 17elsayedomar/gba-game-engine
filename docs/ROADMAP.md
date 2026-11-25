# Roadmap

Status legend: ⬜ not started · 🟨 in progress · ✅ done (with test evidence in TESTING.md) · ⏸ deferred

Refinements vs the original brief's phases:
- Phase 1 is split into four small, separately testable steps (M1.0–M1.3).
- Two **research spikes** run early (hand-written prototype ROMs, not engine code)
  because isometric and multiplayer constrain foundational decisions.
- Determinism (needed for lockstep multiplayer) is a rule from Phase 1, not Phase 6.

## Phase 0 — Research and architecture  🟨
- ✅ Workspace and toolchain inventory
- ✅ Library/license research (first pass)
- ✅ Architecture proposal, decision records, documentation set
- ⬜ Omar approves or changes D-002, D-003, D-004, D-005 (and later D-009)
- ⬜ devkitPro installed with GBA Development packages

## Phase 1 — Minimal editor-to-ROM pipeline ("Hello Sprite")

### M1.0 — Toolchain sanity  ⬜
Build the devkitPro GBA template and run it in mGBA.
- **Accept:** `make` in a copy of the template exits 0; the `.gba` opens in mGBA; result recorded.

### M1.1 — Hand-written "target output"  ⬜
Write by hand the exact C++ we *want* the generator to produce for one sprite, using a
first tiny runtime (`core`: boot, VBlank wait; `sprites`: OAM shadow + commit).
- **Accept:** sprite visible in mGBA at (100, 60); OAM entry 0 holds Y=60, X=100
  (checked in mGBA's OAM viewer, then automatically once M1.2 tests exist).
- **Why first:** we design the generator's output before writing the generator.

### M1.2 — Headless pipeline (CLI)  ⬜
`core/model`, `core/assets` (PNG → 4bpp tiles + palette), `core/codegen`, `core/build`, `cli/`.
- **Accept:**
  1. `gbstudio new MyGame` creates the documented folder layout.
  2. Importing a ≤15-color + transparent PNG creates an asset and a `.meta` sidecar.
  3. Load → save of a scene is byte-identical (round-trip test).
  4. Generating twice produces byte-identical `generated/` trees (SHA-256 equal), also from a different folder path.
  5. Golden tests: generated files match checked-in expected output.
  6. Asset tests: a known PNG produces exactly the expected tile and palette bytes.
  7. `make` inside the generated project folder, **without the editor**, produces a ROM.
  8. Emulator test: after N frames, OAM entry 0 = (100, 60) and a screenshot matches the reference image.
  9. A PNG with too many colors produces a clear error naming the file and the color count — no crash.
  10. A file in `scripts/` is byte-identical before and after regeneration.

### M1.3 — Minimal editor  ⬜
Window with Scene viewport, Hierarchy, Inspector, Asset list, Console; Play button.
- **Accept:**
  1. Create project, import PNG, create GameObject, add SpriteRenderer — no code typed.
  2. Drag the sprite in the viewport; Inspector shows integer pixel position; grid snapping works.
  3. Undo/redo of create/move/add-component (automated command-layer tests + manual check).
  4. Save, close, reopen: scene identical.
  5. Play: validate → convert → generate → build → launch mGBA; sprite at the shown position.
  6. Move to (150, 90), Play again: only the position line in the generated scene file changes.
  7. Build errors appear in the Console with file and line.
  8. Measured Play-to-mGBA time on this laptop is recorded (target < 10 s warm).
  9. Resource panel shows OBJ tiles and palette banks used (estimated).

## Research spikes (hand-written prototypes; can overlap Phase 2)
- **S1 Isometric depth & occlusion** ⬜ — a few blocks at different heights, one
  jumping sprite passing behind/in front; compare screen-Y sort vs bounding-box
  topological sort; measure CPU per frame for 16/32/64 objects.
- **S2 Link-cable lockstep** ⬜ — two mGBA instances via local link; exchange inputs
  with gba-link-connection; verify identical state hashes on both after 10,000 frames.

## Phase 2 — Basic gameplay ⬜
User scripts + `GBS_PROPERTY` scanner, input, movement, Inspector-exposed properties,
basic camera, AABB collision, sample scripts, independent source export.

## Phase 3 — Graphics and level tools ⬜
Sprite-sheet splitter (grid + connected-region detection with manual correction),
pixel editor, palette editor, animation, tilesets, tilemaps, asset optimization.

## Phase 4 — Reusable authoring ⬜
Prefabs (nested, variants, overrides), custom components, data assets, state machines, events.

## Phase 5 — Isometric engine ⬜ (architectural priority; informed by S1)

## Phase 6 — Multiplayer ⬜ (informed by S2)

## Phase 7 — Advanced rendering and physics ⬜
Other video modes, affine/Mode 7 profile, bitmap profile, software 3D, lighting.

## Phase 8 — Advanced tools ⬜
Visual scripting, debugger integration, profiler, importers, more backends.

## Deferred
- Wireless Adapter, single-cartridge MultiBoot (after Phase 6).
- Embedded emulator inside the editor (after external-mGBA workflow is solid).
- Two-way sync of hand-edited generated code (not promised).
