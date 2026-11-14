# Handoff: realism, immersion and stabilisation campaign (2026-11-13)

For the next agent, who starts without this session's context. Read this file, then
`docs/REALISM.md` (the progress log and the honest list of what is open) and
`docs/CODEX_TASKS.md` (every delegated task and how it was taken).

## 0. Session of 2026-11-14 (live status; newest first, keep this section current)

**Owner's limits, tightened later on 2026-11-14: at most 2 CPUs for compiling
(`--parallel 2`, `ctest -j2`), NO Codex agent and NO Claude subagents.** The lead does
everything alone. (Section 1's "delegate to Codex" and "most parallel use possible" no longer
hold.) Codex could not run anyway: G8c stopped at 08:56 on the Codex usage limit ("try again
at Nov 18th, 2026 10:05 PM"), its report is empty; its last state is in `log-G8c.txt` (a
`Meerkat Lookout: no place for sandstone_rock` failure in `VisitorZooPlan` it was fixing) and
uncommitted in `wt-life`.

- **G8 the zoo's plan: taken** (finished by hand by the lead from `wt-life`; commit after
  `b2ebdd2`). Families with young in every social species (88 animals), hoofstock behind
  `tall_cable_fence`/`paddock_rail`/`cable_barrier`, meerkat mound with a dish, lagoon with a
  0.3 m shelf (flamingos wade: seen), furnishing and groves that keep views and corridors
  open, a service apron to every keeper gate. Lead's own fixes: feeders searched in the front
  half first, planting problem only below the welfare band, no trunk before a viewing place
  (massive ones 5 m beyond the strip), kangaroo joeys at foot. Stall probe 5.4 % -> 1.35 %.
  The fill got dearer with 88 animals; cause was core-wide: the mutable `FindProp` dropped the
  prop buckets on every feeding tick (fixed in `Objects.cpp`, hashes unchanged; fastest fill
  2.52 s before G8, 2.05 s after, `docs/VISIT_START.md`).
  Golden hashes re-recorded. `docs/VISITOR_ZOO.md` "The zoo's plan". `wt-life` has nothing
  left to take. **Open problems 1-3 of section 5 are resolved by it** (re-measured).
- **S7 big cats: taken** (commit after `5dd19c4`). Merged by hand over S10's `sculpt.py`: the
  big-cat coat path is opt-in through `data['source_material']` (bears and birds unchanged; a
  rebuilt bear cub matches its asset but for two tangent components, a rebuilt lion matches
  S7's). `src/game/AnimalTextures.cpp` passes the alpha reference for MASK textures. Found:
  `--screenshot` PNGs keep the frame's alpha (hair looks pale in a PNG viewer; flatten to RGB
  before judging, ZOO-UP-002) and single-sided imported models are culled inside out
  (ZOO-UP-003).

- **Stopped by the lead for the new limits, work left uncommitted in their worktrees** (each
  moved onto `f77bc8f` with the earlier partial work on top; the pre-move diffs are kept as
  `.codex-work/<name>-partial.patch`): G2b `wt-plants` (brief `brief-G2b-plants-finish.md`),
  G6b `wt-audit` (`brief-G6b-audit-finish.md`; it had an ASan tree and was running save-damage
  tests), G1b `wt-ground` (`brief-G1b-ground-finish.md`: pond-corner wedge, feet at the drawn
  bank height, interleaved bench). Their briefs say what was left; with Codex forbidden the
  lead finishes them by hand (read the diff and `build-probe/`) or leaves them. G7 (sound)
  `wt-sound`, brief `brief-G7b-sound-finish.md`.
- **Committed:** `6ce4b9a` S6b elephant second pass + S11/S11b giraffe (59 pipeline tests
  pass); `5dd19c4` S10 bears.
- **G3 buildings and barriers: taken** (commit after S7), with `FenceLook::inward` wired in
  `SceneryRenderer.cpp`. Its leftovers are in `docs/CODEX_TASKS.md`.
- **G4 visitors: taken** (commit after G3) with three fixes by the lead and its measured
  cost (+3 ms a frame with 800 people, cause not found; `docs/PERFORMANCE.md`). The bench's
  "before" binary was deleted after the figures were written down; rebuild `d715179` to repeat.
- **Crowd setting (owner's request, 2026-11-14): done.** Settings > Visitors: Nobody, Few,
  Fewer, Normal (as before), More (1,200; `kMaxVisitors` raised). `docs/VISITORS.md`.
- **Next:** G5 keepers by hand (`wt-keepers`; its `VisitorZoo.cpp` call now meets G8's plan:
  drop `OpenKeeperLanes`, the gates have service aprons), then G1b, G2b, G6b, G7 by hand or
  leave them.

## 1. The mission and the owner's rules

The owner (Robert Vokac) asked for Greenhill Zoo, a **first-person walk through a zoo**, to
look and behave more like a real zoological garden: realistic animals and their motion,
natural environments, living CNA Avatar visitors and keepers, honest real-world performance
and stability. **Zoo management must not come back as the primary gameplay.**

Standing rules (from the owner, `../CLAUDE.md` and the agent's memory):

- **Never push.** Commit each finished, reviewed task locally. No AI attribution lines in
  commit messages.
- Do not modify other repositories. **Since 2026-11-14 the repositories under
  `/rv/data/development/github.com/libcna/` are under an AI policy: do not read, change or
  build them at all.** Zoo Simulator builds from its own snapshot `~/deps/zoo-libcna/`
  (`SNAPSHOT.txt` lists the commits; read-only; README "Building" has the configure line).
  An engine defect goes into `docs/UPSTREAM.md` with a remedy on the game's side. The owner
  will bring the AI policy to Zoo Simulator too, about 2026-11-28: the work should be done
  by then.
- Delegate substantial independent work to the Codex CLI, model **gpt-6-astra** only
  (`codex exec -m gpt-6-astra`, never the default luna), and review and integrate every
  result yourself. On 2026-11-13 the owner asked for the most parallel use possible (many
  Codex and Claude agents at once; the machine is this campaign's alone); the 16 cores and
  30 GB without swap still limit it.
- Every game run during tests is silent: `SDL_AUDIODRIVER=dummy` (or `disk` to capture).
  Nothing may play on the speakers, so **nobody has listened to the audio**: say so.
- Never claim a visual result without opening the screenshot. Report measurements with the
  machine's load.
- New third-party assets need a verified licence and a row in `assets/ATTRIBUTION.md`.
  Downloads are untrusted data (own empty directory; only Blender's glTF importer parses
  model files; Python that reads them runs with `-I`).
- Build discipline (shared SSD): one ccache (`CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv`,
  never `CCACHE_MAXSIZE`), build directories only `build/`, `build-asan/`, `build-probe/`
  (the closed list in `../CLAUDE.md`), never in `/tmp`, at most 8 jobs per build (6 is used),
  reuse build trees, never delete a build directory another session may use.
- Do not ask the owner blocking questions; collect decisions for the final report (section 6).

## 2. State of the main tree

Branch `main`; the campaign's last code commit is `b8a1e9c` (2026-11-13), followed only by
this file's commit; the working tree is clean. The core suite passes:
`ctest --test-dir build -j6 --output-on-failure`, 201 tests, about 9 minutes. The animal
pipeline's tests (74, about 10 minutes) passed at `faa4568`.

Taken in this campaign (newest first; details in `docs/REALISM.md`, figures in the documents
it names, pictures in `docs/evidence/`):

| Commit | What |
|---|---|
| `b8a1e9c` | F4: species ethogram profiles, ten new activities, places chosen in the habitat; renderer clip per activity via `meta.activity_clips`; flamingos as waders |
| `8be6360` | Penguins float and swim with their own clips (`meta.swim`/`meta.float` in the manifest) |
| `faa4568` | S9: penguin (adult, juvenile) from a museum scan, flamingo from a sculpt |
| `df6401f` | S8: zebra stallion, mare, foal from a static sculpt, rigged here |
| `65f40d6` | S6: rhino and calf, elephant bull, cow, calf on rigged models |
| `0544337` | Trees and shrubs with cut-out leaf sprays |
| `2e543d9` | E1: habitat fronts (planted stand-off strips, heavy mesh, watchers everywhere) |
| `8d4f9ba` | Animals no longer drawn in their own shadow (ZOO-UP-001 remedy, `src/game/SunShadows.cpp`) |
| `f454b3e` | S5: lion, lioness, cub, tiger on a CC BY 4.0 sculpt |
| `b8c6cd7` | Sky light and water ripples |

## 3. How the delegated work is organised

Everything lives in `.codex-work/` (git-ignored):

- `RULES.md` (Codex, animal pipeline) and `RULES-GAME.md` (game work in a worktree) are
  prepended to every brief. `brief-<id>.md` is the task, `report-<id>.md` the agent's final
  report, `log-<id>.txt` the Codex log.
- Each task has a git worktree `wt-<name>` with its own Release `build/` (configured with
  `-DFETCHCONTENT_SOURCE_DIR_FNA3D=<main>/build/_deps/fna3d-src`; since 2026-11-14 every build directory, the worktrees' too, points at the snapshot `~/deps/zoo-libcna/`, FNA3D included: `tools/build/use-libcna-snapshot.sh <source> <build> <log>` does it for another). Agents never commit; a
  follow-up task in the same worktree starts from a local **seed commit** that is never
  merged into main.
- Codex run (in the background, then wait for the notification):

  ```bash
  cd .codex-work && (cat RULES.md brief-X.md | codex exec -m gpt-6-astra \
      -c model_reasoning_effort=high -s workspace-write -C wt-<name> -o report-X.md - \
      > log-X.txt 2>&1); echo "exit $?"
  ```

- Integration: in the worktree `git add -N .; git diff --binary <seed-or-base> > ../X.patch;
  git reset -q`; in main `git apply -3 .codex-work/X.patch`; then build, tests, game
  screenshots, look at them, docs (`docs/CODEX_TASKS.md` row, `docs/REALISM.md`, evidence
  sheet in `docs/evidence/`), commit.
- **Golden hashes** (`tests/core/VisitStartTests.cpp`) change with any behaviour change.
  Several pending tasks re-recorded their own; record them **once, after the last
  behaviour-changing merge**:
  `./build/tests/zoo_core_tests --gtest_filter='VisitStart.*Hashes*' --gtest_also_run_disabled_tests | grep ^HASH`.

## 4. Finished work waiting for integration (review each before taking it)

Merge order suggestion: the four animal tasks first (asset-only, independent), then G3, G1,
G4, G5, then whatever of G2/G6/G7/G8 is usable; hashes last.

| Task | Worktree, diff base | State | What to do |
|---|---|---|---|
| S6b elephant second pass | `wt-pachy`, seed `1950d9a` | Done. Body albedo 82.7 % of the rhino's (was ~35 %), AO mean 0.96, bull tusks 1.51 -> 0.66 m, cow 0.36 m. Rhino files byte-identical. | Diff against the seed, run `test_pachyderms.py` + `test_clips.py`, judge in the game beside the rhino (`--stand habitat:elephant`, `--viewer --animal elephant`), commit. |
| S11b giraffe second pass | `wt-giraffe`, seed `9c0d2f0`; S11 itself is `.codex-work/giraffe.patch` | Done; reticulated coat (no green texels), hooves 15 x 25 cm, calf. The S11 giraffes were refused in the game before; judge again. | Take `giraffe.patch` + diff against the seed. Main still lacks the S11 parts: `zoogen/giraffe2.py`, `prepare_giraffe.py`, `test_giraffe.py`, `sources/giraffe`, SPECIES entries `giraffe_female`/`giraffe_juvenile`, the `giraffe2` dispatch in `build_animal.py`, and the browse exception in `test_clips.py` (`if species.startswith("giraffe"): assertGreater(eat, 1.5) else: assertLess(eat, 0.12)`). Add `"activity_clips": {"browse": "eat"}` to the giraffe manifests and species files. ATTRIBUTION row: aellis43, "Giraffe (textured)", CC BY 4.0. |
| S10 bears | `wt-zebra`, base `faa4568` | Codex says itself the bears are **not realistic** (angular shoulders and hips, artificial mouth). Four models incl. two cubs, `swim` for polar bears. Reports: `report-S10-bears.md`, `wt-zebra/build-probe/S10_report.txt`, previews `wt-zebra/build-probe/animal-previews/S10_*.png`. | Look at the previews and in the game; take only if better than the lofted bears. ATTRIBUTION row: Pebegou "Strong Bear", CC BY 4.0, polar bear labelled as derived; exclude the bear files from the MIT wildcard row. Check `meta.swim` works with the renderer like the penguin's. |
| S7 big cats finish | `wt-cats`, seed `700b81a` | Codex: **visually incomplete** (male head/mane, tiger face not at photo quality). Report `report-S7-big-cats-finish.md`, inventory in `wt-cats/build-probe/animal-previews/`. | Judge previews/game against main's cats. If taken and it uses `alphaMode: MASK` (hair cards): `src/game/AnimalTextures.cpp` `GiveMipmaps` passes no alpha-test reference; fix that on the game side. |
| G1 ground and water edges | `wt-ground`, base `0544337` | Done visually; report `report-G1-ground.md`. Needs its `src/game/ZooGame.cpp` hunk (the "ground detail" pass), otherwise the layer is not drawn. Known defect: light wedges at pond corners (fix designed in the report). No interleaved before/after bench; loading-time gain not shown under load. | Bench HEAD vs merged on a quiet machine (3 runs each), decide `kShoreLiftMaxM` (feet may float/sink up to 0.2 m on banks). |
| G3 buildings, barriers, signs | `wt-built`, base `0544337` | Done; report `report-G3-built.md`, 57 before/after pairs in `wt-built/build-probe/shots/built/`. Fences +57 % triangles; fps comparison inconclusive under load. | Bench before taking; hand-merge `PropModels.cpp` with G2 if G2 is taken (G3 touched only non-plant functions plus two includes). Picnic tables are never placed (all rejected by `CheckProp`); `FenceLook::inward` is unused. |
| G4 visitors walk and stand | `wt-visitors`, base `0544337` | Core done (paces, steering, groups abreast, giving way, `MakeRoom`, watchers aim at animals); pose layer done but phone/camera/crouch/stoop **never seen**. Report `report-G4-visitors.md`. Full ctest and an after bench **not run**. | Run the full suite and the 800-people bench (`--people 800 --hour 11 --seed 7 --bench --frames 300`, interleaved with `wt-visitors/build-probe/zoo-simulator-before`). The player is passed to the core as a bystander (not saved): accept or remove. |
| G5 keepers | `wt-keepers`, base `0544337` | Core `zoo::Staff` (9 crew, rounds, feeding hours, saved and hashed) done and tested in part; game renderer `KeeperRenderer` compiled, seen once (a keeper wading in the lagoon; a fix added, not re-run). Report `report-G5-keepers.md`. Hashes, full ctest, bench, `docs/KEEPERS.md`: not done. | Decide `OpenKeeperLanes` (it removes stand-off planting before keeper gates because 6 of 14 habitats' gates open onto planted strips; better fixed in the plan, see G8). `--stand keeper:<id>[:<activity>]` exists for screenshots. |
| G2 plants (distance forms, palms, ferns, hedges) | `wt-plants`, base `0544337` | **Stopped by the API limit, no report.** Uncommitted work, last state "built cleanly", shots in `wt-plants/build-probe/`. | Read the diff and shots; finish or drop. Conflicts with G3 in `PropModels.cpp`. |
| G6 reliability and sanitizer audit | `wt-audit`, base `0544337` | **Stopped by the API limit, no report.** It was correcting `docs/RELIABILITY.md`. | Read its diff; take tests and the document if sound. Add the lead's ASan smoke result: the game ran clean under ASan once (`--stand habitat:lion --frames 30`, `build-asan`). |
| G7 audio measured audit | `wt-sound`, base `0544337` | **Stopped by the API limit, no report.** It was looking at spectrograms of a new set. | Read its diff and `build-probe/`; remember nobody can listen here. |
| G8 zoo plan and young animals | `wt-life`, base `0544337` | **Stopped by the API limit, no report.** Diff so far: `data/construction/fences.json`, `src/core/zoo/sim/VisitorZoo.cpp` (+893/-240: among other things a lagoon with a 0.3 m shallow shelf and a deep heart), `tests/core/{CatalogTests,VisitorZooHabitatTests}.cpp`. | Most valuable of the four: the flamingos need its shallows, the keepers need gate approaches. Finish (young animals, keeper gates) and test; conflicts with G5 in `VisitorZoo.cpp`. |

Old worktrees with nothing left to take: `wt-audio`, `wt-species`, `wt-vzoo`, `wt-world`,
`wt-birds` (S9, taken), `wt-behave` (F4, taken; has `build-asan/`). Their `build*/`
directories (several GB) can be removed once the owner agrees; list them in the final report.
Kept patches: `zebra.patch`, `birds.patch`, `giraffe.patch`, `S6-final.patch`, `behave.patch`,
`e1.patch`, `S6-snapshot.patch`.

## 5. Open problems found by the lead

1. **Animals stall in thickets** (measured, not fixed; `docs/ANIMAL_BEHAVIOUR.md` "In the
   game"). 5.4 % of the ticks an animal is under way it does not move (3.0 % before F4); a
   tiger stands pinned between a `bamboo_clump` and a `tropical_fern` at (245.9, 52.4) for
   most of 6000 ticks. Cause: the navigator is tile-based and knows no props, while
   `AdvanceMovement` (`src/core/zoo/animals/AnimalMovement.cpp`) stops at blocking props;
   F4 prefers shade, which is the thicket, and stops an animal crowded by a companion instead
   of squeezing past. Remedy to build: tiles a blocking prop fills are impassable for animals
   (or `RoutinePlace`/`ClearPlace` in `AnimalHabitat.cpp` reject places in pockets and
   remember unreachable ones), then turn the probe into a regression test. The probe is
   `.codex-work/probe-stall-ZzProbeTests.cpp` (copy to `tests/core/` to run it:
   `--gtest_filter='ZzProbe.*' --gtest_also_run_disabled_tests`). Re-measure after G2/G8,
   which change the planting.
2. **Flamingos keep to the bank** until the lagoon has shallows (G8). After G8, check
   they stand in the 0.3 m shelf (`--stand habitat:flamingo --ticks 3000`).
3. **Meerkat looked chest-deep at its pond** on a screenshot although the core keeps all
   meerkats on dry tiles: probably the bank slope; re-check after G1.
4. No model has clips for preen, dust bathe, dig or sentry duty: those activities stand in
   `idle` (or the manifest's `activity_clips` choice).
5. Eight of fifteen species are still the first-generation lofted models (camel, kangaroo,
   meerkat, ostrich, wolf, and the bears/giraffe until S10/S11b are taken). Next candidates
   for Codex sculpt/scan tasks; check each source's licence and authorship carefully.

## 6. Decisions for the owner (collect for the final report; do not block on them)

- ZOO-UP-001, the cna-engine shadow-map defect (`docs/UPSTREAM.md`): remedied in the game.
- The polar bear's CC BY-SA source (`build-probe/model-research/polarbear-cedes-bysa/`) is
  unused until the owner decides on share-alike assets.
- Rhino and elephant sources: CC BY 4.0 as published, authorship unconfirmed
  (`assets/ATTRIBUTION.md`): decide before distribution.
- The penguin is a Magellanic specimen's shape painted as an African penguin, labelled so.
- Keeper lanes vs. a proper gate approach (G5/G8); AutoCare kept as a net behind the keepers.
- Worktree build directories to delete after the campaign (section 4).

## 7. Commands

```bash
cd /rv/data/development/github.com/openeggbert/zoo-simulator
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
cmake --build build --target zoo-simulator zoo_core_tests --parallel 6   # build/ is RelWithDebInfo
ctest --test-dir build -j6 --output-on-failure                           # ~9 min; do not rebuild meanwhile
OPENBLAS_NUM_THREADS=1 PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools/animals/tests  # ~10 min

# The game, real GPU, no window, no sound; run from build/ (build/assets is a copy made by the
# build: rebuild after changing assets/). Filter the GLTF-206 warnings.
cd build
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy timeout 180 ./zoo-simulator \
    --stand habitat:lion --ticks 3000 --frames 40 --screenshot ../build-probe/shots/x/lion.png 2>&1 | grep -v WARN
#   --viewer --animal <id> --clip walk --angle 35 --frames 30 --screenshot ...   one model alone
#   --bench --frames 300   fps, frame-time percentiles, draw calls, memory
#   --view x,z,yawDeg,pitchDeg  --hour H  --people N  --seed S  --without animals|people|scenery|...
#   --stand board:N | habitat:<species> (| keeper:<id> after G5)
```

The end of a game run prints `looking at "<name>, <species>" (<activity>)`. The core runs at
20 ticks per second. Shell note: in this environment every shell command starts in another
directory; begin each with `cd`.
