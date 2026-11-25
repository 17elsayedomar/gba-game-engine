# Changelog

All meaningful changes are recorded here. Newest first.

## [Unreleased]

### 2026-11-25 — Editor framework prototypes (D-003 still open)
- Added `prototypes/`: shared UI-independent `proto_core` (model, components,
  undoable commands, PNG import with GBA color preview, deterministic JSON
  save/load, `gen_` C++ source preview, isometric and node-graph geometry) with
  13 doctest cases; a Dear ImGui + SDL3 prototype; a Qt 6 Widgets prototype; both
  implement the same 14 features.
- Added `prototypes/tools/measure.ps1` and `count_lines.py`; measured both
  prototypes on Omar's laptop (`docs/EDITOR_FRAMEWORKS.md` section 2).
- Rewrote `docs/EDITOR_FRAMEWORKS.md` as an ImGui-vs-Qt comparison with measured
  results and a Qt facility analysis.
- Recorded M1.0 (devkitPro template ROM, emulator verified) in TESTING and ROADMAP.
- Installed (outside the repo): devkitPro to `C:\devkitPro`; Qt 6.8.3 MSVC 2022 to
  `C:\Qt` (aqtinstall). Git identity "Gamebot Advance"; repo pushed to GitHub.

### 2026-11-24 — Decisions and research follow-up
- Accepted D-002 (runtime "middle path"), D-004 (C/C++ family), D-012 (`gen_` prefix
  for generated files), D-013 (working name "GBA Game Engine").
- Added `docs/EDITOR_FRAMEWORKS.md`; LEARNING §2b (palette mix-ups, hidden OAM
  entries); RESEARCH §3b (Omar's prior resources).

### 2026-11-24 — Phase 0 kickoff
- Added: initial documentation set (`PROJECT.md`, `README.md`, `AGENTS.md`,
  `docs/ARCHITECTURE.md`, `DECISIONS.md`, `ROADMAP.md`, `RESEARCH.md`, `TESTING.md`,
  `LEARNING.md`, `BRIEF.md`).
- Added: Git repository, `.gitignore`, `.gitattributes` (LF line endings).
- No engine code yet.
