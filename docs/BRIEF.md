# Original Requirements (condensed)

Condensed from Omar's project brief of 2026-11-24. Section numbers match the brief.
If anything here conflicts with a later Accepted decision, the decision wins.

1. **Role** — AI acts as architect, GBA specialist, engineer, researcher, tester and
   instructor. Omar knows devkitPro GBA development, not Butano or engine building.
   Goal: Omar learns enough to maintain/extend the engine independently.
2. **Vision** — Unity/Godot/GameMaker-like visual engine for GBA; build a complete
   visual scene before any script; produce native ROMs for real hardware/accurate
   emulators; serve beginners (visual) and experts (C/C++, hardware access).
3. **Architecture** — devkitARM + custom lightweight modular runtime + desktop editor +
   deterministic source/asset generation. Study Butano (not automatic foundation).
   Research Tonc, libgba, agbabi, grit, mGBA, GBATEK, homebrew, decomps, multiplayer
   libs. Keep separated: editor, data model, component metadata, asset pipeline,
   code generator, build system, runtime, rendering profiles/backends, emulator/debug,
   testing. Runtime usable without editor. Don't build multiple backends up front.
4. **Research** — study existing projects for sprites, tiles, BGs, animation, camera,
   collision, physics, isometric, software 3D, lighting, audio, multiplayer, saves,
   memory, optimization, state. Document files/functions/algorithms with sources;
   verify licenses; study ≠ permission to redistribute. Ask before big downloads/deps.
5. **Editor** — viewport, hierarchy, inspector, asset browser, component/prefab/sprite/
   bitmap/tileset/tilemap/animation/state-machine/visual-script editors, script editor
   integration, audio, rendering/physics/multiplayer/build settings, console, debugger,
   resource profiler, source preview/export. Drag-drop, multi-select, duplicate,
   transforms, snapping, copy/paste, undo/redo, shortcuts, search, save/load, autosave
   and recovery, configurable layouts. Windows first; don't block Linux/macOS.
6. **GameObjects/components** — Unity-like hierarchy; long-term components: Transform,
   SpriteRenderer, BitmapRenderer, BackgroundRenderer, TilemapRenderer,
   IsometricRenderer, MeshRenderer, Animator, Camera, Collider, Rigidbody,
   CharacterController, Light, AudioSource, InputController, Script, StateMachine,
   NetworkIdentity, NetworkSync, Trigger, ParticleEmitter. Custom components. Runtime
   must be lightweight (static allocation, compact data, compile-time resolution).
7. **Authoring** — prefabs (instances, nesting, variants, overrides, runtime
   instantiation); Inspector-exposed script properties incl. asset/object/prefab refs
   via drag-drop, without heavy runtime reflection; ScriptableObject-style data assets;
   visual state machines compiled to native code/data; node-based visual scripting
   compiled deterministically (no LLM); normal C/C++ scripts always possible.
8. **Display modes & Rendering Profiles** — all six modes; profiles (2D platformer,
   top-down, isometric RPG/platformer, affine pseudo-3D, Mode 7, software 3D, bitmap,
   custom) define compatible modes, components, coordinates, camera, algorithms,
   assets, physics, editor tools, resource needs. Explain incompatibilities.
9. **Isometric engine (architectural priority)** — iso/dimetric projection, logical 3D
   coords, iso tilemap and elevation editing, overlapping heights, slopes/stairs,
   bridges, jumping/gravity, ground/platform/wall collision, triggers, moving
   platforms, robust depth sorting and occlusion (not just screen-Y), animation,
   camera, interactables, transitions. References: Super Mario RPG, Banjo-Kazooie:
   Grunty's Revenge (techniques only, no assets/code).
10. **Physics, 3D, lighting** — modular 2D/2.5D/simple 3D physics with layers;
    software 3D (fixed-point, projection, wireframe/flat/limited texture, culling,
    clipping, sorting, rasterization); hardware-appropriate lighting. Decide by
    measured performance.
11. **Graphics tools** — sprite editor (pixel tools, palette, frames, onion skin);
    bitmap editor for Modes 3/4/5; sprite-sheet splitter (grid + automatic
    connected-region detection, background detection, manual correction, naming,
    ordering, pivots, metasprites; deterministic algorithms); tile/tilemap tools
    (metatiles, layers, collision/elevation painting, animated tiles, flips, palettes);
    asset processing (quantization, 4/8bpp, dedupe, alignment, compression,
    declarations), keeping originals separate; warn on quality loss/limits.
12. **Deterministic source generation (defining feature)** — every project exportable
    as readable, buildable C/C++; model → deterministic pipeline (no LLM/internet);
    large data as const/binary; meaningful deterministic comments with scene/object/
    asset names, beginner-friendly; separate user scripts / generated / runtime /
    assets / third-party / build config; never overwrite user scripts; ROM-only,
    full-source, self-contained, SDK-referencing exports; reproducible builds; source
    preview; exported games build without the editor. No promise of two-way sync.
13. **Multiplayer** — 2–4 player link cable; sessions, roles, IDs, connect/disconnect,
    input and state sync, events, shared objects, scene transitions, errors, latency.
    Evaluate lockstep, host-authoritative, input-sync, compact state. Editor workflow
    (settings, player prefabs, ownership, NetworkIdentity/Sync, spawn points, debug).
    Low-level API for experts. Multi-instance mGBA testing; real hardware where
    possible. Wireless Adapter and MultiBoot later, optional.
14. **Build & emulator** — devkitARM; Editor Preview (fast, approximate, clearly
    labelled) vs Game Mode (validate → assets → generate → compile → ROM → mGBA).
    Later: embed/debug mGBA (pause, step, breakpoints, memory/registers/VRAM,
    profiling, multiplayer, error navigation). Don't write an emulator. Keep build logs.
15. **Resource validation** — track/estimate VRAM (BG/OBJ), OAM, scanline limits,
    palettes, tiles, BGs, affine, EWRAM/IWRAM, ROM, stack/heap, DMA, timers, IRQs,
    audio, link, CPU, frame time; warn near limits; estimated vs measured; suggestions.
16. **Importing projects** — research workspace (inspect without converting), asset
    import, library/source integration, project-specific importers; non-destructive;
    respect licenses (public decomps ≠ redistribution rights).
17. **Engine code quality** — clear names, modules, small functions, documented
    assumptions; beginner-friendly comments on meaningful blocks, algorithms, hardware;
    don't sacrifice efficiency for explainability (add explanations instead).
18. **Teaching** — explain for a beginner: problem, simple concept, analogy, small
    example, how it applies, proper terms, where in code; step by step; encourage
    Omar's own experiments; help him understand rather than replace his code.
19. **Testing** — incremental: behavior, deps, acceptance criteria, small version,
    tests, build, run, editor check, mGBA check, manual tests, fix, document, record.
    Labels: implemented-untested / auto-tested / manually tested / emulator verified /
    real-hardware verified. Never fabricate results. Review before new milestones.
20. **Sample games** — demos with clear purpose using public APIs only, beginner
    comments, exercises for Omar, kept as regression tests (static sprite … 4-player).
21. **Docs (mandatory)** — PROJECT.md, docs/ARCHITECTURE.md, ROADMAP.md, RESEARCH.md,
    DECISIONS.md, TESTING.md, LEARNING.md, CHANGELOG.md, README.md, AGENTS.md.
22. **Session continuity** — start: read PROJECT/AGENTS/architecture/decisions, inspect
    repo and Git state, milestone, tests, confirm next task. End: record what was done,
    files, decisions, tests, failures, untested items, problems, next steps.
23. **Version control safety** — Git from the start; focused commits; tags at stable
    milestones; no deletions, overwriting user scripts, destructive Git, silent major
    installs, unrelated changes, unexplained architecture changes or pushes without approval.
24. **Engineering** — correctness, maintainability, modularity, readability, hardware
    accuracy, performance, memory efficiency, reproducibility, usability, education.
    Unused features must not cost size/time; low-level access must remain possible.
25. **Roadmap** — Phases 0–8 (research; minimal editor-to-ROM; gameplay; graphics/levels;
    authoring systems; isometric; multiplayer; advanced rendering/physics; advanced tools).
26. **First assignment** — research and planning only; wait for approval before
    implementing the main application.
