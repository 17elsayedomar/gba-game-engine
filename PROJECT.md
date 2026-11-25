# GBA Game Engine — Project Overview

> **Read this first.** This is the continuity document for humans and AI assistants.
> It is kept short on purpose; details live in `docs/`. Update it at the end of every
> meaningful session (see `AGENTS.md`).

"GBA Game Engine" is a temporary working name (D-013); the final name will be chosen
later. Earlier notes may say "GBA Studio" — same project.

## Vision

A desktop, visual game-development environment (inspired by Unity / Godot / GB Studio)
that produces **native Game Boy Advance ROMs** and **readable, buildable C++ source
projects** with deterministic, auto-generated comments. Designed around real GBA
hardware limits, with long-term priorities on an **isometric platformer engine**,
**easy link-cable multiplayer**, and **teaching the developer how it all works**.

A condensed, section-by-section record of the original requirements is in
`docs/BRIEF.md`; treat it as the requirements baseline.

## Architectural direction (summary)

devkitARM toolchain + our own lightweight modular GBA runtime (C++ subset) that
deliberately borrows proven pieces (agbabi, Maxmod, gba-link-connection, selected
Butano techniques with attribution) + desktop editor + UI-independent core libraries
(project model, asset pipeline, deterministic code generator, build driver) + a
headless CLI. See `docs/ARCHITECTURE.md` and `docs/DECISIONS.md`.

## Current status

| Item | Value |
|---|---|
| Phase | **Phase 0 — Research and architecture** (nearly done) |
| Milestone | M1.0 ✅ (toolchain sanity). Next: D-003 decision, then M1.1 |
| State | **Waiting for Omar to test both editor prototypes** (`prototypes/README.md` checklist), then discuss D-003 |
| Code written | Throwaway editor prototypes only (`prototypes/`); no engine code yet |
| Git | `main` on GitHub `17elsayedomar/gba-game-engine`; author "Gamebot Advance"; pushes approved after each milestone. Repo-local `http.sslBackend=schannel` (Avast HTTPS scanning breaks Git's default certificate check) |

### Decisions
- Accepted: D-001 devkitARM, D-002 runtime "middle path", D-004 C/C++ family for
  runtime/generated code/scripts, D-005 JSON project format, D-012 `gen_` file-name
  prefix for generated files, D-013 working name.
- Open: **D-003 editor UI framework**: Dear ImGui + SDL3 vs Qt 6 Widgets. Both
  prototypes built and measured; see `docs/EDITOR_FRAMEWORKS.md`. **Do not finalize
  until Omar has tested both and we have discussed.** D-009 engine license (not urgent).

### About Omar (to tailor explanations)
Has built devkitPro C demos (Mode 4, DMA, 8.8 fixed-point, mGBA logging) with
Programmer's Notepad; has C++ experience outside GBA; got stuck on sprites (vague
tutorials; scrambled colors from palette mismatches). Has a real GBA + flash cartridge.
Details: `docs/RESEARCH.md` §3b. Redistribution of third-party code/IP is not a goal:
examples are for learning; released samples will use generic original assets.

## Environment (verified 2026-11-24 on Omar's Windows 10 laptop, i5-7300U, 8 GB RAM)

| Tool | Status |
|---|---|
| devkitPro / devkitARM | Installed at `C:\devkitPro` (GCC 16.1.0, make, gbafix, grit). Build via `C:\devkitPro\msys2\usr\bin\bash.exe -lc '...'` |
| Qt | 6.8.3 MSVC 2022 at `C:\Qt\6.8.3\msvc2022_64` (qtbase only, 1.96 GB, via aqtinstall). Used only by `prototypes/qt_editor` |
| Avast antivirus | Scans each new `.exe` on first launch (slows first run); intercepts HTTPS (Python downloads need a CA bundle that includes the Avast root) |
| mGBA | 0.10.3 at `C:\Program Files\mGBA` (not on PATH). Upstream latest 0.10.5. |
| Arm GNU Toolchain 15.2 (standalone) | On PATH. Not devkitARM; must not be picked up by our builds. |
| Git 2.46, CMake 4.3, Python 3.14 | Installed. `gh` (GitHub CLI) not installed. |
| MSVC (VS 2022 Build Tools) | Installed (C++ x86/x64 tools). |
| MSYS2 (`C:\msys64`, mingw64 gcc) | Installed, no `make` package. |
| .NET | Runtime 7.0.8 only, no SDK. |
| Node.js, Rust | Not installed. |

## Implemented features

Engine: none yet. Prototypes (throwaway, for D-003), in `prototypes/`:
- `proto_core`: model, components, undoable commands, PNG import with GBA color
  preview, JSON save/load, `gen_` source preview, iso/node geometry. **Automatically tested** (13 cases).
- `imgui_editor` and `qt_editor`: the same 14 features. Smoke-tested automatically;
  full feature check **Implemented, untested** until Omar's checklist.

## Next tasks
1. Omar: run both prototypes through the checklist in `prototypes/README.md`.
2. Discuss results; finalize D-003.
3. M1.1 hand-written "target output" sprite ROM.

## Known limitations / risks (top items)

- Scope is very large; strict milestone discipline is required.
- Final name not chosen; "GBA Studio" collides with GB Studio and other repos.
- Full risk list: `docs/ARCHITECTURE.md`, section 11 "Risks".

## Known bugs

- Prototypes: node-graph edits are not undoable (both, by design for now).

## Testing status

M1.0 emulator verified; `proto_core` 13/13 tests passing; prototype measurements in
`docs/EDITOR_FRAMEWORKS.md`. Details and labels: `docs/TESTING.md`.

## Documentation map

- `docs/ARCHITECTURE.md` — modules, data flow, data model, codegen, profiles, risks
- `docs/DECISIONS.md` — decision records (with status)
- `docs/EDITOR_FRAMEWORKS.md` — detailed UI framework comparison (D-003)
- `docs/ROADMAP.md` — milestones, acceptance criteria
- `docs/RESEARCH.md` — technologies studied, licenses, Omar's prior resources
- `docs/TESTING.md` — test strategy, commands, recent results
- `docs/LEARNING.md` — beginner-friendly explanations
- `docs/BRIEF.md` — condensed original requirements (baseline)
- `prototypes/README.md` — D-003 prototypes: build, run, Omar's test checklist
- `AGENTS.md` — rules for AI assistants
- `CHANGELOG.md`

## Session log

- **2026-11-24** — Session 1. Inspected workspace and toolchain, researched libraries
  and licenses, wrote architecture proposal and all initial docs, initialized Git.
  Nothing implemented or tested.
- **2026-11-24 (later)** — Session 2. Recorded Omar's answers (runtime middle path,
  C/C++ family, `gen_` prefix, working name). Studied Omar's prior tutorials (Luke
  Walter gravity demo source, Kyle Halladay part 2, Tonc-based sprite video transcript);
  added LEARNING §2b on palette mix-ups; wrote `docs/EDITOR_FRAMEWORKS.md`. Omar removed
  the devkitPro source-installer zip from the workspace. Nothing implemented or tested.
- **2026-11-24/25** — Session 3. Omar installed devkitPro and shared the GitHub repo;
  Git configured and Phase 0 docs pushed. M1.0 passed (emulator verified). Omar
  narrowed D-003 to ImGui vs Qt and asked for equivalent prototypes: built
  `prototypes/` (shared core + both editors), installed Qt 6.8.3 via aqtinstall,
  measured both on this laptop, rewrote `docs/EDITOR_FRAMEWORKS.md`. Waiting for
  Omar's hands-on test before deciding D-003.
