# Reliability: what was run, what was found, what was changed

**Merged into develop so far (2026-11-14, lead):** the core corrections (`AnimalLife.cpp`: the
birth loop no longer runs past an invalidated end iterator, and a litter keeps the room reserved
for other mothers; `ZooSave.cpp`: a failed read is an error, not an exception), the new core
tests (`AnimalBirthCapacityTests`, `ZooSaveDamageTests`, `VisitStartCostTests`,
`VisitorZooHabitat.SaveLoadInTheMiddleOfTheVisit`), the board and saved-pose corrections of
`ZooGameLook.cpp` and `ZooGameVisit.cpp`, and `tools/bench/`; after the ground task also
corrections 1 and 2 of `Walker.cpp`, `StartMark` in `ZooGame.cpp`, the model timing of
`AnimalRenderer.cpp` and the `zoo_game_tests` target with `tests/game/` (it links
`GroundPaint.cpp` now: the walker stands on the drawn ground). On develop 13 walker tests pass;
`StandsOnTheGroundWhereTheVisitBeginsAndWalksAtAStroll` was shortened for the brisker pace (the
body ran into the forecourt's fountain). On 2026-11-19
`NoHabitatAndNoPlantedStripIsEnteredFromAnySide` was re-enabled after its real plan defect was
fixed: every actual edge from public ground to planting is railed, including Red Dune's irregular
service-apron corner.
The historical sanitizer runs described below were made in the audit's tree. A current-develop
AddressSanitizer pass was added on 2026-11-19: the existing root `build-asan/` was rebuilt as
Debug with `ZOO_SANITIZE=address`; 30 targeted birth/life, animal-stall, damaged-save and Walker
tests passed in 626.21 s. This included all five `ZooSaveDamageTests` (the broad wrong-type
mutation took 536.98 s), all thirteen Walker tests and the 12,000-step Visitor Zoo stall test.
A real offscreen game run, `--stand habitat:meerkat --ticks 3000 --frames 3`, also completed
cleanly with 88 animals and 624 visitors. Both used
`ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1`. A separate minimal game run with
`detect_leaks=1` could not produce a leak verdict in this managed session: LeakSanitizer stopped
itself with `LeakSanitizer does not work under ptrace`. UBSan and the entire current suite were
not rerun; do not read this targeted ASan pass as either of those claims.

**J26 follow-up (2026-11-20):** managed breeding now projects the minimum litter into the
actual home habitat before conception, including shelter, space and the minimum litters already
reserved by every species there. At birth, a larger random litter is capped to the remaining
habitat capacity; if even the minimum no longer fits, birth waits. A real small-shelter regression
caps an ostrich clutch at one chick and proves that a full habitat starts no further pregnancy.
The week test is strict after births now: seed 1 ends day 7 with 91 animals and three births,
with every habitat still sheltered and at full space. Short and 6,000-step hashes for seeds 1–3
are unchanged. All 274 active CTest cases pass, including the 365.49-second animation pipeline;
21 diagnostics remain disabled.

**Found while merging:** `SaveLoadInTheMiddleOfTheVisit` failed on develop for seed 2: 100 steps
after a load the zoo was no longer the saved one. The number of people watching each habitat,
which steers where the animals glance (`LookAtInterest`), was an input "not saved"; after a load
it was 0 until the next whole second. It is saved now with the habitat's care state
(`"viewers"`, compared by `Animals::operator==`); a save without it is told the count again on
loading (`Zoo::TellViewers`). The test's "animals on a half-walked route" precondition counts
animals on their way now (most routes are a single leg since waypoints are kept off props).

Continuation G6b, 2026-11-14, rebased onto `f77bc8f`: verification below is being repeated
against the current animal routines and models. The earlier figures are historical, not a
claim that the interrupted sanitizer suite finished.

**Lead-supplied evidence:** on 2026-11-13 the lead ran main's `build-asan/zoo-simulator`
with `--stand habitat:lion --frames 30`; it completed cleanly under ASan. This was the
lead's run, not a run performed by this audit working tree.

The audit of 2026-11-13 (task G6), at commit `0544337`: the game and the core test suite under
AddressSanitizer and UndefinedBehaviorSanitizer, save and load, the player's collision, lifetimes
and render resources, the start of a visit and the memory it takes. Everything below was run on
the development machine (AMD Ryzen 7 PRO 7840U, Radeon 780M, Mesa 25.0.7), without a window
(`SDL_VIDEODRIVER=offscreen`) and without sound (`SDL_AUDIODRIVER=dummy`).

**The machine was busy all day**: seven other agents built, tested and ran the game beside this
audit, and the load average stood at 20 to 30 on 16 cores. Counts, hashes and sanitizer verdicts
do not depend on that; every time in this document does, and is two to six times what a quiet
machine gives (`docs/PERFORMANCE.md` has the quiet figures). Where a time matters, the load is
named with it.

## How to repeat it

```bash
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv

# The sanitizer tree (RelWithDebInfo, -fsanitize=address,undefined on the game, the core and the tests)
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DZOO_SANITIZE=address,undefined \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build-asan --parallel 3

# The core suite and the walker's tests under the sanitizers; UBSan prints and goes on, so read the log
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-asan -j3 --timeout 3000
grep -c "runtime error\|AddressSanitizer\|LeakSanitizer" build-asan/Testing/Temporary/LastTest.log

# One sanitized game run (from build-asan/; a start takes three to five minutes there)
ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:halt_on_error=0 UBSAN_OPTIONS=print_stacktrace=1 \
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy timeout 900 ./zoo-simulator --frames 60 2>&1 | grep -v WARN

# A visit from end to end: board, walk, map, pause, save, load twice, F5, quit
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy timeout 300 ./zoo-simulator --stand board:0 --frames 400 \
    --lockstep --bench --saves ../build-probe/saves --script ../tests/game/visit_save_load.script

# The start by phase and the memory by kind (Release, from the repository's root)
(cd build && SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy timeout 120 ./zoo-simulator --bench --frames 5 \
    2>&1 | grep "zoo: start")
tools/bench/memory.sh 300
./build/tests/zoo_core_tests --gtest_filter='VisitStartCost.*' --gtest_also_run_disabled_tests
```

What the sanitizers see: the game's own sources, `zoo_core` and the tests. CNA, cna-engine,
easy-gl and the graphics driver are compiled without them (they are other repositories' build
targets), so a wrong write inside the engine is seen only when it lands in memory the game
allocated. ThreadSanitizer was not run: it needs a third build tree, and the game's own code
starts no threads (the audio mixer and the driver have theirs).

## What was changed

Six corrections, each the smallest change at the place of the defect. The lines are those of the
working tree after the change.

| # | File and line | What showed it | The correction and why it is right |
|---|---|---|---|
| 1 | `src/game/Walker.cpp:29-34` (`Body`) | `Walker.NoHabitatAndNoPlantedStripIsEnteredFromAnySide`: "fastest frame 25.75 m/s" for a body that runs at 4.4; the same test and `TheFurnitureOfAViewingPlaceIsNotSqueezedInto` found bodies beyond rails | `stepHeight` 0.3 -> 0. The world has no ledge to step on: the ground is a height field, and every solid stands higher than a step. With a step height cna-engine's controller, stopped by a wall, hops forward by the body's radius and keeps the hop wherever the ground beyond is a millimetre higher (defect "the hop" below). No step height, no hop. |
| 2 | `src/game/Walker.cpp:42-50, 224-239` (`Wedged`, `Walker::Update`) | `Walker.TheFurnitureOfAViewingPlaceIsNotSqueezedInto`: 12 of 495 runs ended between a bench and its lamp with no way out (33 with correction 1 alone) | A sub-step that ends with the body more than 5 mm inside a wall, a trunk or a post is not taken (the body is put back where the sub-step began). The controller gives up pushing out after four passes and keeps an overlapping place (defect "the creep" below); refusing such a step is what a solid is. A body that already stands inside something (set down there) may still walk out. |
| 3 | `src/core/zoo/animals/AnimalLife.cpp:138-146` (`Animals::AdvanceLife`, birth) | `VisitorZooHabitat.NoHabitatEmptyOrOverFullAtTheStartAfterADayAndAfterAWeek`: "Expected: (group.sameSpecies) <= (def.groupMax), actual: 23 vs 20 ... Ostrich Rise holds too many ostrich" after day 7, seed 1 | The litter is cut to the room the group has left, never below the smallest litter. Conception already asks for room, but only for `litterMin` young; birth then drew up to `litterMax` (ostrich: 1 to 20 in a group of at most 20). An over-full group fights, by the simulation's own rule. |
| 4 | `src/core/zoo/sim/ZooSave.cpp:438-445` (`ReadJsonFile`) | `ZooSaveDamageTests.TruncatedFilesAreRefused`: "C++ exception with description "basic_filebuf::underflow error reading the file: Is a directory" thrown in the test body" | The parse is wrapped in `try`; the message goes where every other load error goes. The parser was told not to throw, but it reads the stream's buffer itself, and libstdc++'s buffer throws on a failed read (a directory under the save's name, a failing disk). The exception left `LoadZoo` and `ReadSaveSummary`, and with them the game: `ListSaves` reads every entry of the saves directory. |
| 5 | `src/game/ZooGameVisit.cpp:222-228` (`LoadVisit`) | Reading the code: the pose file beside a save was checked for finite numbers inside the map and nothing else | A pose on enclosure ground or in the planted strip before a barrier is not used; the visit begins at the entrance instead. The pose is a file of its own (`<save>.pose`): another zoo's, or a damaged one, set the player down among the animals, behind barriers that then held them in. |
| 6 | `src/game/ZooGameLook.cpp` (`OpenBoard`) | `--stand board:0`, key E: "This board is still blank" on both boards at the entrance (`build-probe/shots/board-entrance-before.png`) | The later final correction associates only boards within 8 m with a lived-in habitat. The two remote entrance boards instead welcome visitors, give the 9:00–19:00 hours, 16:30 last admission, current animal/habitat counts and map/species-board guidance (`docs/evidence/entrance-welcome-board-j25-2026-11-20.jpg`). |

Also added, not corrections: `StartMark` in `src/game/ZooGame.cpp` (with `--bench`, one line for each
phase of the start and of a load: seconds and resident memory), the test target `zoo_game_tests`
in `tests/CMakeLists.txt`, and the tests and tools named below.

None of the corrections changes what the golden hashes of `tests/core/VisitStartTests.cpp` cover
(`VisitStart.*`: 3 tests pass unchanged; no birth falls into the 6,000 steps of a fill). Correction 3
does change the management simulation from the first over-large litter on: fewer young, by design.

## The sanitizers

### The core suite and the walker's tests

`ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0
ctest --test-dir build-asan -j3 --timeout 3000 --output-on-failure`, started 12:08 and **stopped
at 12:34 when all sessions were told to wrap up**, at 200 of 217:

- **186 passed, 0 failed**, 14 disabled. An AddressSanitizer error or a leak ends a test process
  with a failing status, so for these 186 there is none.
- Not finished: three were running when it was stopped
  (`NoHabitatEmptyOrOverFullAtTheStartAfterADayAndAfterAWeek` for seed 1, the seven-day one;
  `NoHabitatEmptyOrOverFullDuringTheVisit` and `EveryBoardFacesThePathAndTellsOfTheAnimalsBehindIt`
  for one seed each; the other seed of the first two had passed, 386 s and 219 s), and 14 had not
  started: the second board test, `zoogen_animation`, and the twelve `Walker.*` tests. The
  walker's tests had been run under the sanitizers on their own an hour before
  (`build-asan/tests/zoo_game_tests`, `detect_leaks=1`): 12 of 12, no report.
- **UBSan's verdict on the passing tests is not known.** UBSan prints `runtime error` and lets
  the test pass; the lines are in ctest's `LastTest.log` only, and that log was lost (the
  stopped run left a temporary file, and a `ctest -N` listing made afterwards replaced it: the
  auditor's mistake). The console log has no `runtime error`, which says nothing, since ctest
  shows the output of failing tests only. Next time run with `UBSAN_OPTIONS=halt_on_error=1`,
  so that a UBSan finding fails its test and no log has to be read.
- The slowest under the sanitizers at this load: `EveryKindOfValueReplacedByAWrongOne` 1,020 s,
  the week 386 s, `SaveLoadInTheMiddleOfTheVisit` 139 s.

Release, the same sources: `ctest --test-dir build -j3`: **100 % tests passed, 0 failed out of
203** (14 disabled: timings and printers), 727 s at load average 25. That run was made before
correction 5, which touches one game-only file no test compiles; the walker's and the save tests
were run again after every later change.

### The game

`build-probe/asan-run.sh` (the command above with a name and a log). "Reports" counts
`ERROR: AddressSanitizer`, `runtime error` (UBSan) and `ERROR: LeakSanitizer` in the whole log.

| Run | Arguments | Exit | Seconds | Reports |
|---|---|---|---|---|
| start-5 | `--frames 5` | 0 | 68 | none |
| start-60 | `--frames 60` | 0 | 183 | none |
| stand-lion (a carnivore) | `--stand habitat:lion --frames 40` | 0 | 172 | none |
| stand-zebra (hoofstock) | `--stand habitat:zebra --frames 40` | 0 | 191 | none |
| stand-flamingo (a bird) | `--stand habitat:flamingo --frames 40` | 0 | 273 | none |
| stand-board0 | `--stand board:0 --frames 40` | 0 | 275 | none |
| people-800 | `--people 800 --frames 40` | 0 | 310 | none |
| people-0 | `--people 0 --frames 40` | | | stopped while it ran |

Each of the seven builds the zoo, fills it with 6,000 steps (800 visitors, 74 animals), paints
the ground, loads the 28 avatars, catalogues 53 recordings, draws its frames and leaves through the
normal end of the game: all of the start and of an ordinary frame at four places, with nothing
reported. start-5 was run before the corrections, the other six after them.

**Not run under the sanitizers** (prepared in `build-probe/asan-batch.sh` and
`build-probe/asan-batch-2.sh`, stopped by the wrap-up; each takes three to five minutes):
`--bench --frames 60`; `--viewer --animal lion --clip walk`; `--editor` and the editor's save
and load (`tests/ui/save_load.script`); **the scripted visit with its two loads**
(`tests/game/visit_save_load.script`); `--ticks 20000`; `--menu`; `--editor --empty`; the
`--without` switches; a stand at the penguins; a save that cannot be written
(`build-probe/save-refused.script`, `--saves /dev/null/saves`); the walk into a wall
(`build-probe/wall.script`). This is the larger half of work item 1, and it is open.

### Leaks

The game itself was **not** run with `detect_leaks=1` (the last run of the stopped batch). What
was: the walker's tests (no leak) and the 186 core tests above (a leak fails a test; none did).

## Save and load

**The round trip is exact.** `VisitorZooHabitat.SaveLoadAtVisitingTime` (there before) and the
new `SaveLoadInTheMiddleOfTheVisit` (seeds 1 and 2): the visit is saved four times, 300 steps
apart, each time in the middle of things (the test first asks that there is a middle: animals
and visitors on half-walked routes, visitors in at least three activities). Each time the loaded
zoo has the saved one's state hash, map (reserved strips included) and configuration, saves to
the same document, and then walks 300 steps beside the zoo that was never saved, hash for hash.

**Damaged saves** (`tests/core/ZooSaveDamageTests.cpp`, new, five tests; the rule of the file:
loading ends in an error with a message, or in a zoo that then steps a game minute and a half,
hashes, saves and loads again; never a crash, and never a zoo that was accepted and then breaks):

| Test | What it does | Result |
|---|---|---|
| `EveryKindOfValueReplacedByAWrongOne` | every kind of leaf in a real save (220 kinds: "/zoo/animals/animals/#/route/#/#" is one), each replaced by other types and by numbers no writer produces (negative, huge, NaN-like, out of every range) | 3,252 cases: 1,890 refused with a message, 1,362 accepted and lived on |
| `WholePartsMissingOrOfTheWrongType` | every part of the document removed, null, an empty array, an empty object | 72 cases: 44 refused, 28 accepted |
| `TruncatedFilesAreRefused` | the file cut at many lengths, from empty to one byte short; a directory under the save's name; a path below a file | all refused with a message; no `.tmp` left behind; **found defect 4** |
| `FlippedBytesAreRefusedOrHarmless` | 320 single-byte flips, spread over the file and over the first 6,000 characters of each part | 161 no longer JSON, 104 refused by the checks, 55 accepted and lived on |
| `AVersionOneSaveWithoutStandOffOpens` | a version 1 save (no stand-off layer) opens as the zoo it was; version 2 still opens under format 3, while version 2 without the required layer is refused, naming `stand_off` | passes |

Under ASan and UBSan these tests are where an accepted but wrong value would show as a read past
an array: all five passed there (1,020 s, 31 s, 24 s, 107 s and 20 s), which rules out the ASan
kind of error; for UBSan see the caveat above.

**The game's side** (`src/game/Saves.cpp`, `ZooGameVisit.cpp`), by reading and by running:

- A save that cannot be written: by reading, `SaveZoo`'s error is flashed for six seconds ("The visit could
  not be saved: ...") and printed (`zoo: save failed: ...`), and the visit goes on. **Not run**:
  the prepared script (`build-probe/save-refused.script` with `--saves /dev/null/saves`) was
  stopped by the wrap-up. The core's part is tested: `TruncatedFilesAreRefused` saves below a
  file and gets a message and no `.tmp`.
- A missing or unreadable save in the list is shown as unreadable and cannot be chosen; a save
  that fails to load leaves the running visit as it was (`LoadVisit` adopts the new zoo only
  after `LoadZoo` returned one).
- `SaveZoo` writes `<name>.tmp` and renames it, so a crash in the middle of a save leaves the old
  file whole. The pose file is written directly and without a check; a damaged pose costs the
  place the player stood at, nothing else (correction 5 closes the one case that cost more).
- Not corrected, small: `SaveVisit` ignores a failed `create_directories` (the failure then
  shows as SaveZoo's error, as the refused-save run shows) and a failed pose write (silent: the
  next load starts at the entrance). `ListSaves` stops at the first file-system error of the
  directory walk and shows the saves read so far.
- **J28: save-list summaries no longer parse the whole save.** `SaveZoo` writes a small derived
  `<save>.summary` sidecar after the authoritative save has been atomically committed. Its saved
  file size and exact file-system timestamp bind it to that save; a missing, stale, malformed,
  unsupported or implausibly large sidecar is ignored, the full save is parsed, and a successful
  parse repairs the sidecar. Failure to write this cache never fails or damages the save. It is
  not itself a save and `ListSaves` does not display it. On the current 2,374 KiB Visitor Zoo
  save, three diagnostic rounds took 24.1--24.4 ms for the forced cold fallback and 34--50
  microseconds for the normal cached read (roughly 500--700x faster). The earlier 0.10--0.26 s
  figures included substantially noisier machine load. Regressions cover normal creation,
  missing-cache fallback, stale stamps,
  malformed/oversized cache data, self-repair and truncated authoritative saves.

**Loading while a visit runs** (work item 6): `tests/game/visit_save_load.script` saves from the
pause menu, loads that save, walks, loads it again, saves with F5 and quits through the menu.
Run once, Release, 10:21, before the corrections: `zoo: saved`, `zoo: loaded` twice, `zoo: saved`
(F5), the save on quitting, exit status 0 before frame 400; the pictures are
`build-probe/shots/visit-*.png`. **Under ASan this script was not run** (it was the twelfth run
of the stopped batch), so the lifetimes around `Adopt` are covered by reading only: see below.

## Occupancy and boards: the owner's regression tests

All in `tests/core/VisitorZooHabitatTests.cpp`, for seeds 1 and 2, on the real definitions.

**`NoHabitatEmptyOrOverFullAtTheStartAfterADayAndAfterAWeek`.** `ExpectOccupancy` asks of every
habitat of Greenhill: it has residents; none of them is loose; each stands on its own habitat's
ground; the species are the ones the plan promises; no species has more animals than its
`groupMax`; the shelters hold all residents; and the space score is at least 0.99, including
after births. Checked after `BuildVisitorZoo`, after one day of the management
simulation (14,400 steps) and, for seed 1, after seven days.

- At the start and after a day: no habitat empty, none over-full, both seeds.
- Before correction 3, Ostrich Rise held 23 ostriches of at most 20. Correction 3 limited the
  group but still let shelter and ground overflow. Before J26 that historical week ended with
  101 animals and 27 births: 20 ostriches at space 0.83 and 20 penguins, both with shelter for 18.
- After J26, seed 1 ends day 7 with 91 animals and three births. Penguin Cove grows from 13 to 15
  and one other habitat gains a young animal; all fourteen habitats still pass the strict shelter
  and space checks. Reproduction remains visible without manufacturing overcrowded groups.

**`NoHabitatEmptyOrOverFullDuringTheVisit`** fills the zoo and steps the visit as the game does
(pace 12), and asks the same as the visit begins and 6,000 steps later. **A visit's calendar
never reaches a second day**: `ZooGameVisit.cpp:440` stops it at 15:30 (`kLatestHour`), so "after
a day" and "after a week" are days of the simulation, which these tests walk.

**The animals do not stop with the calendar.** The visit slows the calendar twelve times and
stops it at 15:30, but the animals live by the steps (`Zoo::Step`: "by the motion clock"):
they eat, age, conceive and give birth at the management simulation's pace, twenty steps a
second. Before J26, `DISABLED_NoHabitatEmptyOrOverFullOverAVisitingDay` (172,800 steps: 2 h 24
min of play, twelve days of the animals' life; 317 s in Release, seed 1; seed 2 was stopped by
the wrap-up) recorded:

```
OCCUPANCY seed=1 the first birth of the visit after 67201 steps (56 minutes of play)
OCCUPANCY seed=1 after 172800 steps of the visit day=15 animals=112 births=38 deaths=0 escapes=0
OCCUPANCY ...: Meerkat Lookout has grown to 9 animals, shelter for 8, space 1.000000
OCCUPANCY ...: Ostrich Rise has grown to 20 animals, shelter for 18, space 0.832787
OCCUPANCY ...: Penguin Cove has grown to 20 animals, shelter for 18, space 1.000000
```

No habitat emptied and no group passed its `groupMax` (with correction 3), but three habitats
outgrew their shelter. J26 makes the same strict rule apply after births; the ordinary first 6,000
visit steps still have 74 animals and no birth, and their hashes are unchanged. Re-recorded after
J26, the long seed-1 diagnostic passes in 25.61 s: the first birth is at step 52,201 (43 minutes of
play), and step 172,800 ends with 102 animals, 14 births, no death or escape, and every habitat
still passing the strict group, shelter and space checks.

**`EveryBoardFacesThePathAndTellsOfTheAnimalsBehindIt`** (with the earlier
`EveryHabitatHasItsOwnReadableBoard`). Of every information board: it stands on public ground,
neither in a habitat nor in the planted strip nor in a building; it is never blank; paving that
the entrance leads to lies in front of its desk, within the game's reach of 2.8 m and within 60
degrees of where the desk looks; a step in front of it is still the visitors' ground; the first
habitat met walking back from the desk is the habitat the board tells of; the species it is
about is one the plan promises for that habitat and lives there; and every row of the board
window has its data (common and scientific name, range, biome, mass, height, lifespan, status,
facts). And of every habitat: at least one board of its own stands before it.

Where the HUD's text comes from: `ZooGame::OpenBoard` is a member of the game's class and cannot
be called without a window, so the test repeats its choice in ten lines (`BoardHome`: the
nearest lived-in habitat; then the most numerous species) and checks the fields of `SpeciesDef`
the rows are made of. The two must be kept alike by hand; moving the choice into the core would
remove that (not done: `ZooGameLook.cpp` is edited by other tasks today).

Result, both seeds: `BOARDS seed=1 all=16 away from habitats=2`. The 14 habitat boards pass
every point; the two boards at the entrance stand before no habitat (correction 6).

## The player's collision

`tests/game/WalkerTests.cpp` (new target `zoo_game_tests`: the game's own `Walker.cpp` on
cna-engine's collision, no window, no graphics) walks the real Greenhill, seed 1. Twelve tests;
Release 31 s, and under ASan, UBSan and the leak check: 12 of 12, no report.

| Asked | Result |
|---|---|
| Every path tile the visitors use can be walked, tile to tile | 5,013 tiles, 10,024 steps between neighbours, none closed to the player; feet never more than 0.20 mm off the ground; the steepest path tile rises 5.7 cm |
| No habitat and no planted strip can be entered, from any side, walking or running, straight or at 45 degrees along the barrier | 1,496 barrier edges (wire mesh 73, mesh-backed hedge 20, tall mesh 51, glass 201, cable 2, high glass 171, heavy mesh 228, stand-off rail 746, paddock rail 2, tall cable 2), 14 of them gates: no trespass; the fastest frame was 8.80 m/s |
| Keepers' gates stay shut | 14 gates, 8 with public ground before them: none passed |
| Buildings are solid from all sides | 41 buildings, 28 reachable on public ground, 424 runs at them from 16 directions: no body inside (beyond the 5.2 cm a round body cuts a square corner by design) |
| The furniture of a viewing place cannot be squeezed into | 42 groups of bench, lamp, board or bin; 495 runs: caught 0 |
| Props stop the body and let it pass | 16 kinds; no trunk, bench, bin, lamp or board walked into. 84 runs aimed 5 cm beside the centre did not slide past within 3.5 s: the body stands, as it does before a tree, and is not held (it leaves in any other direction) |
| The land ends before the map does; out of the entrance the body stays on the land | the entrance gate holds the player in: from z = 303 the body comes to 307.61 of 320 |
| A long aimless walk (60 legs, 2,735 m, 36,000 frames) | never off public ground, never trapped |
| Standing, strolling, the same steps giving the same place, slopes and water | pass |

### Two defects of cna-engine's character controller

Found at cna-engine `04ab7b3`, `src/Collision/CharacterController.cpp`; the engine is not changed
from here. Both belong in `docs/UPSTREAM.md` (the lead's file).

**The hop.** Lines 220-245: when a move ends at a wall and a step height is set, the controller
lifts the body by the step height, moves it at least `radius + 0.01` m along its heading, and
keeps that place if it went further than the plain move and `feet_.Y > start.Y + 1e-3f`: if the
ground there is a millimetre higher than where it started. That is meant for a ledge. On open
ground that merely rises, any wall met at an angle qualifies: the body advanced 0.29 m in one
sub-step of 1/90 s. Measured in Greenhill before the correction: **25.75 m/s** along a fence for
a body whose running speed is 4.4, and bodies carried through gaps narrower than themselves.
Suggested correction there: accept the hop only when the ground under the new place is a ledge
(higher than the ground under the old place by more than the slope alone gives over that
distance, or: only when `LedgeBehind` holds for the contact that stopped the move).

**The creep.** `Resolve`, lines 85-135: the body is pushed out of its deepest contact,
`iterations` times (4), and the place it has then is kept, also when it still overlaps
something. Between two solids a little closer than the body is wide (a bench and the lamp before
it: 0.51 m for a body of 0.56 m) each pass pushes it out of the one into the other; a few
millimetres are left over every step, and the body creeps through and into a pocket it cannot
leave. Suggested correction there: when the last pass still leaves an overlap deeper than the
skin, `Resolve` reports it and `Step` does not take the move.

What each remedy on the game's side does, measured with the same tests:

| `Walker.cpp` | fastest frame | caught between furniture (495 runs) |
|---|---|---|
| as it was (step height 0.3) | 25.75 m/s | 12 |
| the guard only | 25.75 m/s | 2 |
| step height 0 only | 4.40 m/s | 33 |
| both (as changed) | 4.40 m/s | 0 |

The guard's price: the contacts of the body are asked once more after every sub-step. The
aimless-walk test (36,000 frames) took 477 ms before and 609 ms after: about 13 -> 17
microseconds a frame, 0.04 % of a 9 ms frame.

The scripted habitat viewpoints now use the same solid radii as `Walker` before choosing a
place, including the otherwise simulation-passable benches, bins, lamps and boards, plus the
body's 0.28 m radius. A zero-tick `--stand habitat:<species>` smoke of all fourteen habitats
reported exactly the same set-down and final coordinates to 0.1 m; the former 42 furniture
overlaps therefore no longer rely on the controller pushing the body out (2026-11-19).

They now also classify the containment barrier beyond a planted stand-off. Keeper gates are
rejected; glass and moat fronts are strongly preferred over service mesh and hedges; and a panel
with matching neighbours is preferred over an end panel. Visibility, intervening scenery and
the animal's useful distance still participate in the same score, rather than an absolute
material order hiding the subject behind a tree. Real offscreen Lion Rock and Bear Falls runs
kept the selected animal visible through the public front; the inspected pair is
`docs/evidence/public-front-stand-j32-2026-11-20.jpg` (2026-11-20).

The camera near plane is 0.1 m now (was 0.3 m). At the valid closest centre of the body to the
gift shop's west wall (`--view 181.97,301,75,0`), the old plane cut away the left quarter of
the wall and exposed the space beyond it; the same offscreen contact picture after the change
is wall across the full frame. A normal `--stand habitat:lion` picture showed no depth fighting
or other visible projection regression (2026-11-19).

### What is still open in the walk

- `Walker::Update` rebuilds its collision world when objects or fences change (their revision
  counters), not when the ground or the water is edited. The visit changes neither, so nothing
  is stale today; a mode that sculpts the ground while the player walks (the editor has its own
  camera) would have to tell the walker.

## Lifetimes and render resources

Read: `ZooGame`'s members and their order of destruction, `Adopt`, `LoadVisit`, every place
that creates a texture, a render target, an effect or a buffer in `src/game`. Run: all of the
above under ASan, where a use after free of the game's own objects stops the process.

- **Replacing the zoo.** `ZooGame::Adopt` resets the tools, the sounds, the animals' renderer,
  the scenery and the terrain before it replaces `zoo_`, and builds them again for the new one;
  nothing that holds a reference into the old zoo outlives it. ("What draws the old zoo goes before
  the zoo it refers to", in the code's own words.) Read, and run in Release (the scripted
  visit, two loads, exit 0); not run under ASan, see "Save and load".
- **Created once.** The visitors' shading texture and effects sit behind null checks; the
  avatar cast's sheets are one `RenderTarget2D` for each of the 28 cast members, made once, one
  a frame; the surfaces' materials are keyed by colour, made once and kept across `Adopt`; the
  sky's cubes are made once. No creation was found in a per-frame path.
- **Per frame.** No texture, target or effect is created in `Draw` or `Update`. What is
  allocated per frame is vectors of instances and of people in view (reserved, reused).
- **Memory across loads.** **Not measured.** `StartMark` prints the resident memory after each phase
  of `Adopt`, so one `--bench` run of the scripted visit shows whether a load leaves memory
  behind (compare the "visitors and tools" lines of the two loads); the run was stopped by the
  wrap-up.
- **Exit.** The game ends by `--frames`, by the Quit button of the pause menu (the visit script)
  and by the menu; all three leave with status 0 under ASan, with the leak check as described
  above.
- Not covered: closing the window (there is none offscreen) and losing the graphics context.

## The start of a visit

**Not measured in this audit.** What was done: `StartMark` (`src/game/ZooGame.cpp`), which with
`--bench` prints one line after every phase of `LoadGame` and of `Adopt` and at the last frame:

```
zoo: start: <phase>   <seconds since the start> s (+<this phase> s) <resident> MB resident
```

for "ground", "scenery", "animals", "visitors and tools" and the phases of `LoadGame`. The three
Release runs (`--bench --frames 5`, command at the top) were to follow the sanitized batch and
were stopped by the wrap-up; with a load average of 25 they would also have said little about
the 5.5 s of a quiet machine (`docs/PERFORMANCE.md`). The sanitized runs print the game's own
older lines, which at least order the costs under ASan: building the zoo 30 to 40 s, the fill
100 to 170 s, painting the ground 24 to 31 s of a start of three to five minutes.

### Filled-zoo cache

`VisitStartCost.DISABLED_BuildingAndFillingAgainstLoadingASave` (new, core only, Release), three
rounds at load average 25:

| Round | build the zoo | fill it (6,000 steps) | together | save it | load it | summary only | file |
|---|---|---|---|---|---|---|---|
| 0 | 3.64 s | 6.97 s | 10.63 s | 0.111 s | 0.115 s | 0.096 s | 2,306 KiB |
| 1 | 2.48 s | 8.36 s | 10.84 s | 0.125 s | 0.125 s | 0.102 s | |
| 2 | 3.14 s | 11.54 s | 14.69 s | 0.235 s | 0.237 s | 0.262 s | |

On a quiet machine the same two phases are 0.6 s and 1.2 s (`docs/VISIT_START.md`), and the
ratio is what carries over: **loading the filled zoo costs between a sixtieth and a ninetieth of
building and filling it**, and the loaded zoo is the same state bit for bit (the round-trip
tests). J27 implements that proposal. The private key includes the seed, crowd, hour, ticks,
available models, save/game versions and the contents of the construction and species definitions;
a code-only builder change has an explicit revision beside the key. Bad entries rebuild, variants
are kept out of the save menu, the newest eight are retained, and automation touches them only with
an explicit `--saves` directory.

In a matched software-GL pair the fresh path stored state `2609457543749151601`; the next run
loaded the same hash. The `the zoo, built and filled` phase fell 2.58 -> 0.04 s (64.5x), and startup
through `visitors and tools` fell 8.15 -> 5.65 s. The remaining ground, scenery and animal imports
are real renderer work and remain the larger part. A different crowd got a different file and
state; a deliberately truncated entry was rejected, rebuilt and restored to its earlier hash.
All 273 active C++/game CTest cases pass in 108.78 s; the unchanged animation pipeline also passed
earlier in this session (274 active overall, 21 diagnostics disabled).

## Memory

Measured 2026-11-19 with `tools/bench/memory.sh 2 --bench --lockstep`. This sandbox exposes no
`/dev/dri`, so Mesa used software OpenGL; the absolute figures are higher than the owner's Radeon
figures, but every comparison below is the same binary and setup. The script's corrected child-PID
sampling works. Before the texture correction, its largest sample was 3,873 MB resident: 3,037 MB
heap, 708 MB anonymous and 127 MB file mappings. The editor's starter zoo used 1,436 MB, isolating
the Visitor Zoo's animal models as the largest addition.

Animal mip generation used to retain both the imported one-level texture and a new full mip chain.
`AnimalTextures` now replaces an unambiguously used model-owned texture in place and explicitly
disposes the `ContentManager` representative that otherwise kept the old renderer alive. It still
makes distinct chains when one image is used with different colour spaces or alpha-test rules.
On desktop Linux, resources made during loading also omit CNA's CPU context-recovery copy; recovery
is restored before runtime textures and automated screenshots are made. Animal imports temporarily
retain their source pixels just long enough to make the role-aware mipmaps.

The first correction reports 3,381 MB: 2,548 MB heap, 705 MB anonymous and 127 MB file mappings,
a reduction of **492 MB (12.7%)**. Animal atlases whose long edge exceeds 1,024 texels now begin
at the 1K mip, made with the same sRGB/linear and alpha-coverage rules as the rest of the chain.
The identical script after that cap reports 2,870 MB: 2,373 MB heap, 368 MB anonymous and 127 MB
file mappings. That is another 511 MB, and **1,003 MB (25.9%)** below the original run. The
`StartMark` immediately after animals fell from 3,475 through 3,096 to 2,467 MB.

The lion viewer's ownership screenshot is pixel-for-pixel unchanged. Side-by-side elephant,
giraffe, brown-bear and zebra viewer screenshots show no visible loss from the 1K cap at 720p;
the giraffe is pixel-identical. Five focused tests cover ownership, repeated use, colour/data
filtering, alpha coverage and the oversized-atlas cap. This is a real cut, but it does not meet
the under-1-GB target.

The visit's shadow map is now 2,048 rather than 4,096 texels. Across its 180 m span that still
resolves less than 9 cm per texel at the game's 720p output. In matched `StartMark` phases, adding
the shadows raised resident memory from 266 to 413 MB at 4K and from 266 to 317 MB at 2K: a
directly attributable **96 MB reduction**. The whole-run sampler reported 2,687 MB in the new run;
heap variation makes the phase delta the conservative number. Entrance, lion, zebra and flamingo
screenshots were reviewed side by side and show no visible shadow-quality loss.

The dual plant forms were then isolated with `ZOO_PLANTS_FULL_DETAIL=1`. With the unused far
batches omitted, the scenery phase adds 295 MB (874 -> 1,169 MB), against 369 MB (875 -> 1,244 MB)
normally: about 74-75 MB belongs to the far forms. They remain in a normal visit because their
measured rendering benefit is about 1 ms per frame. The diagnostic full-detail mode now avoids
building resources it never submits.

Visitor terrain now discards its CPU edit cache after upload: 100 one-megabyte chunk images plus
tile signatures that only the editor can use. Water animation remains live; editor memory is
unchanged. With glibc's free arenas trimmed, the ground `StartMark` fell from 875 to 826 MB, a
conservative **49 MB resident**. The whole-run sampler read 2,682 MB (only 5 MB below the prior
run, within heap variation), and a zebra screenshot remains visually correct.

The existing 400-frame save/load script reached both reloads, saved again and made its final
screenshot without a renderer failure, although the 180 s software-rendering limit stopped it at
frame 277 before its Quit click. It also exposed an unresolved reload spike: old static GPU
resources remain resident while replacements are built, reaching about 3.1 GB; the first terrain
rebuild took 107 s. Static context-recovery copies are now disabled around every `Adopt`, not only
the initial load, but the spike remains and is not claimed as fixed.

Audio files are now decoded on first use instead of all 53 during `Soundscape` construction. The
library contains 22.71 MB of decoded PCM (12.79 MB ambience, 9.42 MB calls and 0.50 MB footsteps),
while the decoder/backend made the eager sound phase cost 46 MB resident. Its loading mark now
stays at 220 MB before and after sounds. In a 60-frame visit, three recordings were decoded and
four voices were audible (the crowd recording feeds two positioned voices), with no failures.
Animal takes and footstep surfaces remain unloaded until selected. The whole-run sampler read
2,711 MB in this run, higher than the prior 2,682 MB through heap variance, so no total-peak
reduction is claimed from that noisy number.

Animal sex/age variants are now lazy as well. The renderer formerly imported `_female` and
`_juvenile` beside the base model on first sight of a species. The Visitor Zoo consequently kept
`brown_bear_juvenile` and `polar_bear_juvenile` although its two bear habitats contain only adult
pairs. It now resolves each alternate on first demand (including a later birth), reducing the
ordinary visit from 33 imported animal models to 31. An immediate matched software-GL sample fell
2,796 -> 2,736 MB resident and 2,461 -> 2,394 MB heap; model import plus mip generation fell by
0.279 s. The 60 MB resident difference is the conservative result quoted here; absolute figures
still do not describe the owner's Radeon.

## The bench

**Not run in this audit** (`./zoo-simulator --bench --frames 300`, three times, was the last item
and was stopped by the wrap-up), so "the Release bench is not slower" rests on this: of the six
corrections only the walker's guard runs every frame, and it costs 4 microseconds a frame
(measured in the test above); corrections 3 to 6 run at a birth, at a load and at a key press,
and `StartMark` returns at once without `--bench`. The lead's merge bench is the first real
measurement.

## For the lead to decide

1. **The two controller defects** for `docs/UPSTREAM.md` (text above), and whether the game
   keeps both remedies when cna-engine is corrected: the guard costs 4 microseconds a frame and
   is harmless on a correct engine; `stepHeight = 0` should stay as long as the world has no
   ledges.
2. **Animals that breed during a visit:** J26 resolves the welfare failure by reserving nursery
   capacity while retaining young animals as something to see. The separate calendar question
   remains: animal life still advances after the visitor clock stops at 15:30.
3. **What this audit did not get to**, in the order it would be done: the rest of the sanitized
   game runs, above all the scripted visit with its loads; the core suite once more with
   `halt_on_error=1` for UBSan; the start by phase, the memory by kind and the bench in Release;
   the pictures of the wall and of the refused save.
