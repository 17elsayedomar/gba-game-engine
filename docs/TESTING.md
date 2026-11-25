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
None yet — added when the first test target exists (M1.1/M1.2).

## Manual test procedures
None yet.

## Recent results
| Date | What | Result | Label |
|---|---|---|---|
| 2026-11-24 | Environment inventory (tools present/absent) | See PROJECT.md table | Observed via shell |

## Known limitations
- devkitPro not yet installed, so no ROM can be built.
- mGBA automation method not yet chosen.
- Real hardware: Omar has a GBA and a flash cartridge (model not yet recorded).
  Hardware test procedure to be written at M1.1.
