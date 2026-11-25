# GBA Game Engine (working name)

A visual game-development environment that produces native Game Boy Advance ROMs and
readable, buildable C++ source projects.

**Status: Phase 0 (architecture).** There is no editor or runtime code yet. Start with
[`PROJECT.md`](PROJECT.md).

## Installing dependencies (Windows)

1. **devkitPro (required).** Run `devkitProUpdater-3.0.3.exe` (or the latest from
   <https://github.com/devkitPro/installer/releases>). Select **GBA Development**.
   Keep the default install path `C:\devkitPro` (no spaces).
   Verify in the *devkitPro → MSYS2* shell:
   ```sh
   echo $DEVKITARM          # should print /opt/devkitpro/devkitARM
   arm-none-eabi-gcc --version
   ```
2. **mGBA** — <https://mgba.io>. Installed on the dev machine at `C:\Program Files\mGBA`.
3. **CMake ≥ 3.25** and **Visual Studio 2022 Build Tools (C++)** — for the editor (later).

## Building the editor / running it / creating a project / building a ROM
Not available yet — see `docs/ROADMAP.md` milestone M1.

## Running tests
Not available yet — see `docs/TESTING.md`.

## Repository structure (planned)
See `docs/ARCHITECTURE.md`, section 10.

| Path | Contents |
|---|---|
| `PROJECT.md` | Current status and continuity notes |
| `AGENTS.md` | Rules for AI assistants |
| `docs/` | Architecture, decisions, roadmap, research, testing, learning guide |
