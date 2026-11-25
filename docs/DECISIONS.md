# Architectural Decision Records

Status values: **Accepted** (approved by Omar), **Proposed** (recommended, awaiting
approval), **Open** (no recommendation yet), **Superseded** (replaced; link to new one).

Never silently contradict an Accepted decision. To change one, add a new record that
supersedes it and explain why.

---

## D-001 — Toolchain: devkitPro / devkitARM
- **Date:** 2026-11-24 — **Status:** Accepted (given in project brief)
- **Problem:** Which compiler/linker toolchain builds GBA ROMs.
- **Options:** devkitARM; Wonderful Toolchain; stock Arm GNU Toolchain + own crt0/linker script.
- **Selected:** devkitARM (GCC, newlib, `gba_crt0.s`, linker specs, `gbafix`, `make`-based templates).
- **Reasoning:** Community standard, Omar already knows it, Butano/Tonc examples use it.
- **Tradeoffs:** Windows install brings its own MSYS2; `make` dislikes paths with spaces.
- **Revisit if:** devkitARM drops GBA support or Wonderful becomes clearly better for our needs.
  The stock Arm GNU Toolchain already on this machine is **not** used (no GBA crt0/specs).

## D-002 — Runtime: custom lightweight runtime; Butano as reference, not foundation
- **Date:** 2026-11-24 — **Status:** Accepted (Omar, 2026-11-24) as the "middle path":
  our own runtime, deliberately borrowing specific Butano techniques/code (zlib, with
  attribution) wherever Butano's solution is better than what we'd write.
- **Problem:** Build the GBA-side engine ourselves or on top of Butano.
- **Options:**
  1. *Butano as foundation.* Mature, zlib, very feature-rich (sprites, BGs, affine,
     bitmap BGs, audio, link via gba-link-connection, profiler), active (v21.10.1, Nov 2026).
     But: resources (VRAM tiles, palettes) are allocated **at runtime** behind RAII
     handles, which conflicts with our goal of **build-time** resource planning and
     exact editor budgets; generated code would teach Butano's API rather than the
     hardware; own Python/grit asset pipeline and frequent major versions mean tight
     coupling to upstream; custom profiles (isometric occlusion, software 3D) would
     often fight its display managers.
  2. *Custom runtime* reusing small permissive libraries (agbabi zlib, Maxmod ISC,
     gba-link-connection MIT), with Tonc/libtonc (MIT) and Butano as references.
  3. *Fully from scratch* including audio and link — reinvents reliable code.
- **Selected (proposed):** Option 2.
- **Reasoning:** Fits build-time resolution, static allocation, readable generated
  code, and per-profile low-level control. Butano stays the main study reference and
  a possible source of attributed zlib code for specific algorithms.
- **Tradeoffs:** More up-front work; we must earn the reliability Butano already has.
- **Butano as optional backend later:** the generator lowers to a backend-neutral
  Build IR, so a "Butano emitter" can be added later if justified. We will **not**
  design runtime APIs around Butano now.
- **Revisit if:** the custom runtime falls far behind in stability after M2, or
  Omar prefers shipping games quickly over owning the runtime.

## D-003 — Editor UI framework
- **Date:** 2026-11-24 — **Status:** Proposed — Omar wants a detailed comparison first:
  see `docs/EDITOR_FRAMEWORKS.md`.
- **Problem:** Which desktop UI technology for the editor.
- **Options:**

  | Option | Pros | Cons |
  |---|---|---|
  | **A. C++20 + Dear ImGui (docking) + SDL3** | One language across editor, tools and runtime; can compile runtime logic into the editor for previews/tests; MIT; tiny and fast on this 8 GB laptop; widely used for custom engine editors; docking, tables, drag-drop built in | Less polished "native" look; we write our own property-grid widgets; immediate-mode UI needs discipline for complex state; text editing is basic (use external code editor) |
  | B. C# + Avalonia (+ Dock) | Retained UI with data binding, quick forms/inspectors, MIT, cross-platform | Second language; cannot share runtime C++ code directly; needs .NET SDK install |
  | C. C++ + Qt 6 | Most complete desktop toolkit, docking, native look | Multi-GB install, LGPL obligations, `moc` build complexity, steep for a learner |
  | D. Electron/Tauri + TypeScript (GB Studio's path) | Fastest UI development, huge ecosystem, GB Studio precedent | Three languages (TS + C++ + Rust/Node); Electron is memory-heavy; Node/Rust not installed |
  | E. Build the editor inside Godot | Docking, inspector, undo already exist | We'd be fighting Godot's own scene model; heavy coupling |

- **Selected (proposed):** A, with B as the runner-up.
- **Reasoning:** Omar knows C; one language family maximizes learning transfer; the
  editor can link the *same* math/collision/projection code as the runtime, so the
  preview and the GBA agree; lightweight for the laptop. Risk is contained because all
  logic lives in UI-independent `core/` libraries.
- **Tradeoffs:** More hand-built widgets; UI polish takes longer.
- **Revisit if:** inspector/form-heavy UI becomes the bottleneck after M1.3.

## D-004 — Programming languages
- **Date:** 2026-11-24 — **Status:** Accepted for runtime, generated code and scripts
  (Omar: C/C++ family; has C++ experience outside GBA). Editor language depends on D-003.
- **Selected:**
  - Runtime: **C++20 subset** (`-fno-exceptions -fno-rtti`, no heap in hot paths,
    Thumb in ROM by default, ARM in IWRAM for hot loops, C-compatible where cheap).
  - Generated code and user scripts: **C++** in a simple, readable style.
  - Editor, core libraries, CLI: **C++20** (MSVC on Windows; GCC/Clang later).
  - Tests: doctest (MIT) on host; emulator tests via mGBA.
  - Python: allowed for optional dev scripts only; **never required** to build an exported game.
- **Alternatives:** C runtime (simpler, but no constexpr/templates for fixed-point types);
  Rust (no mature GBA editor ecosystem; Omar doesn't know it).
- **Revisit if:** C++ code size or compile times become a real problem (measure first).

## D-005 — Project file format
- **Date:** 2026-11-24 — **Status:** Accepted (Omar, 2026-11-24)
- **Selected:** JSON text files, sorted keys, LF, stable random 64-bit IDs, `.meta`
  sidecars for asset import settings, `formatVersion` + migrations, no binary floats.
- **Alternatives:** binary format (fast, undiffable); YAML (ambiguous parsing);
  SQLite (great queries, poor Git diffs).
- **Revisit if:** very large tilemaps make JSON too slow — then store map cell data in
  a binary sidecar referenced from JSON.

## D-006 — Build-time resolution and static allocation
- **Date:** 2026-11-24 — **Status:** Proposed
- **Selected:** Prefabs, references, VRAM tile slots, palette banks and component pool
  sizes are resolved when building. Runtime uses fixed-capacity pools and simple slot
  allocators for spawned objects. No general heap in normal operation.
- **Tradeoffs:** Truly dynamic content (e.g. unbounded spawning) needs declared capacities.

## D-007 — Own image converter instead of linking grit
- **Date:** 2026-11-24 — **Status:** Proposed
- **Reasoning:** grit is GPL-2.0; linking it into the editor would impose GPL on the
  editor. Our own converter is also needed for exact budgets, caching and tests.
  grit may still be run externally as a cross-check in tests.

## D-008 — Deterministic simulation from day one
- **Date:** 2026-11-24 — **Status:** Proposed
- **Reasoning:** Lockstep multiplayer, replays and emulator regression tests all need
  the same inputs to produce the same game state. Rules: fixed-point math, fixed system
  order, seeded RNG, input as the only external source, no uninitialized memory.

## D-009 — License for GBA Studio itself
- **Date:** 2026-11-24 — **Status:** Open (Omar's decision)
- **Considerations:** The runtime is linked into every game, so it should be
  permissive (zlib or MIT) so developers can sell ROMs without obligations beyond
  notices. The editor could share that license or use another. Not urgent until
  the repo is published.

## D-010 — AI instruction files
- **Date:** 2026-11-24 — **Status:** Proposed
- **Selected:** `AGENTS.md` as the single repo-level instruction file (Cursor reads it
  automatically, as do other assistants). Add small, file-scoped `.cursor/rules/*.mdc`
  rules later only where they add value (e.g. `runtime/**`: no heap/exceptions;
  `generated/**`: never hand-edit). Avoid duplicating the brief across files.

## D-011 — Exported project build system
- **Date:** 2026-11-24 — **Status:** Proposed
- **Selected:** Exported games build with a plain `make` Makefile based on the
  devkitARM GBA template (familiar to GBA developers). The editor/CLI themselves use CMake.
- **Revisit if:** devkitPro's CMake support proves simpler for multi-module builds.

## D-012 — Generated files are marked in their file name
- **Date:** 2026-11-24 — **Status:** Accepted (Omar's request)
- **Problem:** Folder location alone (`generated/`) is easy to miss when a file is open
  in an editor tab, in search results or in a compiler error.
- **Selected:** Every generated file name starts with `gen_` (the extension stays
  normal: `.cpp`, `.hpp`, `.s`, `.bin`). Examples: `gen_scene_main.cpp`,
  `gen_assets.hpp`, `gen_sprite_player.bin`. Files also begin with a "GENERATED — do
  not edit" banner. User scripts and runtime files never use the `gen_` prefix, and
  the generator refuses to *overwrite* a file whose name lacks it. (One-time
  scaffolding — the `Makefile` and new script stubs — is only created when the file
  does not exist yet; it is not "generated" output and belongs to the user.)
- **Alternatives:** suffix (`scene_main.gen.cpp`) — rejected because it reads like a
  different file type; banner only — too easy to miss.

## D-013 — Working name
- **Date:** 2026-11-24 — **Status:** Accepted (temporary)
- **Selected:** "GBA Game Engine" until a final name is chosen. Code identifiers
  (namespace, macro prefix) will be picked at M1.1 and kept short so a later rename is a
  simple search-and-replace.
