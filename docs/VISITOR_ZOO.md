# Greenhill Zoo — visitor plan

A 160 × 160 tile (320 × 320 m) zoo, with free admission and automatic animal care.
North leads into the zoo from the south gate. Entrance coordinates `(east, north)` mean
world tile `(entrance.x + east, entrance.z - north)`; one tile is 2 m. The entrance tile is
`(79,153)`, centred at world `(159,307)` metres. Construct with `VisitorZooConfig(seed)`,
then call `BuildVisitorZoo(zoo, options)`. `withoutSpecies` suppresses animals only.

```text
                              NORTH
             Polar Sound     Bear Falls — Wolf Wood — Tiger Forest
                  |              \    northern court    /       |
             Penguin Cove ======= woodland shortcut ======= Lion Rock
                  |             Red Dune                        |
             Elephant Valley === central food court ============ |
                  |           shaded garden shortcut       Acacia Plain
                  |                                    giraffes + zebras
             Rhino Hollow    Kangaroo Grove — Ostrich Rise      |
                    \           Meerkat Lookout — Flamingo Lagoon
                     \        ENTRANCE PLAZA                 /
                      education, shops, fountain, shaded seats
                                  SOUTH GATE
```

The asphalt round bends between habitats; gravel circuits, curved viewing bays and shortcuts
make shorter visits possible. Boardwalks follow the lagoon and Penguin Cove. Habitat outlines
combine elliptical lobes. Every habitat has a keeper gate, shelter, stocked feeders, water,
enrichment, a public information board and benches. Planting concentrates behind the open
viewing ground, with woods between habitats and around the perimeter.

The seed-1 zoo contains **74 animals in 14 habitats, 41 buildings and 2,817 props** when
every species is included. The farthest public path tile is **77.1 m** walking distance
from a visitor facility. `VisitorZooTests.DISABLED_PrintPlan` prints these measurements. Groups are adult males + females. **S** = south; **W/E** = west/east.
The principal fronts face south and the zoo's centre. The habitat footprints are unchanged:
the public walks and rails move outward, preserving all space and habitat provisions.

| Habitat | Species and group | Area before → after, m² | Front | Back | Stand-off depth |
|---|---|---:|---|---|---:|
| Meerkat Lookout | Meerkat 1 + 5 | 252 → 252 | Glass S/W | Wire mesh | 0 |
| Flamingo Lagoon | Flamingo 3 + 9 | 796 → 796 | Glass S/W | Wire mesh | 0 |
| Acacia Plain | Giraffe 1 + 3; zebra 1 + 5 | 4,284 → 4,284 | Public rail, planting, tall mesh S/W | Tall mesh | 1 tile / 2 m |
| Ostrich Rise | Ostrich 1 + 3 | 1,016 → 1,016 | Public rail, planting, tall mesh S/W | Tall mesh | 1 tile / 2 m |
| Lion Rock | Lion 1 + 2 | 2,236 → 2,236 | Glass S/W | Heavy welded mesh | 0 |
| Tiger Forest | Tiger 1 + 1 | 1,788 → 1,788 | High glass S/W | Heavy welded mesh | 0 |
| Wolf Wood | Wolf 1 + 4 | 1,788 → 1,788 | Glass S/W | Tall mesh | 0 |
| Bear Falls | Brown bear 1 + 1 | 2,236 → 2,236 | High glass S/E | Heavy welded mesh | 0 |
| Polar Sound | Polar bear 1 + 1 | 2,812 → 2,812 | High glass S/E | Heavy welded mesh; cooled den | 0 |
| Penguin Cove | Penguin 4 + 8 | 1,044 → 1,044 | Glass S/E | Wire mesh | 0 |
| Elephant Valley | Elephant 1 + 3 | 5,436 → 5,436 | Public rail, planting, cable S/E | Cable | 2 tiles / 4 m |
| Rhino Hollow | Rhino 1 + 2 | 1,692 → 1,692 | Public rail, planting, cable S/E | Cable | 2 tiles / 4 m |
| Red Dune | Camel 1 + 3 | 1,616 → 1,616 | Public rail, planting, tall mesh S/E | Tall mesh | 1 tile / 2 m |
| Kangaroo Grove | Kangaroo 1 + 4 | 1,136 → 1,136 | Public rail, planting, tall mesh S/E | Tall mesh | 1 tile / 2 m |

The six habitats with strips also have them beside their back circuits: the public cannot
approach the same animals from behind at a smaller distance. Square dilation of the curved,
rasterised footprints keeps at least the stated clearance even at corners; some sightlines
cross more tiles. Straight portions of both principal fronts have exactly the tabulated depth.
There are 1,015 explicitly marked strip tiles in both tested seeds, with one low, seeded shrub
or rock per tile: biome-specific bushes at scale 0.7
(1.26 m high, the Builder's minimum scale) and occasional rock accents at 1.05 m.
The planting is added after the animal groups and habitat plants, preserving their seeded
construction sequence. All habitats retain their own reachable information board.

## Stand-off strips

`Tile::standOff` is an explicit reservation, changed through `Builder::PlanStandOff` and
`PlaceStandOff`. Marking costs $10 per tile in Landscaping; removal is free, like path
removal, and leaves independently built rails and plants in place. Planning deduplicates
tiles and checks affordability before any edit. Marking rejects paths, buildings, water,
functional props, the entrance, and all existing enclosures (including occupied ones).
Paths, water and buildings cannot subsequently be placed on a strip. Trees, bushes, rocks
and decorative props may stand there; feeders, toys and visitor furniture may not.

`Enclosures::Rebuild` separates marked ground from ordinary connected components and
classifies it as `Region::StandOff`, with no enclosure id, name or habitat statistics.
Fencing an unmarked pocket still makes an ordinary enclosure. Strip tiles never appear in
scenario habitat totals, visitor habitat choices or information-board associations.
Save format 2 carries a row-major `stand_off` coordinate array in the zoo envelope. Format 1 saves omit
it and load with no strips; the base world JSON format is unchanged. The version bump prevents
older readers from turning planted reservations into unnamed habitats.

A viewing corridor is axis-aligned: public path, rail, one or more marked tiles without
internal fences, containment barrier, enclosure. `ViewingSpot::side` points toward the
animals, and `standOffM` measures from the path tile's edge to the containment barrier
(2 m per crossed tile, zero for a direct view). Visibility is the product of containment
and rail visibility. An absent path, absent rail, opaque barrier, intervening fence or
diagonal route does not produce a view. Viewing-tile counts, mean visibility, crowd counts
and visitor destinations use these same spots; visitors stand at the outer rail.
Only the animal barrier contributes to minimum height/strength and the weakest edge.

The new layout exposed an existing navigation endpoint case: in seed 2, flamingo Sara's
ordinary movement landed exactly on the south glass edge at `(214.335, 272)` m, and
`Navigator::Walkable` accepted it even though tile ownership had crossed the fence.
`AnimalMovement` now also checks enclosure ownership before an ordinary step. This keeps
animals off ground outside their enclosure, including strips, without altering deliberate
`BreakOut` movement or the behaviour of already loose animals. A small endpoint regression
covers both public and stand-off ground and verifies that intentional breakouts still cross.
The generic endpoint issue in `src/core/zoo/nav/Nav.cpp` remains a follow-up for its owner.


The absence of paths excludes visitors from strips; the rail and containment fence enclose
them. Animals still use their existing enclosure constraints and escape behaviour.
Litter drops that would land on marked ground are suppressed; existing litter on newly
reserved ground is cleared on the next simulation step. The low rail is not an
animal containment substitute: tall mesh remains behind the hoofstock planting and the
original cable barrier remains behind the elephant/rhino planting.

The entrance has the gift shop, education centre, refreshments, restroom, fountain and statue.
The central food court groups burger, drinks and ice-cream stalls around picnic tables and
acacia islands; another court sits below the northern predators. Restrooms serve the entrance,
central court and northern court. Additional kiosks serve the outer circuits. There are two
fountains, two statues, lamps, bins, flower beds and groves along the walks. Feed reserves cover
at least eighteen hours for the principal species, spanning the 15:00–07:00 care interval.

Catalog limits: there is no low glass wall and no low rail that would hold a flamingo, so the
small animals stand behind the same 3.5 m glass as the lions. Tigers and bears need a 4 m barrier of strength 4: their viewing fronts are the
4.5 m `high_glass_wall`, the rest of the boundary 4.5 m `heavy_mesh`.

## Stand-off welfare measurements

Seeds 1 and 2 produce the same figures below, before and after this change. Habitat welfare
is `EvaluateHabitat(...).overall`; filled welfare is the species mean of `Animal::welfare`
after `FillForVisit(..., {.fillTicks = 600})`. Tests retain the recorded pre-change values
and reject decreases beyond float rounding. Space scores remain 1.0, and all areas in the
fronts table are retained, including Acacia Plain's 4,284 m² against a 3,840 m² requirement
from the giraffes' mixed-group evaluation. Elephant Valley retains 5,436 m² against 1,980 m²;
Rhino Hollow retains 1,692 m² against 1,100 m².

| Species | Habitat welfare before → after | Filled welfare before → after |
|---|---:|---:|
| Giraffe | 0.964078784 → 0.964078784 | 0.949947774 → 0.949947774 |
| Zebra | 0.978447199 → 0.978447199 | 0.945476532 → 0.945476532 |
| Ostrich | 0.970645428 → 0.970645428 | 0.970760703 → 0.970760703 |
| Lion | 0.970187545 → 0.970187545 | 0.938474238 → 0.938474238 |
| Tiger | 0.976001620 → 0.976001620 | 0.902444363 → 0.902444363 |
| Wolf | 0.978674769 → 0.978674769 | 0.927070796 → 0.927070796 |
| Brown bear | 0.969099939 → 0.969099939 | 0.976925611 → 0.976925611 |
| Polar bear | 0.962766051 → 0.962766051 | 0.976536632 → 0.976536632 |
| Elephant | 0.952969968 → 0.952969968 | 0.937010646 → 0.937010646 |
| Rhino | 0.976396203 → 0.976396203 | 0.969400227 → 0.969400227 |
| Camel | 0.985593915 → 0.985593915 | 0.975913942 → 0.975913942 |
| Kangaroo | 0.991231322 → 0.991231322 | 0.978923440 → 0.978923440 |

Containment remains 1.0; tiger and both bear species improve from 0.99999994 to 1.0 as the
4 m bars become 4.5 m mesh. Public rails are excluded from these containment figures.
The existing seed-1/2 visit tests check every step for 2,000 steps following a 600-step fill:
no animal in the six strip habitats is closer to any public path tile than the specified
minimum clearance, and no visitor or litter occupies a strip. Every habitat has watchers
at the fill and readable information boards. The 6,000-step variants remain disabled in
the routine suite; their golden hashes are recorded separately.

The following `Zoo::StateHash()` values were printed and pasted into `VisitStartTests.cpp`;
“later” means 500 further steps with the calendar still stopped.

| Fill ticks | Seed | At fill | Later |
|---:|---:|---:|---:|
| 600 | 1 | 648016986628378690 | 1159593659289431489 |
| 600 | 2 | 11333951371405565671 | 3327385516196825043 |
| 600 | 3 | 12847727589574245904 | 399561598498052959 |
| 6000 | 1 | 4119655421383573257 | 3513100252823275081 |
| 6000 | 2 | 6197420463027346240 | 10941191729566125093 |
| 6000 | 3 | 15964635073259134339 | 15096508033282633152 |


## The zoo as the game visits it

The game builds this scenario without any species that has no model in `assets/animals/`
(`withoutSpecies`); such a habitat would stand finished but empty. Since 2026-11-12 all fifteen
species have one, so a seed-1 visit has all **74 animals in fourteen habitats**.

The ice-cream kiosk formerly in the narrow Elephant Valley/Kangaroo gap is relocated
near entrance coordinates `(-5,18)`, facing north, to keep refreshment access around the wider
walks. Three explicit planting tiles at `(20,28)`, `(21,29)` and `(22,30)` close diagonal
slivers between Ostrich Rise and Flamingo Lagoon; the lagoon's containment is preserved.

The visitor system opens at 09:00. A visit starts at 10:30: the game steps the zoo from the
opening before the first frame, then `Zoo::SpreadVisitors()` sets the groups that have arrived
down over all the paths (a visit on foot would otherwise meet the whole morning's crowd in a
column at the gate). The spread places one eligible group at each reachable habitat
front first, then scatters the remaining groups over the walks; the visit pace (`VisitorPace`: linger, stay, arrivals) keeps them at the
habitats. Historical measurements from a seed-1 visit before the stand-off layout
(`zoo: visit for ...` lines of the game; these are not baselines for the revised layout):

| | |
|---|---:|
| Visitors simulated (the cap, `Visitors::kMaxVisitors`) | 800 |
| Walking | 415 |
| Watching animals | 177 |
| Eating | 153 |
| Queueing | 15 |
| Sitting | 10 |
| Reading a board | 8 |
| Habitats being watched | 10 |

The core scenario on its own (no spread, all species, the simulation's default pace) is measured
by the disabled test `VisitorZooTests.DISABLED_MorningAndCrowdFigures`; under AddressSanitizer it
runs for about 1,000 seconds.

## Game hand-off

No game files were changed or built. `rg -n 'Region::' src/game` finds no matches in this
checkout, so there are no existing Region switches to extend here. The render-side work
can use `Tile::standOff` in `GroundPaint.cpp` / `TerrainRenderer.cpp`, refine mesh and rail
geometry in `FenceModels.cpp` (submitted by `SceneryRenderer.cpp`), and optionally use
`ViewingSpot::standOffM` in `ZooGameLook.cpp`'s viewing-camera selection. The information-board
association there is still covered by the habitat tests. `HudBuild.cpp` sizes its fence
palette from `catalog.fences.size()` and iterates the catalog; both new kinds appear without
a separate palette edit.

Public core additions are `Tile::standOff`, `World::SetStandOff`, `Region::StandOff`,
`ViewingSpot::standOffM`, and the Builder check/plan/place reservation operations and price.
The save version is 2, with version 1 loading retained. Standalone world JSON exports do not
carry the zoo-envelope reservation layer; consumers needing complete zoo state should use
`ZooSave`. The generic navigator endpoint correction described above is outside this task's
file ownership and remains recommended for its owner.

## E1 verification (2026-11-13)

The existing `build-asan/` configuration was reused (Debug, AddressSanitizer, game disabled,
shared libcna checkout, ccache launchers). Every build/test invocation used:

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv ASAN_OPTIONS=detect_leaks=0
cmake --build build-asan --parallel 2
ctest --test-dir build-asan --output-on-failure -j2
```

The final build succeeded. The full suite passed **178 enabled tests, zero failures**, with
9 tests disabled, in **891.78 seconds** (`build-asan/e1-final-suite.log`). The suite does not
meet a literal “few seconds” target: existing visitor-zoo and multi-day simulations already
took tens to hundreds of seconds before E1 (the baseline equal-seeds test took 189.49 s).
The new small core regressions take milliseconds; the 6,000-step tests remain disabled.

Focused verification commands, with the same environment:

```sh
build-asan/tests/zoo_core_tests --gtest_filter='AnimalMovementTests.*:StripFixture.*:StandOff.SaveRoundTripAndLegacySave:StandOff.CannotReserveAnAnimalsOccupiedHome:Seeds/VisitorZooHabitat.VisibleAndContainedDuringVisit/*'
build-asan/tests/zoo_core_tests --gtest_filter='StandOff.ReservingGroundClearsExistingLitterOnTheNextStep'
build-asan/tests/zoo_core_tests --gtest_also_run_disabled_tests --gtest_filter='VisitStart.DISABLED_FillHashesFull'
```

Results: **13/13 passed** (including both seeds' 2,000-step visits and intentional escape
regressions); **1/1 passed** for existing-litter removal; **1/1 passed** for full-fill hashes
(all three seeds, at 6,000 and 6,500 steps, 354.734 s). The routine suite also passed the
three short-fill hash pairs and both save/load continuation tests.

The measurements and hash recordings used these commands before/after the layout changes:

```sh
build-asan/tests/zoo_core_tests --gtest_also_run_disabled_tests --gtest_filter=StandOff.DISABLED_RecordHabitatFigures
build-asan/tests/zoo_core_tests --gtest_also_run_disabled_tests --gtest_filter='VisitorZooTests.DISABLED_PrintPlan'
build-asan/tests/zoo_core_tests --gtest_also_run_disabled_tests --gtest_filter='VisitorZooTests.DISABLED_PrintPlan:StandOff.DISABLED_RecordHabitatFigures:VisitStart.DISABLED_FillHashesFull'
build-asan/tests/zoo_core_tests --gtest_also_run_disabled_tests --gtest_filter='VisitStart.FillHashesShort:VisitStart.DISABLED_FillHashesFull'
```

The original figure recording passed (1 test). After correcting the kiosk location, the
combined recording passed the figures and plan tests; its old full-fill hash expectations
failed as expected. The final post-containment recording passed the short hashes and found
the changed seed-2 full pair; those printed values were pasted into fixed assertions and
then verified successfully by the full-fill command above.

During implementation, core checks first passed 4/5 (a scenery-placement exception and
fixture funds were corrected); layout checks progressed from 5/9 to 8/9, exposing the
three diagonal pockets, missing watchers, and the fence-endpoint crossing. The preliminary
whole suite ran 175 enabled tests with 10 failures: eight tests assumed the last catalog
entry was their strong fence, one used the previous hashes, and one exposed Sara's crossing.
The fixtures now select the original `high_glass_wall` by id. No assertions were weakened;
visitor spreading's former “all idle” assertion now accepts and validates seeded watchers.
The information-board checks retain their original association/readability intent. Early
baseline and pre-litter suite runs were interrupted and are not claimed as full passes.
All modified C++ files were run through `clang-format-19 -i`; `git diff --check` is clean.

Additional diagnostic invocations used the same executable and environment:

```sh
build-asan/tests/zoo_core_tests --gtest_filter='StripFixture.*:StandOff.SaveRoundTripAndLegacySave'
build-asan/tests/zoo_core_tests --gtest_filter='StripFixture.*:StandOff.SaveRoundTripAndLegacySave:Seeds/VisitorZooHabitat.EveryHabitatHasItsOwnReadableBoard/*:Seeds/VisitorZooHabitat.VisibleAndContainedDuringVisit/*'
build-asan/tests/zoo_core_tests --gtest_filter='Seeds/VisitorZooHabitat.VisibleAndContainedDuringVisit/1'
```

These produced the intermediate 4/5, 5/9, 8/9, and reproducible seed-2 containment failures
summarised above, before the final passing run. The first sandboxed build could not write
the mandated shared ccache and was rerun with access to that existing cache. Incremental
builds also caught and corrected two test compile errors (a const environment helper and
an incorrect activity enum name). No new cache, build directory, dependency, branch,
commit, push or pull request was created.

## Integration in the main tree (lead, 2026-11-13)

What the game needed, and what the first look at it changed:

- **Look.** The public rail of a strip is a leaning rail with a broad top board
  (`FenceLook::handrail`); `heavy_mesh` has square posts and a welded frame
  (`FenceLook::heavy`); strip tiles are painted as bark mulch (`GroundPaint.cpp`). Chain-link
  was a 15 cm net of thumb-thick light-grey wire that read as a cage: it is a 6 cm net of thin
  dark wire now (`Surfaces.cpp`, `fences.json` colours), which the eye looks through.
- **Nobody on the planting.** Main's newer "stand in an arc by the bench" had no walkability
  check, and one visitor stood 3 cm inside a strip. Those places are pulled back to ground
  that can be walked to, and `Navigator::Passable` refuses strip tiles for every profile.
- **An animal walked through its trough** when its place was on the far side (the prop it
  is bound for was no obstacle at all). It may come right up to it now, not through it
  (`AnimalMovement.cpp`).

### Who watches where

After the 6,000-step fill the star habitats were often nearly unwatched: the choice was
"best score wins", the score mostly nearness, so the habitats beside the busiest paths took
everybody (seed 1 at the end of the fill: Meerkat Lookout 52, Kangaroo Grove 30, Acacia
Plain 0, Tiger Forest 2, Wolf Wood 3). Three changes, all in `Stepper::TryWatch` and the
visit's pace:

1. The habitat is drawn **by lot** among those not yet seen twice, with the weight
   `(0.2 + appeal)^2 * (60 / (60 + metres))^2 / (1 + watchers per viewing tile)`, times 0.6
   for a second look. A less starry or further habitat gets its share instead of nothing,
   and a full front draws fewer.
2. **A slower visit** (`VisitStartOptions::pace`, which the game uses): linger 9 (72 to
   126 s at a fence, a meal 45 to 72 s), stay 12 (forty minutes in the zoo, needs half as
   often). At stay 6 one visitor in three was on the way to a stall, a restroom or a bin.
3. The full-length test asks for somebody at each habitat's front within 2,000 steps
   rather than at one instant.

Measured with `Seeds/VisitorZooHabitat.DISABLED_PrintWatchersOverTime` (800 people, thirteen
samples 500 steps apart after the fill, two seeds):

| | watching, of 800 | fewest at any habitat | most at any habitat |
|---|---|---|---|
| before (best score, linger 6, stay 6) | 110 to 215 | 0 (several habitats, most samples) | 52 |
| after | 192 to 260 | 1 to 3 (one sample of 26 shows a 1) | 38 |

The game's own line at the start of a visit (seed 1, 10:30): 245 watching, between 4 (Wolf
Wood) and 30 (Ostrich Rise) at every one of the fourteen habitats. Both
`DISABLED_VisibleAndContainedFull` variants pass. The golden fill hashes were recorded again.

**Still open at the fronts:** hoofstock is seen across the strip through 3.5 m chain-link (a
real zoo would use a moat or a low cable fence there); the strip's shrubs are the same
coarse blobs as every other bush; the stand chooser of `--stand` may still prefer a mesh side
to the front when the animals are nearer to it.
