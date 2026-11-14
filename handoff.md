# Handoff: Zoo Simulator (state of 2026-11-14, evening)

For the next agent, who starts without any earlier context. Read this file first, then
`docs/REALISM.md` (progress log and the honest list of what is open), and the document of
whatever you take up (`docs/VISITOR_ZOO.md`, `docs/ANIMAL_BEHAVIOUR.md`, `docs/VISITORS.md`,
`docs/WEB.md`, `docs/PERFORMANCE.md`, `docs/CODEX_TASKS.md` for the history of delegated tasks).

## 0. Start here

- **The game:** Greenhill Zoo, a first-person walk through a zoo (C++23, the simulation core in
  `src/core/zoo/`, the game on CNA / cna-engine in `src/game/`). Zoo management must not come
  back as the primary gameplay.
- **State:** branch `main`, working tree clean, all 221 core tests pass. Three commits are
  **not pushed** (`7ebabef` pace and map travel, `7588339` binoculars, `fb8b864` web build):
  push only when the owner asks.
- **Next step, in this order:** (1) the keepers, G5 (section 4.1); (2) plants' distance forms,
  G2b (4.3); (3) ground and water edges, G1/G1b (4.2); then 4.4 to 4.8 as time allows.
- **Deadline:** about **2026-11-28** the owner puts Zoo Simulator under the same AI policy as
  the libcna repositories; the work should be done by then.

## 1. The owner's rules (all still in force)

- **Resources (since 2026-11-14): at most 2 CPUs for compiling** (`--parallel 2`, `ctest -j2`,
  Blender `-t 1`/`-t 2`), **no Codex agent and no Claude subagents**: the lead works alone.
  (Older documents speak of delegating to Codex `gpt-6-astra` and of "maximum parallelism":
  that no longer holds.)
- **Never push unless the owner asks** in so many words; a permission given once does not carry
  over to the next work. Commit each finished, reviewed piece locally. No AI attribution lines
  in commit messages.
- **libcna is off limits:** do not read, change or build anything under
  `/rv/data/development/github.com/libcna/`. Zoo Simulator builds from its own read-only
  snapshot `~/deps/zoo-libcna/` (`SNAPSHOT.txt` lists the commits; README "Building" has the
  configure line; `grep github.com/libcna <build>/CMakeCache.txt` may show only the FNA3D URL).
  An engine defect goes into `docs/UPSTREAM.md` with a remedy on the game's side.
- **Build discipline** (`../CLAUDE.md`, shared SSD): one ccache
  (`CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv`, never `CCACHE_MAXSIZE`); build directories
  only from the closed list (`build/`, `build-asan/`, `build-probe/`, ...); never build in
  `/tmp`; reuse build trees; never delete a build directory another session may use.
- **Silence:** every game run uses `SDL_AUDIODRIVER=dummy` (or `disk` to capture). Nobody has
  ever listened to the audio: say so.
- **Look before you claim:** never describe a picture you have not opened. Screenshots keep the
  frame's alpha: flatten to RGB before judging (`ZOO-UP-002`). Report timings with the machine's
  load (other agents share it; loads of 10 to 28 were seen).
- New third-party assets need a verified licence and a row in `assets/ATTRIBUTION.md`;
  downloads are untrusted data (own empty directory, Python reading them with `-I`).
- Ask the owner nothing blocking; collect decisions in section 6.

## 2. What exists (main, newest first)

| Commit | What |
|---|---|
| `fb8b864` | Browser build (Emscripten, WebGL 2): `docs/WEB.md` |
| `7588339` | Binoculars: `B` or the right mouse button, 7 x, card of an animal up to 300 m |
| `7ebabef` | Walk 4.4 m/s, run 8.8 m/s (stride 1.5 m); the map (`M`) frees the mouse and marks 20 places to travel to at a click (`ZooGame::AddMapPlaces`, `VisitorHud::Map`) |
| `a08e4eb` | G8, the zoo's plan: families with young (88 animals), hoofstock behind cables and timber, meerkat mound with a dish, lagoon with a 0.3 m shelf, furnishing and groves keeping views and corridors open, a service apron to every keeper gate; the prop-bucket fix in `Objects.cpp` (visit fill 2.52 -> 2.05 s) |
| `734073c` | Settings > Visitors: Nobody, Few, Fewer, Normal, More (1,200) |
| `a6bc3fb` | G4 visitors walk like people (steering, own paces, groups abreast, room to stand) |
| `d715179` | G3 buildings, barriers, signs, boards |
| `12cb68f`, `5dd19c4`, `6ce4b9a` | S7 big cats second pass, S10 bears with cubs, S6b elephant, S11/S11b giraffe |
| `b8a1e9c` and earlier | F4 species routines, penguins swim, S5-S9 animal models, E1 habitat fronts, foliage, sky light (`docs/REALISM.md` "Done") |

Controls: `W A S D` walk, mouse look, `Shift` toggles running, `E` reads a board, `B` / right
button binoculars, `M` map (click a marker to travel), `Esc` pause, `F5` save, `F4` diagnostics.

**Tested only by script, never by a person with a real mouse:** map travel, binoculars, the
faster pace. **The web build** ran only in headless Chrome driven over the DevTools protocol
(`build-probe/web/drive.mjs`, `drive2.mjs`); never in a visible browser (fps, pointer lock,
sound unknown); saves there live only in the page's memory.

## 3. Verification recipe for any change

1. Build: `cmake --build build --target zoo-simulator zoo_core_tests --parallel 2`.
2. Focused tests, then the full suite `ctest --test-dir build -j2 --output-on-failure`
   (221 tests, about 5 to 7 minutes; do not rebuild while it runs).
3. **Golden hashes** (`tests/core/VisitStartTests.cpp`) change with any behaviour change of the
   core: print with
   `./build/tests/zoo_core_tests --gtest_filter='VisitStart.*Hashes*' --gtest_also_run_disabled_tests | grep ^HASH`,
   paste them, add a line to the comment above them, record the table in the document of the
   change. A pure caching or speed fix must leave them unchanged (that is its proof).
4. Look in the game (`--stand`, `--view`, `--script`, section 7) and open every screenshot.
5. Docs: the feature's document, `docs/REALISM.md` (Done / Open), an evidence sheet in
   `docs/evidence/` for visible changes, this file.
6. Format: core files follow `.clang-format` (`clang-format-19 -i`); **the game files under
   `src/game/` were never formatted with it** — do not run it on them (it rewrites unrelated
   lines); match the surrounding style by hand.

## 4. Remaining work (priority order)

### 4.1 Keepers (G5) — the most visible gap

`.codex-work/wt-keepers` (base `0544337`, far behind main; uncommitted, about 4,100 lines with
the new files). Report: `.codex-work/report-G5-keepers.md`. Content: core `zoo::Staff` (9 crew,
rounds, feeding hours, saved and hashed; `src/core/zoo/staff/*`), hooks in `Zoo.*`,
`ZooSave.cpp`, one staffing call in `VisitorZoo.cpp`, a game `KeeperRenderer`, and
`--stand keeper:<id>[:<activity>]` for screenshots. Seen once in the game (a keeper wading in
the lagoon; a fix was added and never re-run). Not done: hashes, full suite, bench,
`docs/KEEPERS.md`.

How to take it: export its diff (`git -C wt-keepers add -N .; git -C wt-keepers diff --binary
HEAD > ../G5.patch; git -C wt-keepers reset -q`) and apply by hand, file by file: main has
since changed `VisitorZoo.cpp` (G8), `ZooGame*.cpp` and `ZooGameLook.cpp` (map, binoculars), so
`git apply -3` will conflict. **Drop `OpenKeeperLanes`** (it cleared planting before the gates):
since G8 every gate has a dry service apron (`EveryKeeperGateHasADryConnectedApproach`). Make
keepers use the aprons, check they never stand in water or planting, keep AutoCare as a safety
net, record the hashes, write `docs/KEEPERS.md`, measure the frame cost with
`--bench --frames 300` interleaved before/after.

### 4.2 Ground and water edges (G1, G1b)

`.codex-work/wt-ground` (moved onto `f77bc8f`; G1 plus the unfinished G1b, uncommitted).
Reports: `report-G1-ground.md` (G1b's report is empty), brief `brief-G1b-ground-finish.md`.
G1 is **not in main**. Needs its `ZooGame.cpp` "ground detail" pass hunk or nothing is drawn.
Left: light wedges at pond corners (fix designed in the report), feet at the drawn bank height
(`kShoreLiftMaxM`, feet float or sink up to 0.2 m), an interleaved before/after bench.
Re-check afterwards that the meerkats do not look chest-deep at their (now two-tile) dish.

### 4.3 Plants (G2b): distance forms, palms, ferns, hedges

`.codex-work/wt-plants` (on `f77bc8f`, uncommitted, about 2,000 lines, "built cleanly", shots
in `wt-plants/build-probe/`), brief `brief-G2b-plants-finish.md`. Plants are 1.93 of the
scenery's 2.18 million triangles; cut-out leaves cast no shadow; far edges are pixelated.
Conflicts with G3 (taken) in `src/game/PropModels.cpp`: merge by hand. Re-run the stall probe
after it (4.6).

### 4.4 Reliability audit (G6b)

`.codex-work/wt-audit` (on `f77bc8f`, uncommitted; it has a 4 GB ASan tree), brief
`brief-G6b-audit-finish.md`; it was correcting `docs/RELIABILITY.md` and running save-damage
tests. Take its tests and document if sound; add the known ASan smoke (the game ran clean once:
`--stand habitat:lion --frames 30` in `build-asan`).

### 4.5 Audio audit (G7)

`.codex-work/wt-sound` (on `f77bc8f`, uncommitted), brief `brief-G7b-sound-finish.md`; it was
comparing spectrograms of a new set. Remember nobody can listen here; the bear growl is known
to be quiet in the mix.

### 4.6 Animals

- First-generation stylised models still: wolf, camel, meerkat, ostrich, kangaroo; bears are
  sculpt-based but crude (`docs/ANIMALS.md`; pipeline in `tools/animals/`, its tests
  `python3 -m unittest discover -s tools/animals/tests`, about 10 minutes).
- No juvenile model for meerkat, ostrich, wolf, camel, kangaroo, flamingo (drawn as scaled
  adults); the brown/polar bear cub models exist but are unused (bears live as pairs).
- No clips for preen, dust bathe, dig, sentry duty (they stand in `idle`).
- Animals still stall where blocking props make pockets: 1.35 % of moving ticks (one wolf 820
  of 6,000 ticks). The navigator is tile-based and knows no props. Remedy: tiles a blocking prop
  fills impassable for animals, or `RoutinePlace`/`ClearPlace` (`AnimalHabitat.cpp`) rejecting
  pocket places; then turn `.codex-work/probe-stall-ZzProbeTests.cpp` into a regression test
  (copy to `tests/core/`, run with `--gtest_filter='ZzProbe.*' --gtest_also_run_disabled_tests`).

### 4.7 Performance

- Loading about 5.5 s, 2.4 s of it painting the ground (in the browser 7 to 18 s, one thread).
- 800 visitors cost about +3 ms a frame since G4, cause not found (`docs/PERFORMANCE.md`;
  rebuild `d715179` for the "before" binary).
- During the visit's fill the visitors' routing and watching are now the largest part (gdb
  stack sampling, `docs/VISIT_START.md`; there is no `perf` on this machine).

### 4.8 Plan details left from G8 (`docs/VISITOR_ZOO.md` "The zoo's plan")

No dry moat or ha-ha in the catalog; 48 paved tiles on 2 m walks (a zoo's side path is
2.5 to 3 m); a slender tree may stand 1 m inside a barrier behind a strip (Elephant Valley);
area per animal is an estimate, not EAZA/AZA figures; `--stand habitat:meerkat` can pick the
keeper gate's mesh.

## 5. Worktrees in `.codex-work/` (git-ignored)

Still holding work to take: `wt-keepers` (4.1), `wt-ground` (4.2), `wt-plants` (4.3),
`wt-audit` (4.4), `wt-sound` (4.5). Briefs `brief-<id>.md`, reports `report-<id>.md`, logs
`log-<id>.txt` beside them.

Nothing left to take (their build trees can go once the owner agrees; sizes of 2026-11-14):
`wt-life` 598 MB (G8), `wt-built` 716 MB (G3), `wt-visitors` 715 MB (G4), `wt-cats` 251 MB (S7),
`wt-zebra` 272 MB (S10, S8), `wt-pachy` 279 MB (S6b), `wt-giraffe` 338 MB (S11b), `wt-birds`
158 MB, `wt-behave` 349 MB, `wt-audio` 221 MB, `wt-species` 469 MB, `wt-vzoo` 360 MB,
`wt-world` 293 MB. Kept patches: `G8.patch`, `zebra.patch`, `birds.patch`, `giraffe.patch`,
`S6-final.patch`, `behave.patch`, `e1.patch`, `S6-snapshot.patch`.

Worktree build trees point at the snapshot too; `tools/build/use-libcna-snapshot.sh <source>
<build> <log>` re-points another one.

## 6. Decisions for the owner (collect; do not block)

- Push the three local commits (section 0)?
- Licences before any distribution: rhino and elephant sources are CC BY 4.0 as published but
  their authorship is unconfirmed (`assets/ATTRIBUTION.md`); the polar bear's CC BY-SA source
  (`build-probe/model-research/polarbear-cedes-bysa/`) is unused until share-alike is decided;
  the penguin is a Magellanic specimen's shape painted as an African penguin (labelled so).
- A top-level `build-web/` for the browser build (now in `build-probe/web/`, 594 MB, because the
  closed list of build directories has no web entry).
- Deleting the finished worktrees' build trees (section 5, about 5 GB).
- ZOO-UP-001 (cna-engine shadow map), ZOO-UP-002, ZOO-UP-003 (`docs/UPSTREAM.md`): remedied
  on the game's side; for the engine's owner.

## 7. Commands

```bash
cd /rv/data/development/github.com/openeggbert/zoo-simulator
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
cmake --build build --target zoo-simulator zoo_core_tests --parallel 2   # build/ is RelWithDebInfo
ctest --test-dir build -j2 --output-on-failure                           # 221 tests, ~5-7 min

# The game: real GPU, no window, no sound; run from build/ (build/assets is a copy made by the
# build: rebuild after changing assets/). Filter the GLTF warnings.
cd build
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy timeout 180 ./zoo-simulator \
    --stand habitat:lion --ticks 3000 --frames 40 --screenshot ../build-probe/shots/x/lion.png 2>&1 | grep -v WARN
#   --view x,z,yawDeg,pitchDeg   (yaw 180 looks north; to the right the heading falls)
#   --viewer --animal <id> --clip walk --angle 35 --frames 30 --screenshot ...   one model alone
#   --bench --frames 300   fps, frame-time percentiles, draw calls, memory
#   --hour H  --people N  --seed S  --without animals|people|scenery|...  --menu (start at the title)
#   --script FILE --lockstep --frames N   scripted input, 1/60 s a frame (src/game/InputScript.hpp):
#       "30 key M", "40 click 807 350", "31 hold W", "45 shot ../build-probe/shots/x/a.png"
#   Window pixels are 1280 x 720: take click coordinates from a full-size screenshot, not
#   from a shrunken contact sheet.

# The plan from above as a picture, and the plan's figures:
./build/tests/zoo_core_tests --gtest_filter='VisitorZooPlanTests.DISABLED_WritePlanImage' --gtest_also_run_disabled_tests
./build/tests/zoo_core_tests --gtest_filter='VisitorZooTests.DISABLED_PrintPlan' --gtest_also_run_disabled_tests
# Visit-fill timing (interleave before/after binaries, report the load):
./build/tests/zoo_core_tests --gtest_filter='VisitStart.DISABLED_FillTiming' --gtest_also_run_disabled_tests
```

Browser build: `docs/WEB.md` (configure once with `emcmake` into `build-probe/web/`; rebuild
`cmake --build build-probe/web --target zoo-simulator --parallel 2` after `source
~/emsdk/emsdk_env.sh`; about a minute after a game change). To try it headless: serve
`build-probe/web/web-package/` with `python3 -m http.server 8765 --bind 127.0.0.1` and run
`~/emsdk/node/24.19.0_64bit/bin/node build-probe/web/drive2.mjs
'http://127.0.0.1:8765/?arg=--stand&arg=habitat:elephant&arg=--frames&arg=1000000' shots`
from `build-probe/web/` (Chrome flags `--use-angle=gl-egl --ozone-platform=headless`; game
arguments go in the URL as `arg=`). Stop the server and delete `build-probe/web/chrome-profile`
afterwards.

The end of a game run prints `looking at "<name>, <species>" (<activity>)`; automated runs
also print `zoo: map: N places` and `zoo: went to <place>`. The core runs at 20 ticks a second;
a calendar year has 48 days (a week ages an animal 0.146 years). Shell note: each shell command
may start in another directory; begin each with `cd`.
