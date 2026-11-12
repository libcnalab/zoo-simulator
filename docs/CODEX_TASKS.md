# Codex CLI task log

Work delegated to the Codex CLI (`codex exec`), newest last. Each task gets a written brief
(objective, context, allowed files, requirements, acceptance criteria, tests, report) and its
result is reviewed, built and tested by the lead before it is committed. Codex never commits
and never pushes; briefs are kept in `.codex-work/` (not versioned) while a task runs.

| # | Task | Files owned | Result | Review notes |
|---|------|-------------|--------|--------------|
| B | Species catalog: JSON loading and validation of `Species`, the fifteen species data files, the species table in `docs/ANIMALS.md` | `src/core/zoo/animals/Species.cpp`, `tests/core/SpeciesTests.cpp`, `data/species/*.json`, `docs/ANIMALS.md` | Accepted with corrections. 5 tests, ASan build clean in the worktree (leak detection off: LeakSanitizer cannot run in the Codex sandbox), re-verified in the main tree. | Code correct but dense (several statements per line): reformatted with the new `.clang-format`. Two facts corrected against the IUCN Red List: African penguin is Critically Endangered (2024 uplisting), and the domestic Bactrian camel is not assessed at all, which needed a new `NotEvaluated` status. Validation messages are terse ("X is invalid") — good enough for data authors, to be improved when the editor needs them. |
| A | World grid: heights, tiles, fences on tile edges, water, terraforming brushes, ray cast, chunk revisions; the seeded map generator (relief, boundary wall, entrance, plaza, approach road); JSON persistence of a world | `src/core/zoo/world/World.cpp`, `WorldGen.cpp`, `WorldJson.hpp/.cpp`, `tests/core/World*Tests.cpp` | Accepted as delivered. 16 tests, ASan build clean in the worktree, re-verified in the main tree. | Heights are saved in millimetres and layers run-length encoded, which keeps a 128 x 128 save small. Reformatted with `.clang-format`. A corner tile does not bump the diagonal neighbour chunk's revision; harmless so far (the renderer's normals look one vertex across a chunk border, not diagonally). |
