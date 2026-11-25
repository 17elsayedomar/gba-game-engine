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
| Milestone | M0 → next is M1.0 (toolchain sanity) |
| State | Waiting for: devkitPro install, Git identity, editor framework comparison (D-003) |
| Code written | None (documentation and repo scaffolding only) |
| Git | Initialized, branch `main`, no remote. Docs staged but **not committed**: no Git user identity configured yet |

### Decisions
- Accepted: D-001 devkitARM, D-002 runtime "middle path", D-004 C/C++ family for
  runtime/generated code/scripts, D-005 JSON project format, D-012 `gen_` file-name
  prefix for generated files, D-013 working name.
- Open: **D-003 editor UI framework** — Omar wants a detailed comparison
  (`docs/EDITOR_FRAMEWORKS.md`), possibly hands-on prototypes. D-009 engine license (not urgent).

### About Omar (to tailor explanations)
Has built devkitPro C demos (Mode 4, DMA, 8.8 fixed-point, mGBA logging) with
Programmer's Notepad; has C++ experience outside GBA; got stuck on sprites (vague
tutorials; scrambled colors from palette mismatches). Has a real GBA + flash cartridge.
Details: `docs/RESEARCH.md` §3b. Redistribution of third-party code/IP is not a goal:
examples are for learning; released samples will use generic original assets.

## Environment (verified 2026-11-24 on Omar's Windows 10 laptop, i5-7300U, 8 GB RAM)

| Tool | Status |
|---|---|
| devkitPro / devkitARM | **Not installed yet.** Omar will run `devkitProUpdater-3.0.3.exe` (default path `C:\devkitPro`). |
| mGBA | 0.10.3 at `C:\Program Files\mGBA` (not on PATH). Upstream latest 0.10.5. |
| Arm GNU Toolchain 15.2 (standalone) | On PATH. Not devkitARM; must not be picked up by our builds. |
| Git 2.46, CMake 4.3, Python 3.14 | Installed. `gh` (GitHub CLI) not installed. |
| MSVC (VS 2022 Build Tools) | Installed (C++ x86/x64 tools). |
| MSYS2 (`C:\msys64`, mingw64 gcc) | Installed, no `make` package. |
| .NET | Runtime 7.0.8 only, no SDK. |
| Node.js, Rust, Qt | Not installed. |

## Implemented features

None yet.

## Next tasks
1. Omar: install devkitPro (GBA Development); set Git identity; optionally create a GitHub repo.
2. M1.0 toolchain sanity (see `docs/ROADMAP.md`).
3. Decide D-003 (comparison doc, optional prototypes).
4. M1.1 hand-written "target output" sprite ROM.

## Known limitations / risks (top items)

- Scope is very large; strict milestone discipline is required.
- Final name not chosen; "GBA Studio" collides with GB Studio and other repos.
- Full risk list: `docs/ARCHITECTURE.md`, section 11 "Risks".

## Known bugs

None (no code yet).

## Testing status

No tests exist yet. Strategy is defined in `docs/TESTING.md`.

## Documentation map

- `docs/ARCHITECTURE.md` — modules, data flow, data model, codegen, profiles, risks
- `docs/DECISIONS.md` — decision records (with status)
- `docs/EDITOR_FRAMEWORKS.md` — detailed UI framework comparison (D-003)
- `docs/ROADMAP.md` — milestones, acceptance criteria
- `docs/RESEARCH.md` — technologies studied, licenses, Omar's prior resources
- `docs/TESTING.md` — test strategy, commands, recent results
- `docs/LEARNING.md` — beginner-friendly explanations
- `docs/BRIEF.md` — condensed original requirements (baseline)
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
