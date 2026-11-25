# Learning Guide

Plain-language explanations of the ideas behind GBA Studio. Each chapter follows the
same pattern: the problem → the idea → an analogy → a tiny example → how our engine
uses it → the proper terms → where to find it in our code.

Chapters are added as the features they explain are built.

---

## 1. What is a game engine, an editor and a runtime?

**Problem.** Every GBA game needs the same boring jobs: turn pictures into numbers
the GBA understands, put sprites on screen, read buttons, wait for the screen to
finish drawing. Writing those again for every game is slow and error-prone.

**Idea.** Split the work into two programs:
- The **editor** runs on your PC. It's where you *design* the game: drag a character
  onto the screen, pick its picture, set its speed.
- The **runtime** runs on the GBA. It's the small set of code that *does* the boring
  jobs while the game plays.

**Analogy.** The editor is an architect's drawing table; the runtime is the
construction crew. The architect never goes to the building site — the drawings
(files) carry all the instructions.

**In our engine.** The "drawings" are generated C++ files plus converted picture
data. devkitARM compiles them together with the runtime into a `.gba` ROM.

**Terms.** *Host* = your PC. *Target* = the GBA. *Toolchain* = compiler + linker +
helpers (devkitARM). *Build* = turning source files into a ROM.

**Where.** `editor/` (editor), `runtime/` (runtime), `core/` (the code that turns the
drawings into files). Nothing is written yet — see `docs/ARCHITECTURE.md`.

---

## 2. How the GBA draws pictures (tiles, sprites, palettes)

**Problem.** The GBA has very little memory. A full-screen picture with real colors
is 240 × 160 × 2 bytes = 76,800 bytes — most of the video memory.

**Idea: reuse small pieces.** Most game graphics are built from **tiles**: tiny
8 × 8 pixel squares. A grass tile used 200 times is stored once.

**Idea: paint-by-numbers.** Instead of storing a full color for every pixel, each
pixel stores a small **number** that points into a **palette** (a list of colors).
With 16-color palettes, a pixel needs only 4 bits (half a byte) — this is called
**4bpp** ("4 bits per pixel"). Color number 0 means "see-through".

**Analogy.** A paint-by-numbers kit: the picture says "1, 1, 3, 2", and the lid of
the box says 1 = green, 2 = brown, 3 = blue. Change the lid and the whole picture
changes color — that's how palette tricks (like flashing when hurt) work for free.

**Tiny example.** One 8 × 8 tile at 4bpp = 64 pixels × 4 bits = 32 bytes.

**Two kinds of things on screen:**
- **Backgrounds**: big grids of tiles (levels, scenery). Up to 4 layers in Mode 0.
- **Sprites** ("objects", OBJ): small movable pictures made of tiles (characters,
  enemies). Up to 128 at once. Their settings live in **OAM** (Object Attribute
  Memory): position, size, which tiles, which palette.

**In our engine.** When you import a PNG, the asset pipeline finds its colors, builds
a palette, cuts it into 8 × 8 tiles and packs them as 4bpp. The editor shows you the
*converted* result so you see what the GBA will really show.

**Terms.** *Tile*, *palette*, *palette bank* (one of 16 groups of 16 colors),
*4bpp/8bpp*, *VRAM* (video memory), *OAM*, *OBJ*.

---

## 2b. Why sprite colors get "mixed up" (and why the engine plans palettes for you)

**The symptom** (Omar hit this): a downloaded sprite demo looks right, but after adding
your own sprites the colors are wrong — and changing the code scrambles them a
*different* way each time.

**Why it happens.** Remember paint-by-numbers: a tile only stores *numbers*; the
palette says what color each number is. A converter tool turns each PNG into tiles
**plus its own palette**, and it numbers that image's colors starting from 0 in its own
order. Two PNGs converted separately might both say "color 3" — one means *red*, the
other means *blue*.

The GBA has only one set of sprite palettes (at address `0x05000200`). If the program
loads only one image's palette — or loads each image's palette to the *same* place so
the last one wins — then every other sprite is painted with the wrong "lid".
Change which palette gets loaded last, and the colors scramble a different way.

**Tiny example.**

| | Image A's palette | Image B's palette |
|---|---|---|
| index 1 | skin | green |
| index 2 | brown | yellow |
| index 3 | red | blue |

If only A's palette is loaded, B's green shirt (index 1) appears skin-colored.

**The three standard fixes:**
1. **One palette bank per sprite (4bpp).** The sprite palette area holds 16 banks of
   16 colors. Copy A's palette to bank 0 and B's to bank 1 (bank *n* starts at
   `0x05000200 + n × 32` bytes), then set each sprite's palette-bank number in OAM
   attribute 2 (bits 12–15).
2. **One shared palette.** Convert all sprites *together* so they agree on numbering
   (grit, gfx2gba and bmp2gbtile can merge palettes).
3. **Hand-arranged palette** — what the Tonc-based sprite video does: plan in advance
   which colors live at which index for every sprite.

Other common causes worth checking: copying the palette to the **background** palette
(`0x05000000`) instead of the sprite one; a 4bpp/8bpp mismatch between the converter
and OAM attribute 0 bit 13 (this usually garbles the shapes too); the converter
reordering colors so that index 0 (always transparent) is no longer the background color.

**In our engine.** The asset pipeline decides palette banks at build time for every
sprite in the scene, rewrites each image's tile numbers to match, and the generated
code sets the bank in OAM for you. The editor shows how many of the 16 banks are used
and warns when two sprites can't share. You never have to line up palettes by hand —
but the generated code comments show exactly which bank each sprite got, so you can
see how it works.

### Related: the mysterious sprite in the top-left corner
Kyle Halladay's tutorial mentions "weird artifacts in the top-left corner" when using
tile slot 0. The cause: OAM starts out as all zeros, and an all-zero OAM entry is a
*visible* 8×8 sprite at (0, 0) that uses tile 0. All 127 unused sprites stack there.
The fix is to **hide every sprite at startup** (set attribute 0 bit 9, the "disable"
bit), then show only the ones you use. Our runtime will do this in its startup code.

---

## 3. The GBA's memory, in one picture

| Area | Size | Think of it as |
|---|---|---|
| ROM (cartridge) | up to 32 MB | A big library you can read but not write |
| EWRAM | 256 KB | A large but slow desk |
| IWRAM | 32 KB | A tiny, very fast desk — put the hottest work here |
| VRAM | 96 KB | The painter's canvas and tile storage |
| OAM | 1 KB | The list of where each sprite is |
| Palette RAM | 1 KB | The paint-by-numbers color lids |

**Why it matters for us.** The engine decides at **build time** which tiles go where
in VRAM and which palette bank each sprite uses, so the editor can show you exactly
how full each memory area is *before* you run the game.

---

## 4. What is code generation, and why "deterministic"?

**Problem.** Converting art to C arrays, writing declarations, setting up sprites —
by hand, every time something changes.

**Idea.** A program (the **generator**) reads your project and *writes the C++ for you*.

**Analogy.** A mail-merge letter: one template ("Dear {{name}}") + a list of names =
many finished letters. Our templates say things like "place {{object}} at ({{x}}, {{y}})".

**Tiny example.** The project says: Player at (100, 60). The generator writes:

```cpp
// GameObject "Player" (scene "Main") starts at pixel (100, 60).
// (0, 0) is the top-left corner of the 240x160 screen.
{ .x = 100, .y = 60, .sprite = &asset_player_png },
```

**Deterministic** means: same project in → *exactly* the same files out, every time,
on every computer. No randomness, no dates, no AI. That lets us test the generator
by comparing its output with a saved "correct answer" file (a **golden file**).

**Where.** `core/codegen/` and `templates/` (not written yet).

---

## 5. Fixed-point numbers (preview — used from Phase 2)

**Problem.** The GBA has no hardware for decimal numbers like 1.5. Software decimal
math ("floating point") is very slow there.

**Idea.** Store numbers as whole numbers that secretly count *fractions*. If we agree
"1 unit = 1/256 of a pixel", then 1.5 pixels is stored as 384 (1.5 × 256).
Adding is normal integer adding. Multiplying needs one extra step: multiply, then
divide by 256 (a fast bit-shift `>> 8`).

**Analogy.** Counting money in cents instead of dollars: $1.50 is just 150 cents,
and you never need a decimal point.

**Terms.** This format is called **24.8 fixed-point** (24 bits for the whole part,
8 bits for the fraction).
