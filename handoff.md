# Handoff: Zoo Simulator (state of 2026-11-19)

For the next agent, who starts without any earlier context. Read this file first, then
`docs/REALISM.md` (progress log and the honest list of what is open), and the document of
whatever you take up (`docs/VISITOR_ZOO.md`, `docs/ANIMAL_BEHAVIOUR.md`, `docs/VISITORS.md`,
`docs/WEB.md`, `docs/PERFORMANCE.md`, `docs/CODEX_TASKS.md` for the history of delegated tasks).

## 0. Start here

- **The game:** Greenhill Zoo, a first-person walk through a zoo (C++23, the simulation core in
  `src/core/zoo/`, the game on CNA / cna-engine in `src/game/`). Zoo management must not come
  back as the primary gameplay.
- **State (end of 2026-11-14):** branch **`develop`** (the owner switched to it), pushed to
  `origin/develop` at the owner's request; `main` = `origin/main` = `0b13f7c` is behind. New on
  develop this session: keepers G5 (`e22d473`), animals' routes kept off props (`00e269f`),
  reliability audit part 1 with a save/load defect fixed (`7010638`), plants G2c (`f8d0adf`),
  ground and water edges G1c (`6985b10`), and the audit's walker corrections, start-up marks and
  `zoo_game_tests` (last commit). The current complete suite has 265 active tests and 21 intended
  disabled measurement/diagnostic tests; all active tests pass, including 13 walker tests.
- **The owner restarted the machine at about 20:00:** game instances of the lead and four
  subagents took 99 % of the GPU. The CPU lock covered builds, tests and benches but **not game
  runs**. New rule: **only one rendering `zoo-simulator` on the machine at a time**, every game
  run under the same lock (`flock .codex-work/cpu.lock`), always with `timeout` and a short
  `--frames`; no long runs under gdb. At most 2 subagents (owner's last word).
- **The owner's complaint: the game takes about 2.5 GB of RAM.** Memory is now measured by kind
  (`docs/RELIABILITY.md`): in this no-GPU sandbox the identical software-GL run fell 3,873 ->
  3,381 -> 2,870 MB after unused original animal textures and level-zero recovery shadows were
  released, then oversized animal atlases were capped at their 1K mip. That is 1,003 MB (25.9%)
  below the original run. Lion ownership output is pixel-identical; four representative 2K/1K
  viewer comparisons show no visible loss at 720p, and focused tests cover both ownership and the
  cap. The visit's shadow map is now 2K instead of 4K: its matched loading phase saves another
  96 MB, four zoo views show no visible loss, and the latest whole-run sample is 2,687 MB. The
  dual plant far forms are about 74-75 MB but buy about 1 ms/frame, so normal visits keep them;
  full-detail diagnostic mode now omits that unused second form. The under-1-GB target is not
  met. Visitor terrain now discards its 100 MB logical CPU edit cache; the ground mark fell
  875 -> 826 MB (49 MB resident), the latest whole-run sample is 2,682 MB, and editor memory is
  unchanged. An in-place save reload used to spike to about 3.1 GB because old software-GL static
  resources remained resident while replacements were built. This is now fixed on the game's
  side: a visit compares every render-relevant terrain, fence and placed-object field, keeps the
  uploaded ground and scenery when they are identical, and keeps animal species models while
  clearing their per-animal animation state. A two-reload run did no static or animal loading in
  either `Adopt`, ended at 3,033 MB instead of the earlier 3,120 MB software-GL sample, and its
  second reload added 9 MB. An editor zoo or any actual geometry edit still rebuilds. Audio now
  catalogues all 53 recordings but decodes a take only on first play: its loading phase fell
  46 -> 0 MB; a 60-frame run decoded 3 recordings,
  played 4 audible voices and had no failure. The noisy whole-run sample was 2,711 MB, so do not
  claim a total-peak cut from it. Do not quote the sandbox's absolute number as a Radeon
  measurement.
  The complete suite after these changes is 265 active tests passed, with 21 long-running
  measurement/diagnostic tests disabled as intended.
- Animal models are now submitted only when a generous body-and-shadow sphere intersects the
  main camera. The count fell from 88 everywhere to entrance 72, lion 7, meerkat 17, penguin 15
  and giraffe 46. All five screenshots were inspected (including edge bodies and the tallest
  shadows), and the 18 game-side tests pass; no frame-time saving is claimed because animals
  were already under 1 ms. A current AddressSanitizer lion-view smoke also passes
  (`docs/PERFORMANCE.md`, 2026-11-19).
- **Stopped mid-work when the machine restarted (uncommitted, in their worktrees; no reports):**
  - `wt-sound`: G7c audio audit (brief `.codex-work/brief-G7c-sound-develop.md`), 28 files.
  - `wt-visitors`: P1 visitors' frame cost (brief `brief-P1-visitor-cost.md`), 2 files.
  - `wt-life`: G8d plan details (brief `brief-G8d-plan-details.md`: ferns in the lagoon's
    water, 2 m walks, a tree inside a barrier, the meerkat stand on the gate), 6 files. Its Red
    Dune rail finding was fixed on develop on 2026-11-19 and the walker test is active again.
  - `wt-plants` (reused): A1 animal activity clips (brief `brief-A1-activity-clips.md`), 31 files.
  Look at each diff before going on; none is reviewed. Their earlier finished work is in develop.
- **Deadline:** about **2026-11-28** the owner puts Zoo Simulator under the same AI policy as
  the libcna repositories; the work should be done by then.

## 1. The owner's rules (all still in force)

- **Resources (since 2026-11-14): at most 2 CPUs for compiling** (`--parallel 2`, `ctest -j2`,
  Blender `-t 1`/`-t 2`), **no Codex agent**; Claude subagents: none in the morning, then up to 2, then up to 4,
  and at the end of 2026-11-14 **at most 2** again (the owner's last word; ask in a new session).
  (Older documents speak of delegating to Codex `gpt-6-astra` and of "maximum parallelism":
  that no longer holds.)
- **Never push unless the owner asks** in so many words (asked once on 2026-11-14 evening for
  develop); a permission given once does not carry
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

### 4.1 to 4.3: done on develop (2026-11-14)

Keepers (`docs/KEEPERS.md`; open: no collision with animals and visitors, dangerous habitats
never entered), ground and water edges (`docs/GROUND.md`; open: steep banks at deep pools, props'
collision floor still at the simulation's height in `Walker.cpp`), plants (`docs/PLANTS.md`;
open: far cut-out edges aliased, reeds unused, fill cost of alpha-tested cards). The new first
priority is memory (section 0).

### 4.4 Reliability audit (G6b): merged, see `docs/RELIABILITY.md` top

`.codex-work/wt-audit` (on `f77bc8f`, uncommitted; it has a 4 GB ASan tree), brief
`brief-G6b-audit-finish.md`; it was correcting `docs/RELIABILITY.md` and running save-damage
tests. Take its tests and document if sound; add the known ASan smoke (the game ran clean once:
`--stand habitat:lion --frames 30` in `build-asan`).

The stopped wall probe was rerun directly at the gift shop's west wall. At the body's closest
valid centre, the 0.3 m camera near plane cut away a quarter of the wall at a 75-degree view;
0.1 m keeps the whole wall visible. The before/after contact pictures and a normal lion view
were inspected offscreen (2026-11-19).

Root `build-asan/` was rebuilt on current `develop` (Debug, AddressSanitizer). A targeted 30-test
set covering births/life, the 12,000-step stall regression, every damaged-save test and all
thirteen Walker tests passed in 626.21 s; `--stand habitat:meerkat --ticks 3000 --frames 3`
also completed cleanly with 88 animals and 624 visitors. Leak detection itself cannot run under
this managed session's ptrace wrapper; UBSan and the entire current suite remain unrerun
(2026-11-19).

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
- Blocking-prop pockets are now rejected by `ClearPlace`: every routine destination needs two
  body-wide exit segments clear of fences, water and blocking props. Seed 1 fell from 4,183 of
  280,044 stalled moving ticks (1.49 %) to 2,994 of 270,133 (1.11 %), with no continuous stall
  of 300 ticks. The former probe is the active regression
  `AnimalMovementTests.VisitorZooAnimalsDoNotRemainStalledInPropPockets` in
  `tests/core/AnimalStallTests.cpp` (2026-11-19).
- The changed animal positions exposed a visitor leaving a picnic table by an unrelated path
  across 19 ticks of lawn. When a generic nearest-path search would carry somebody more than
  3.5 m from a nearby bench, `Stepper::Route` now uses that bench's recorded path access;
  `VisitorDayTests.ADayInTheStarterZoo` is back to zero astray visitor-ticks (2026-11-19).

### 4.7 Performance

- Loading about 5.5 s, 2.4 s of it painting the ground (in the browser 7 to 18 s, one thread).
- 800 visitors cost about +3 ms a frame since G4, cause not found (`docs/PERFORMANCE.md`;
  rebuild `d715179` for the "before" binary).
- During the visit's fill the visitors' routing and watching are now the largest part (gdb
  stack sampling, `docs/VISIT_START.md`; there is no `perf` on this machine).

### 4.8 Plan details left from G8 (`docs/VISITOR_ZOO.md` "The zoo's plan")

No dry moat or ha-ha in the catalog; 48 paved tiles on 2 m walks (a zoo's side path is
2.5 to 3 m); a slender tree may stand 1 m inside a barrier behind a strip (Elephant Valley);
area per animal is an estimate, not EAZA/AZA figures.

`--stand habitat:<species>` now rejects keeper gates and buildings in the sight line. The
meerkat view moved from `(177.0, 263.2)`, looking through mesh into the shelter wall, to
`(169.2, 269.0)`, with an unobstructed animal in the centre; the before/after offscreen images
were inspected and the 18 game-side Walker/texture tests passed (2026-11-19).

The same command also rejects positions overlapping the exact solid radii used by `Walker`,
including its 0.28 m body radius and furniture which simulation visitors may pass through.
All fourteen species kept identical requested/final coordinates to 0.1 m in a zero-tick
offscreen smoke, so the former 42 set-downs inside furniture are resolved (2026-11-19).

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
