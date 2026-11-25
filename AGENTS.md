# Instructions for AI coding assistants

## Before starting any work
1. Read `PROJECT.md` (status, current milestone, open decisions).
2. Read this file, then the relevant parts of `docs/ARCHITECTURE.md` and `docs/DECISIONS.md`.
3. Run `git status` and `git log --oneline -10`; note the branch and uncommitted changes.
4. Check recent results in `docs/TESTING.md`.
5. Confirm the next task with Omar before starting a new milestone.

Do not rely on chat history; the repository docs are the source of truth.

## Workflow
- Work in small, testable increments: define behavior → acceptance criteria →
  implement → test → document. Do not start a new milestone until the current one
  is reviewed with Omar.
- Never contradict an **Accepted** decision silently. Propose a superseding record in
  `docs/DECISIONS.md` and explain why.
- Ask before: installing major dependencies, cloning large repos, destructive Git
  operations, pushing to remotes, deleting work, major refactors.
- Keep commits focused, with clear messages.
- The dev machine uses Windows PowerShell 5.1: never write text files with
  `Set-Content`/`Add-Content`/`>` (they don't write UTF-8). Use editor file tools or Python.
- Never `git add -A` / `git add .` blindly: the workspace root holds large installer
  files. Check `git status` and add paths explicitly.

## Testing
- Use the status labels in `docs/TESTING.md`. Never claim something works unless it
  was tested; never fabricate results.
- Add/update automated tests with each feature. Keep sample projects as regression tests.
- When manual testing is needed, write exact steps and the expected result.
- Fix general engine behavior; never special-case the engine to make a demo pass.

## Code rules
- `runtime/`: C++ subset — no exceptions, no RTTI, no heap in normal operation,
  fixed-point math, deterministic simulation (D-008). Measure before optimizing.
- `generated/` (in game projects): never hand-edit; change templates or the generator.
- `scripts/` (in game projects): user-owned; tools must never overwrite them.
- Generated-code comments must come from deterministic templates and metadata — never an LLM.
- Third-party code: verify license first, vendor into `third_party/` with its license file,
  record it in `docs/RESEARCH.md`.

## Comments and teaching
- Engine code gets beginner-friendly comments on meaningful blocks: what, why, hardware
  reasons, assumptions, limitations. Skip comments on trivial lines.
- When explaining to Omar: simple language (assume a beginner), problem → idea →
  analogy → small example → how it applies here → proper term → where in the code.
  Help him understand rather than replacing his code. Suggest small exercises.
- Add new concepts to `docs/LEARNING.md`.

## After meaningful work
Update `PROJECT.md` (status + session log), `CHANGELOG.md`, `docs/TESTING.md`
(results), and any affected docs. Record: what changed, files touched, decisions,
tests run and results, what remains untested, next steps.
