# Testing

## Status labels (use these exact words everywhere)
| Label | Meaning |
|---|---|
| Implemented, untested | Code exists; nothing has checked it |
| Automatically tested | Passing automated tests exist and were run |
| Manually tested | A person followed written steps and saw the expected result |
| Emulator verified | Verified running as a ROM in mGBA (automated or manual) |
| Real-hardware verified | Verified on a physical GBA (flash cart) |

Never report a result that was not actually observed.

## Test layers (planned)
| Layer | What | Tool | Runs where |
|---|---|---|---|
| Unit | model, serialization, math, collision, sorting | doctest | host (PC) |
| Runtime-on-host | pure-logic runtime modules compiled for PC | doctest | host |
| Asset conversion | PNG → exact expected tile/palette bytes | doctest + fixture PNGs | host |
| Golden codegen | generated files vs checked-in expected files | doctest; diff on failure | host |
| Determinism | generate twice / in two folders → identical hashes | CLI test | host |
| Build | sample projects build with devkitARM; IWRAM/EWRAM/ROM sizes from map file | CLI + make | host |
| Emulator | run ROM N frames; read OAM/VRAM/RAM; compare screenshots | mGBA (method TBD, see RESEARCH §4) | host |
| Editor | command layer (undo/redo, edits) headless; UI smoke tests manually | doctest + manual checklist | host |
| Performance | cycles per system per frame via hardware timers | runtime profiler module + mGBA | emulator |
| Multiplayer | two/four linked mGBA instances; state-hash comparison | mGBA local link | emulator |
| Hardware | sample ROMs on a real GBA | flash cart (if Omar has one) | GBA |

Every sample project in `samples/` doubles as a regression test.

## Commands

```powershell
# Editor-framework prototypes: shared core tests (doctest)
cmake --build build/prototypes --config Release --target proto_core_tests
build\prototypes\proto_core\Release\proto_core_tests.exe

# Editor-framework prototypes: performance measurements (about 5 minutes)
powershell -ExecutionPolicy Bypass -File prototypes\tools\measure.ps1

# M1.0: build the devkitPro GBA template (copy it somewhere first)
C:\devkitPro\msys2\usr\bin\bash.exe -lc 'cd "/c/path/to/copy" && make'
```

## Manual test procedures
- **Editor prototypes (D-003):** checklist in `prototypes/README.md`, done in both prototypes.

## Recent results
| Date | What | Result | Label |
|---|---|---|---|
| 2026-11-24 | Environment inventory (tools present/absent) | See PROJECT.md table | Observed via shell |
| 2026-11-24 | **M1.0** devkitPro GBA template (`C:\devkitPro\examples\gba\template`, copied) | `make` exit 0 in 5.3 s; ROM 55,600 bytes; mGBA 0.10.3 shows "Hello World!" at 59.9 fps (re-checked 2026-11-25) | **Emulator verified** (screenshot inspected by the assistant) |
| 2026-11-25 | `proto_core` tests: commands/undo/merge, components, PNG import + GBA colors, byte-identical save/load, garbage input, deterministic codegen, iso projection/picking, viewport picking, node graph | 13/13 cases, 199/199 assertions pass | **Automatically tested** |
| 2026-11-25 | Both prototypes start, load `sample_scene.json`, render it | Screenshots inspected: panels docked, GBA frame, sprites in GBA colors | Automatically smoke-tested (not a full manual test) |
| 2026-11-25 | Scripted drag, 60 moves/s through the real command path (`--benchmark-drag`) | No errors; ImGui holds 57 fps (VSync) | Automatically smoke-tested |
| 2026-11-25 | Startup, idle/drag CPU, memory, build times, sizes, lines of code | See `docs/EDITOR_FRAMEWORKS.md` section 2 | Measured on Omar's laptop |
| — | The 14 prototype features by hand | Pending: Omar's checklist | Implemented, untested |

## Known limitations
- No automatic UI tests: prototype UIs are checked by screenshots and by hand.
- Measurements come from one machine and one run per configuration (startup: median of 5).
  The drag CPU numbers varied by about ±3 percentage points between runs.
- Avast scans every newly built `.exe` on its first launch, so first-run timings are discarded.
- mGBA automation method not yet chosen.
- Real hardware: Omar has a GBA and a flash cartridge (model not yet recorded).
  Hardware test procedure to be written at M1.1.
