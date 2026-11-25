# Research Notes

Legend: **[V]** verified this project (date, how) · **[K]** well-established knowledge,
not re-verified · **[H]** hypothesis, needs testing.

## 1. Libraries, tools and licenses

Checked 2026-11-24 via GitHub API / raw license files unless noted.

| Project | What it is | License | Use in GBA Studio |
|---|---|---|---|
| devkitARM (devkitPro) | GCC toolchain, crt0, specs, gbafix | GPL tools; `gba_crt0.s` MPL-2.0 [V, via third-party license notice]; newlib permissive | **Toolchain** (D-001) |
| Butano 21.10.1 (GValiente) | High-level modern C++ GBA engine | zlib [V] | **Reference**; possible attributed reuse; possible future backend (D-002) |
| libtonc (devkitPro fork; gbadev-org fork) | C hardware library from the Tonc tutorial | MIT — author's statement 2018, no LICENSE file in devkitPro repo; gbadev-org fork lists MIT [V] | Reference; optional |
| Tonc tutorial (gbadev.net/tonc) | Best GBA programming tutorial | text, not code dependency | **Primary learning reference** |
| libgba (devkitPro) | C hardware library | LGPL with exceptions [V] | Avoid as hard dependency |
| agbabi (felixjones) | Fast memcpy/memset, IRQ handler, math, coroutines | zlib [V] | **Likely reuse** in runtime |
| grit (devkitPro) | Image → GBA data converter | **GPL-2.0** [V] | External cross-check only (D-007) |
| Maxmod (devkitPro) | Tracker music + SFX | ISC-style [V] | **Likely audio module** |
| Apex Audio System | Audio library | MIT [V] | Alternative audio |
| gba-link-connection (afska) | Link cable / wireless adapter / multiboot libraries (C++) | MIT [V]; very active (pushed 2026-11-24) | **Likely multiplayer transport** |
| libsavgba | SRAM/Flash/EEPROM save library | BSL-1.0 [V] | Candidate save module |
| libugba (AntonioND) | Universal GBA library (also runs on PC) | has `licenses/` folder, not checked [H] | Study: host-side GBA simulation idea |
| mGBA 0.10.x | Accurate emulator: Lua scripting, GDB stub, local link-cable multiplayer | MPL-2.0 [V]; features from README [V] | **Emulator** (external process first) |
| Dear ImGui | Immediate-mode GUI | MIT [V] | Editor UI (proposed D-003) |
| Avalonia | .NET UI | MIT [V] | Editor alternative |
| GB Studio (chrismaltby) | Visual engine for original Game Boy | MIT [V]; Electron | **Closest precedent** — study its project format and compiler |
| pret/pokeemerald etc. | Decompilations | **no license** [V]; contain Nintendo IP | Study algorithms only; never redistribute; ask before cloning (≈200 MB) |
| awesome-gbadev | Curated resource list | custom disclaimer [V] | Index of further projects |

### Licensing notes
- MIT/ISC/zlib/BSL dependencies linked into a ROM need their notices shipped with the
  game. The exporter should generate `THIRD_PARTY_LICENSES.txt` automatically.
- MPL-2.0 (mGBA, devkitARM crt0) is file-level copyleft: modified MPL files must stay
  available; using them unmodified alongside other code is fine.
- GPL tools (gcc, grit) can be *run* without affecting our license; GPL code must not
  be *linked* into the editor or runtime.
- Public decompilations do **not** grant redistribution rights. Studying techniques
  is different from copying code or assets.
- "Game Boy Advance" is a Nintendo trademark; "GBA Studio" also collides with GB
  Studio and several GitHub repos named "gba-studio" [V, GitHub search].
- ROM headers normally contain the Nintendo logo inserted by `gbafix`; this is
  standard homebrew practice but worth knowing.

## 2. GBA hardware summary [K] (source: GBATEK, Tonc)

| Resource | Value |
|---|---|
| CPU | ARM7TDMI, 16.78 MHz; ARM (32-bit) and Thumb (16-bit) instruction sets |
| Screen | 240×160, 15-bit color (BGR555), ~59.73 Hz; 280,896 cycles per frame |
| IWRAM | 32 KB, fast 32-bit bus — hot code/data, stack |
| EWRAM | 256 KB, slower 16-bit bus — bulk data |
| VRAM | 96 KB (BG 64 KB + OBJ 32 KB in tile modes; OBJ 16 KB in bitmap modes) |
| OAM | 1 KB: 128 sprites, 32 affine matrices |
| Palette RAM | 512 B BG (256 colors) + 512 B OBJ (256 colors); 16 banks of 16 in 4bpp |
| ROM | up to 32 MB, read via wait-states (prefetch helps) |
| Modes | 0: 4 regular BGs · 1: 2 regular + 1 affine · 2: 2 affine · 3: 240×160 16bpp bitmap · 4: 240×160 8bpp paletted, 2 pages · 5: 160×128 16bpp, 2 pages |
| Sprites/scanline | limited by OBJ render cycles (~1210 per line, fewer with H-blank-free) — big or affine sprites reduce the count |
| Link port | Multi-player mode up to 4 GBAs, 9600–115200 bps; also Normal/UART/JOY modes |

## 3. Existing projects to study (planned)

| Topic | Where to look | Status |
|---|---|---|
| Sprite/BG management, resource ownership | Butano `butano/src/bn_sprites_manager.cpp`, `bn_bgs_manager.cpp` | to study |
| Asset import pipeline | Butano `tools/` (Python + grit wrappers) | to study |
| Software 3D on GBA | Butano's *Varooom 3D* sources | to study |
| Mode 7 | Butano `examples/mode_7`, Tonc "Mode 7" chapters | to study |
| Visual editor → compiled engine | GB Studio `src/lib/compiler/` | to study |
| Link cable sync | gba-link-connection `LinkCable`, examples | to study |
| Isometric depth sorting | general literature (bounding-box topological sort), GBA isometric homebrew | to research [H] |

Findings will be recorded here with file/function references as each is studied.

## 3b. Omar's prior experience and the resources he used

Recorded 2026-11-24 so explanations can build on what Omar already knows.

- **Setup:** devkitPro + Programmer's Notepad (setup videos: youtu.be/2FZGD1i6QZY,
  youtu.be/je-0rcRlwrg).
- **Worked:** Luke Walter's *GBA-demo-gravity* (github.com/LukeWalter/GBA-demo-gravity,
  video youtube.com/watch?v=t72QpfcGNK4). [V, read source] Georgia Tech CS 2261-style C:
  Mode 4 page flipping, DMA fills, a hand-written `gba.h`, **8.8 fixed-point** for Y
  position/velocity, `mgba_printf` logging via mGBA's debug registers, plain `make`
  Makefile calling devkitARM tools by path. → Omar has already seen fixed-point,
  DMA, register macros and mGBA logging.
- **Luke Walter's other repos** [V, GitHub API]: `GBA-demo-xxl-bg` (large backgrounds),
  `GBA-demo-saving` (saves), `zenith-gba` (full course project), `GBA-Compiler-Docker`,
  `gba_demo_site` (web demo page), `vocc-original` ("web-based GBA image editor and
  converter" — relevant to our asset tools), `SpriteView` (Java study tool). To study.
- **Didn't work (too vague):** Kyle Halladay, *GBA By Example* parts 2 and 5
  (kylehalladay.com). [V, read part 2] Uses 8bpp sprites, grit/Usenti, raw OAM bit
  tables; mentions top-left artifacts caused by un-hidden OAM entries (explained in
  LEARNING.md §2b).
- **Partly worked, colors scrambled:** sprite video youtu.be/JQw6AKSVeWU [V, transcript]
  — Tonc-based; the author hand-arranges one shared sprite palette (256-color sprites
  first, then 16-color banks). Most likely cause of Omar's scrambled colors: mismatched
  palettes between separately converted images (LEARNING.md §2b). Not confirmed —
  needs Omar's code to diagnose.
- **DS2TILE** (tool by the same video author): not found by web search. Ask Omar for a link.
- **bmp2gbtile** (sourceforge) [V, project page]: converts multiple BMPs and *merges
  16-color palettes* by greedy "lowest palette delta" matching — a useful reference
  algorithm for our palette planner.
- **gfx2gba** [V, gbadev.org]: merges several images into one master palette.

## 4. Open research questions
- Exact mGBA automation path on Windows for tests: Qt `--script` Lua, `mgba-sdl`
  with GDB stub, or linking libmgba into a test harness. [H]
- mGBA local multiplayer: opening extra windows via *File → New multiplayer window*
  links up to 4 instances in one process [K; Omar also recalls this]. Open question: can
  this be scripted/automated for multiplayer regression tests? [H]
- How Grunty's Revenge / similar GBA isometric titles split terrain across BG
  priorities vs sprites — technique study only via documentation/observation in an emulator's VRAM viewer. [H]
