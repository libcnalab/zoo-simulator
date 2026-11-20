# Starting a visit

`FillForVisit` extracts the `PrepareVisit(fill = true)` sequence from `ZooGameVisit.cpp`.
It sets visitor pace, day/hour, stops the calendar, uses accelerated arrivals for the first
three fifths of the requested steps, spreads visitors, restores the visit pace, and finishes
the steps. The caller still owns the final `SetDayPace(12)`. Defaults are day 14, 10:30,
6,000 steps, linger 9, stay 12, arrival pace 0.25, fill arrival pace 1.7 (linger and stay
were 6 until the habitat fronts of 2026-11-13: see "Who watches where" in
`docs/VISITOR_ZOO.md`; the timings below were taken at the old pace).

The report gives actual steps, visitors, and visitors moved by spreading. Progress callbacks
run after each 250 steps and once at completion, including a zero-step fill. Negative step
counts mean zero. Calendar and pace validation remains with the existing setters.
The callback is observational: callers must not mutate the zoo if they need identical results.

## Filled-visit cache (J27, 2026-11-20)

The desktop game now saves the completed deterministic build-and-fill state under the save
directory's private `.cache/` subdirectory. The next visit with the same inputs loads that state
through the ordinary validated save reader, restores the unsaved visitor-pace setting, and begins
with the identical `Zoo::StateHash()`. These files are derived data, never shown in the save menu;
only the eight newest variants are retained.

The cache key covers the seed, crowd level, start hour, warm-up ticks, save format, game version,
available animal models and the bytes of every construction and species JSON file. Code-only
changes to `BuildVisitorZoo` or the fill sequence must bump `kVisitCacheRevision` beside the key
builder in `ZooGameVisit.cpp`. An unreadable or damaged entry is reported, ignored and atomically
replaced after a normal build. Automated and scripted runs do not read or write personal cache
files unless they explicitly supply `--saves`; the empty-map diagnostic is never cached.

In matched one-frame software-GL runs, the fresh path built and filled in 2.58 s and stored a
2,431,686-byte entry. The second run loaded it in 0.04 s with the same state hash
`2609457543749151601`, reducing startup through `visitors and tools` from 8.15 to 5.65 s. A
different crowd setting produced a separate 1,720,917-byte state and hash. Truncating that entry
to invalid JSON made the next run log the damage, rebuild it and reproduce its prior hash.

The separate J28 save-summary sidecar also applies to these files through the core save API,
although visit-cache files are never enumerated in the load menu. `VisitStartCost` now reports
both a deliberately uncached summary (24.1--24.4 ms for this 2,374 KiB file) and the repaired
sidecar path (34--50 microseconds); it removes both diagnostic files after the run.

## Method and baseline

Measured on this machine on 2026-11-13 with the existing compiler, Release, no sanitizers,
game disabled, one benchmark process, no concurrent builds or other tests from this task.
No downloads, extra dependencies, threads, or simulation clocks were introduced.
`which valgrind` found no executable. Temporary `steady_clock` scopes were used for profiling,
then removed. Timings include the spread operation and all 6,000 steps; build timings include
Zoo construction and `BuildVisitorZoo`. Three runs use seed 1, ending with 800 visitors and
74 animals. Timing is diagnostic, not a flaky wall-clock assertion.

The uninstrumented baseline fill times were **7.96365, 7.13229, 7.81795 seconds**:
median **7.81795 seconds**. Build times were 0.747441, 0.627570, 0.760242 seconds
(median 0.747441). An earlier exploratory run concurrent with ASan tests gave a 9.94067-second
median; it is not used for the before/after comparison.

All golden hashes were captured before changing the simulation, from commit
`e1472bd0f07b252142d4efd1698525c9cbd1dd0d` plus the equivalent extraction. The test records
both 600-step and 6,000-step hashes for seeds 1, 2, 3, and a further 500 steps at the stopped
calendar. These are full `Zoo::StateHash()` comparisons, not just population checks.
The equivalence test independently spells out the original game's sequence.

## Optimization

Read-only `FindProp` calls in animal movement, decisions, welfare and target validation, and
visitor bin selection now use `std::as_const(env.objects)`. `Env` stores a mutable object
reference: even a `const Env&` does not make the referenced object const. Assigning the result
to `const Prop*` therefore previously selected the mutable overload. That overload invalidates
`ZooObjects`' spatial buckets, so the next `PropsNear` rebuilds the index of all props.

The measured hotspot was animal movement's nearby-prop query, not visitor pathfinding.
One instrumented full fill spent 7.48 seconds in that query. The change preserves lookup
results, ordered query output, floating-point operations and RNG calls. Actual feeder,
trough and bin mutations still use mutable access and invalidate the cache as before.
No changes were made to object indexing, navigation algorithms, persistence or public APIs.

## Regression coverage and findings

The tests use real definitions and the named Greenhill habitat/species plan for seeds 1 and 2.
`ScenarioReport` has aggregate totals and problems only; it does not report a per-habitat
occupancy roster. Tests therefore independently assert all 14 named enclosures and their
15 resident species, including giraffes and zebras sharing Acacia Plain. With penguins omitted,
only Penguin Cove is empty, still named and present. An explicit empty-habitat report would
require a future `ScenarioReport` API change outside this task's ownership.

Board tests reproduce the game's 2.8-metre reach, nearest/gaze selection (looking directly at
the candidate), nearest occupied enclosure within 30 metres of its tile rectangle, and
most-numerous-species selection with ordered tie-breaking. Every habitat must have its own
readable board, outside all enclosures, reached from a passable path-tile centre. Decorative
boards that describe no occupied enclosure do not satisfy any habitat's requirement.
All resident species must have facts, native range, diet and conservation text.

Routine tests use a 600-step fill; full 6,000-step visibility/containment checks are disabled
for routine ASan runs and explicitly run in Release. Both seeds had a longest fully hidden
habitat streak of **zero** during the 2,000 observed steps, after both fill lengths.
No species-specific weakening of the 600-step visibility limit was needed.
Containment checks also verify original species-to-habitat membership, living animals,
passable land/water (using body-scaled wading depth), and no solid-prop overlap at the end.
Sheltered animals are exempt from ground/building passability, as they are intentionally inside.
Prop clearance uses the simulation's `PropDef::radiusM` rule.

An exploratory stricter spawn/early-fill check found animals initially centred on props
(e.g. seed 2 flamingo Esme at `fish_bucket_stand` after 600 steps). These overlaps were absent
at the required end-of-observation checks, including both full fills. The occupancy check
immediately after building requires valid terrain and enclosure membership, not settled steering.

### Saving and loading at visiting time

`SaveLoadAtVisitingTime` writes under `build-probe/tests/`, loads, compares subsystem state and
the total hash, steps both zoos 500 times, and compares again. `VisitorPace` is a setting, not
state (`Visitors::Pace()` says so): it is not saved and takes no part in `StateHash()`; the game
sets it again after loading (`ZooGame::PrepareVisit`). The investigation first read that as a
defect, because a loaded zoo that is stepped *without* setting the pace again diverges from the
original after a few hundred steps (visitors and objects; animals, calendar, weather, finance
and enclosures stay equal). The test now does what the game does, sets the pace on the loaded
zoo, and requires equal state immediately and after 500 further steps, for both seeds.

## Results

The single const-lookup change reduced uninstrumented fill times to **1.47358, 1.44719,
1.43484 seconds**, median **1.44719 seconds**: **81.5% less time (5.40x faster)**.
The corresponding build times were 0.761128, 0.839767, 0.755816 seconds (median 0.761128).
The fill meets the under-three-second target without changing any of the twelve golden hashes.

The following split is from the median-total instrumented run on each side. Profiling runs
are separate from the uninstrumented medians above; clock overhead and ordinary machine
variation mean their totals differ. Visitor rows are nested under the visitor total.

| Full fill component | Before, seconds | After, seconds |
| --- | ---: | ---: |
| Animals, inclusive | 7.834230 | 1.034820 |
| Visitors, inclusive | 0.491296 | 0.427011 |
| Visitor route finding, inclusive | 0.200208 | 0.178394 |
| Visitor movement/steering | 0.138335 | 0.125675 |
| Visitor decisions, excluding nested routing | 0.005661 | 0.004452 |
| Other visitor work, including route lane adjustment | 0.147092 | 0.118490 |
| Weather stepping | 0 | 0 |
| Rest, including spread and orchestration | 0.004820 | 0.003793 |
| Total | 8.330350 | 1.465630 |

Animal movement's main nearby-prop query fell from 7.481700 to 0.770516 seconds in these runs.
Remaining mutable feeder/trough updates still invalidate buckets; that is the main remaining
cost. Making invalidation sensitive to spatial edits would require a separate objects API
change, outside the owned files. Navigation and visitor decisions are no longer worthwhile
first targets here. Weather does not step while `dayPace == 0`; calendar setup is in the rest.

`DISABLED_FillTiming` prints three fresh total/build timings and explicitly labelled reference
split figures from this investigation. It does not claim to remeasure the internal split:
reproducing that requires temporary scopes around `Zoo::Step`'s animal/visitor/weather calls,
visitor `Route`, `Move`, `Decide`, and animal `AdvanceMovement`/nearby-prop calls. Inclusive
scopes account for nested work; decision-only figures exclude nested routes. All such timers,
profiling globals and includes have been removed from the core. No permanent profiling API
or clock was added.

## Reproduction commands

Every build/test shell used:

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv ASAN_OPTIONS=detect_leaks=0
```

`build-asan/` already existed with Debug, AddressSanitizer and the required ccache configuration;
it was reused without configuring again. Release was configured once:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZOO_BUILD_GAME=OFF \
  -DZOO_LIBCNA_ROOT=/rv/data/development/github.com/libcna \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -j2
cmake --build build-asan --parallel 2
ctest --test-dir build-asan --output-on-failure -j2
```

The sandbox cannot write the required shared cache. Builds used elevated execution for that
cache; the first sandboxed build failed with a read-only-filesystem error. No alternate cache
or build directory was made. The two builds never ran simultaneously.

Explicit Release diagnostics:

```sh
build/tests/zoo_core_tests --gtest_filter='VisitStart.DISABLED_FillTiming' \
  --gtest_also_run_disabled_tests
build/tests/zoo_core_tests \
  --gtest_filter='VisitStart.*:Seeds/VisitorZooHabitat.*-Seeds/VisitorZooHabitat.DISABLED_SaveLoadAtVisitingTime/*' \
  --gtest_also_run_disabled_tests
build/tests/zoo_core_tests \
  --gtest_filter='Seeds/VisitorZooHabitat.DISABLED_SaveLoadAtVisitingTime/*' \
  --gtest_also_run_disabled_tests
```

During development the ASan full-suite run and the first habitat run were stopped after the
more expensive baseline had established the findings; they are not counted as completed
suite runs. The initial overly broad checks also required every decorative board to describe
a habitat and checked solid overlap at spawn. The final tests enforce the brief's actual
requirements: at least one usable board per habitat, and no solid overlap after settling.

### Completed validation

- Final uninstrumented Release build: succeeded.
- Release CTest: **167 executed, 0 failed, 10 disabled; 91.80 seconds wall time**. The
  existing `zoogen_animation` test took 71.73 seconds; the C++ portion finished first.
- Explicit extraction/habitat diagnostics, including full hashes, three timing runs and both
  full visibility/containment tests: **13 passed, 0 failed; 29.893 seconds**.
- Explicit disabled save/load regression: **2 executed, 2 expected failures; 2.695 seconds**.
  Its diagnostics also show all three visitor pace fields reverting to 1 on load.
- `clang-format-19 -i` was run on all eight final C++ files. `git diff --check` passed.

The formatting command was:

```sh
clang-format-19 -i src/core/zoo/sim/VisitStart.hpp src/core/zoo/sim/VisitStart.cpp \
  tests/core/VisitStartTests.cpp tests/core/VisitorZooHabitatTests.cpp \
  src/core/zoo/animals/AnimalBehaviour.cpp src/core/zoo/animals/AnimalMovement.cpp \
  src/core/zoo/animals/Animals.cpp src/core/zoo/visitors/VisitorPlans.cpp
git diff --check
```

Development-only runs (before the final checks above):

- `ctest --test-dir build-asan --output-on-failure -j2` on the Part 1 version was
  interrupted before completion; no complete-suite result is claimed for it.
- `build-asan/tests/zoo_core_tests --gtest_filter='VisitStart.*:Seeds/VisitorZooHabitat.*'`
  on the initial test draft was interrupted. The 601-step equivalence test passed in 155.136
  seconds before optimization; early overbroad solid-overlap expectations failed.
- `build/tests/zoo_core_tests --gtest_filter='VisitStart.*:Seeds/VisitorZooHabitat.*'
  --gtest_also_run_disabled_tests` on that draft executed 15 tests: 8 passed, 7 failed,
  116.638 seconds. Failures comprised the two early solid-overlap checks, two overly strict
  decorative-board checks, one early-fill overlap, and two save/load checks. The first five
  were corrected to assert the brief's actual phases/quantifiers; the persistence defect
  remains explicitly disabled. Both full-fill containment tests passed even in this draft.
- `build/tests/zoo_core_tests
  --gtest_filter='VisitStart.DISABLED_FillTiming:Seeds/VisitorZooHabitat.Residents*:Seeds/VisitorZooHabitat.Every*:Seeds/VisitorZooHabitat.Visible*'
  --gtest_also_run_disabled_tests` with corrected habitat expectations and temporary profiling
  executed 7 tests: all passed, 40.427 seconds.
- The timing-only command above was also run with temporary timers to refine the hotspot and
  collect before/after splits. Each completed invocation executed one passing test with three
  full fills. These instrumented totals are not used as the uninstrumented speedup baseline.
- One development compile rejected an attempted `VisitorPace` whole-struct comparison (the
  type has no equality operator). The final diagnostic compares its three fields individually.

Final ASan validation: the Debug/AddressSanitizer build succeeded; CTest executed **167 tests,
0 failures, 10 disabled, 720.64 seconds wall time**. This includes the existing Python animation
suite (28 internal tests), which also passed in Release. Both complete CTest runs include 166
active C++ cases plus that Python-suite entry. Existing tests were not edited.

The requested ASan runtime budgets were **not met**. Final habitat-case times were:

| Check | Seed 1, seconds | Seed 2, seconds |
| --- | ---: | ---: |
| Initial/post-fill residents, including the omitted-penguin scenario | 66.93 | 61.11 |
| Readable boards and species data | 27.19 | 26.85 |
| 600-step fill plus 2,000-step visibility/containment observation | 31.33 | 32.31 |

These six cases sum to 245.72 test-seconds (at least 122.86 seconds even with perfect two-job
scheduling), exceeding the 90-second file target. The unchanged four-zoo construction test
also took 138.24 seconds, and the board-only cases need no warm-up at all. Real scenario
construction in this Debug/ASan configuration is already beyond the approximate 20-second
per-test budget. Optimizing `BuildVisitorZoo` and its construction helpers would require
expanding the read-only scenario/build scope. Coverage and the 2,000-step observation window
were retained instead of hiding this limitation. The 600-step three-seed hash guard took
85.31 seconds, and the two-zoo sequence comparison took 50.73 seconds.

No existing public API was changed. The only added API is the requested visit-start options,
report and function. Negative fill counts are explicitly treated as zero. Suggested follow-ups
are a per-habitat occupancy/omission report and persistence/hash coverage for `VisitorPace`.
All work is left uncommitted; nothing was published remotely.

## The zoo's plan and the prop buckets (2026-11-14)

The reworked plan (`docs/VISITOR_ZOO.md` "The zoo's plan") stocks 88 animals instead of 74.
Measured with `VisitStart.DISABLED_FillTiming` (seed 1, Release with debug information, three
runs per invocation, invocations of the binary before and after interleaved), the machine
shared with other agents at a load average of **10 to 28**, so single figures are noise and the
fastest run of each side is the fairest comparison:

| | fastest fill, s | fastest build, s | animals |
|---|---:|---:|---:|
| before the plan (`b2ebdd2`) | 2.52 | 0.81 | 74 |
| the plan alone | 3.44 | 0.86 | 88 |
| the plan and the bucket fix below | 2.05 | 0.80 | 88 |

The plan alone made the fill dearer. Stack samples taken with gdb during the fill (40 a run)
showed the same picture before and after it: `ZooObjects::RebuildBuckets` in 30 of 40 samples,
called from `PropsNear` in `AdvanceMovement` and `RoutinePlace`. The mutable `FindProp`
threw the spatial buckets away on every call in case the caller moved the prop, and animals
eating or drinking call it every tick for the amount they take, so the next query re-sorted all
2,500 props of the zoo into a `std::map`. Nothing moves a prop through that pointer (props move
only by removal and addition, which do invalidate), so it keeps the buckets now; loading resets
them. The golden hashes did not change. Afterwards `RebuildBuckets` no longer appears among
26 samples; the visitors' routing and watching are the largest part of the fill.
