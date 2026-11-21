# Greenhill Zoo — visitor plan

Greenhill has free admission and automatic animal care. Its 224 × 224 tile map measures
448 × 448 m (200,704 m²). The fenced zoo occupies 424 × 424 m (179,776 m²).
The third expansion adds 64 m on each side of the previous map, including 54,272 m² inside
the property. The two earlier extensions added 128 m to the north.
North leads into the zoo from the south gate. Entrance coordinates `(east, north)` mean
world tile `(entrance.x + east, entrance.z - north)`; one tile is 2 m. The entrance tile is
`(111,217)`, centred at world `(223,435)` metres. Construct with `VisitorZooConfig(seed)`,
then call `BuildVisitorZoo(zoo, options)`. `withoutSpecies` suppresses animals only.

```text
                              NORTH
                              Snow Leopard Heights
                                /             \
       Red Panda Canopy — northern woodland circuit + picnic glade — Lynx Ridge
                      /             |                 /
             Polar Sound     Bear Falls — Wolf Wood — Tiger Forest
                  |              \    northern court    /       |
Capybara Marsh — Penguin Cove === woodland shortcut ======= Lion Rock
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

Two gravel walks link existing galleries through the new land. On the west, follow the path
from Elephant Valley past Capybara Marsh to Polar Sound for a picnic stop with ice cream and toilets. The eastern
walk links Acacia Plain and Tiger Forest, with drinks, picnic tables and toilets halfway.

The asphalt round bends between habitats; gravel circuits, curved viewing bays and shortcuts
make shorter visits possible. Boardwalks follow the lagoon and Penguin Cove. Habitat outlines
combine elliptical lobes. Every habitat has a keeper gate, shelter, stocked feeders, water,
enrichment, a public information board and benches. Planting concentrates behind the open
viewing ground, with woods between habitats and around the perimeter.

The seed-1 zoo contains **107 animals in 18 habitats, 55 buildings and 4,680 props** when every species
is included (74 animals and 2,817 props before the plan's rework of
2026-11-14, below). The farthest public path tile is **78.1 m** walking distance
from a visitor facility. `VisitorZooTests.DISABLED_PrintPlan` prints these measurements. Families
are written out in "The zoo's plan" below. **S** = south; **W/E** = west/east.
The principal fronts face south and the zoo's centre. Habitat areas are unchanged: the public
walks and rails move outward, preserving all space and habitat provisions. J30 recesses one outer
corner of the rail between Elephant Valley and Kangaroo Grove without moving either habitat.

The two boards flanking the entrance are visitor boards rather than habitat boards. They welcome
the visitor, give the 9:00–19:00 opening hours and 16:30 last admission, count the current animals
and habitats, and point to the map and the species boards. A board within eight metres of a lived-in
habitat still shows that habitat's animals instead. The inspected entrance panel is in
`evidence/entrance-welcome-board-j25-2026-11-20.jpg`. Both entrance boards and the first habitat
board were opened in real off-screen runs; all 313 active CTest cases pass (21
diagnostics remain disabled).

| Habitat | Animals (of them young) | Area, m² | m² per animal | Front | Back | Stand-off depth |
|---|---|---:|---:|---|---|---:|
| Meerkat Lookout | Meerkat 5 (2) | 252 | 50 | Glass S/W | Mesh-backed wildlife hedge | 0 |
| Flamingo Lagoon | Flamingo 12 (2) | 796 | 66 | Glass S/W | Wire mesh | 0 |
| Acacia Plain | Giraffe 5 (2); zebra 6 (2) | 4,284 | 389 | Public rail, planting, tall cable fence S/W | Tall cable fence | 1 tile / 2 m |
| Ostrich Rise | Ostrich 7 (3) | 1,016 | 145 | Public rail, planting, tall cable fence S/W | Tall cable fence | 1 tile / 2 m |
| Lion Rock | Lion 7 (4) | 2,236 | 319 | Glass S/W | Heavy welded mesh | 0 |
| Tiger Forest | Tiger 2 | 1,788 | 894 | High glass S/W | Heavy welded mesh | 0 |
| Wolf Wood | Wolf 6 (4) | 1,788 | 298 | Glass S/W | Tall mesh | 0 |
| Bear Falls | Brown bear 3 (2) | 2,236 | 745 | High glass S/E | Heavy welded mesh | 0 |
| Polar Sound | Polar bear 3 (2) | 2,812 | 937 | High glass S/E | Heavy welded mesh; cooled den | 0 |
| Penguin Cove | Penguin 13 (3) | 1,044 | 80 | Glass S/E | Wire mesh | 0 |
| Elephant Valley | Elephant 5 (2) | 5,436 | 1,087 | Public rail, scrubbed dry moat S/E | Cable service side | 2 tiles / 4 m |
| Rhino Hollow | Rhino 4 (1) | 1,692 | 423 | Public rail, scrubbed dry moat S/E | Cable service side | 2 tiles / 4 m |
| Red Dune | Camel 5 (2) | 1,616 | 323 | Public rail, planting, timber paddock fence S/E | Timber paddock fence | 1 tile / 2 m |
| Kangaroo Grove | Kangaroo 7 (3) | 1,136 | 162 | Public rail, planting, timber paddock fence S/E | Timber paddock fence | 1 tile / 2 m |
| Lynx Ridge | Eurasian lynx 3 (2) | 1,500 | 500 | Glass S/W | Tall mesh | 0 |
| Red Panda Canopy | Red panda 3 (2) | 992 | 331 | Glass S/E | Tall mesh | 0 |
| Snow Leopard Heights | Snow leopard 3 (2) | 1,724 | 575 | High glass S/W | Heavy welded mesh | 0 |
| Capybara Marsh | Capybara 8 (4) | 988 | 124 | Glass S/E | Wire mesh | 0 |

The areas did not change with the families; the area per animal is our estimate of what reads
as roomy for the species, **not checked against EAZA or AZA husbandry figures**. Every group
lies within its species' `groupMin`..`groupMax` and keeps to `maxAdultMales`.

The six habitats with strips also have them beside their back circuits: the public cannot
approach the same animals from behind at a smaller distance. Square dilation of the curved,
rasterised footprints keeps at least the stated clearance even at corners; some sightlines
cross more tiles. Straight portions of both principal fronts have exactly the tabulated depth.
There are 999 explicitly marked strip tiles in both tested seeds, with one low, seeded shrub
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
Save format 2 introduced the row-major `stand_off` coordinate array in the zoo envelope. Format 1
saves omit it and load with no strips; the base world JSON format is unchanged. Current format 3
also permits stable fence kind 16, the mesh-backed wildlife hedge. The version bumps prevent older
readers from turning planted reservations into unnamed habitats or misreading a new barrier kind.

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
central court, northern court and the new woodland glade. A drinks kiosk on the outer ridge keeps
the longer northern circuit within the same service-distance limit. Additional kiosks serve the
outer circuits. There are two
fountains, two statues, lamps, bins, flower beds and groves along the walks. Feed reserves cover
at least eighteen hours for the principal species, spanning the 15:00–07:00 care interval.

## Northern woodland expansion (J78, 2026-11-21)

The Visitor Zoo map grows from 160 × 160 to 160 × 192 tiles while retaining its 160-tile width,
so the entrance and all fourteen established habitats keep their entrance-relative positions.
The extra 32 rows add 20,480 m² north of the old plan. A three-tile-wide asphalt circuit branches
from both ends of the predator walk, reaches 348 m north of the gate and leaves a planted margin
before the boundary wall. A two-tile gravel spine joins its crown to the old northern court.

At `(0,164)`, a cobbled oval surrounds a grass picnic glade. Six collision-valid picnic tables,
two benches, lamps, bins and an information board sit with a dedicated drinks stall and restroom;
the path is not cut into narrow teeth around the furniture. In both plan seeds every checked point
is reachable from the entrance, at least 180 paved tiles lie in the new northern band, ordinary
walks remain at least 4 m wide and the steepest paved tile is 3.32 %. Seed 1's farthest public path
is 84.8 m from a visitor facility, below the retained 85 m limit. The larger public parkland raises
the seed-1 prop count from 2,806 to 3,205 and buildings from 41 to 43. It deliberately added no
second copy of an existing species: J79 now uses the reserved eastern land for a genuinely new
habitat.

The cost is recorded rather than hidden. In the matched normal off-screen game, scenery rises
from 2,357,290 to 2,844,153 triangles and from 465 to 511 draw calls. The game built the seed-1
scenario with all 90 animals and filled 800 visitors; the inspected eye-level glade and before/after
plans are in `evidence/northern-expansion-j78-2026-11-21.jpg`. The visit-cache revision is bumped so
an older 160 × 160 cached visit cannot mask the new map. All 307 active CTest cases pass, with 21
long-running diagnostics disabled as intended. A 3,000-tick ASan/UBSan glade visit also completed
cleanly with leak detection disabled, loading all 90 animals and simulating 627 visitors.

## Lynx Ridge (J79, 2026-11-21)

Lynx Ridge occupies 1,500 m² wholly inside the northern extension, centred 320 m north and
116 m east of the gate. Its south and west glass gallery supplies 66 public viewing tiles; the
remaining 3.5 m tall-mesh boundary encloses spruce, pine, birch and juniper cover, granite,
a fallen log, climbing frame and scratching post. Water covers 4.5 % of the habitat, planted
cover 36.0 %, and an 18-tile shelter and raised meat platform support the opening family.

The residents are a six-year-old mother and her female and male 0.45-year-old kittens. The
short black-tipped tail, broad paws, spotted coat, facial ruff and ear pencils in both dedicated
models follow the Eurasian-lynx description in
[Animal Diversity Web](https://animaldiversity.org/accounts/Lynx_lynx/); enclosure and safety
choices use the medium-cat guidance in the
[EAZA felid best-practice guidelines](https://media.eaza.net/uploads/EAZA_Best_Practice_Guidelines_FINAL_SM_f4188181cd.pdf).
The habitat raises seed 1 to 93 animals, 15 habitats, 45 buildings and 3,234 props. Both plan
seeds keep every Lynx Ridge tile in the new northern band, all viewing positions reachable, and
the farthest public tile 84.8 m from a visitor facility. Adult and kitten turntables, the full
plan and the inspected in-game public view are in
`evidence/lynx-ridge-j79-2026-11-21.jpg`. All 309 active CTest cases pass (21 diagnostics are
disabled as intended), and a 3,000-tick ASan/UBSan visit completed cleanly with all three lynxes
outdoors and 637 visitors simulated.

## Red Panda Canopy (J80, 2026-11-21)

Red Panda Canopy occupies 992 m² on the western side of the northern extension, centred 320 m
north and 88 m west of the gate. A short three-tile-wide gravel spur joins its continuous public
gallery to the woodland circuit. Its 54 glass-front viewing tiles are reachable in both plan
seeds; the remaining 3.5 m tall-mesh boundary encloses bamboo, birch, oak and fern cover, granite,
a climbing frame and fallen log. Water covers 2.8 %, planted cover 22.9 %, and an eight-animal
shelter and new low bamboo-and-fruit feeder support the opening family.

The residents are a five-year-old mother and two 0.2-year-old cubs. This mother group, the
bamboo-heavy diet and the compact masked body with a long ringed tail follow the
[Smithsonian National Zoo](https://nationalzoo.si.edu/animals/red-panda); the varied elevations,
shade and climbing furnishings follow the
[EAZA Red Panda Best Practice Guidelines](https://media.eaza.net/uploads/EAZA_Best_Practice_Guidelines_Red_Panda_final_Dec_2021_1_ffc7f12345.pdf).
The zoo rises to 96 animals, 16 habitats, 47 buildings and 3,287 props. A dedicated western drinks
stall keeps the farthest public walk at 84.77 m from a facility, below the 85 m design limit.
Adult and cub turntables and the inspected 3,000-tick public view are in
`evidence/red-panda-canopy-j80-2026-11-21.jpg`; that run loaded all 96 animals, kept all three
pandas outdoors and simulated 650 visitors. Expanding from 15 to 16 habitats also exposed the
old keeper-pairing fallback: it assigned the two new habitats at opposite ends of the northern
land to one 204 m round. The exact memoised planner now gives all eight keepers globally shortest
two-habitat rounds; the staffed week improves on automatic care at 18 against 20 animal-hours in
need, and the visitor-paced morning at 10 against 13. All 311 active CTest cases pass (21
diagnostics remain disabled), as do all 155 animal-pipeline tests. A final 3,000-tick ASan/UBSan
visit completed without a sanitizer finding with leak detection disabled, loading all 96 animals,
47 buildings and 3,287 props and simulating 649 visitors.

## Snow Leopard Heights and the second northern stage (J81, 2026-11-21)

The map gains a second 32-row northern stage, another 20,480 m², for a total depth of 224 tiles.
Two three-tile-wide asphalt arms leave the old woodland crown independently and meet the continuous
public gallery around Snow Leopard Heights, so the new end is a loop rather than a dead end. A
restroom and drinks stall at 376 m north keep the farthest seed-1 public walk at 78.1 m from a
visitor facility, below the retained 85 m limit.

Snow Leopard Heights is a 1,724 m² alpine bowl centred 392 m north of the gate. Its 70 reachable
high-glass views face a granite, juniper, pine and spruce habitat with an 18-place shelter, raised
meat platforms, climbing frame, scratching post, fallen log and two separated water troughs. The
opening group is a seven-year-old mother and her female and male 0.45-year-old cubs. The smoky
rosette coat, broad paws, small rounded ears, long thick balancing tail, solitary adult life and
mother-with-cubs group follow the [Snow Leopard Trust fact sheet](https://snowleopard.org/wp-content/uploads/2025/06/Downloadable-Snow-Leopard-Fact-Sheet.pdf)
and [San Diego Zoo species account](https://animals.sandiegozoo.org/animals/snow-leopard). The
procedural adult and cub are deliberately documented as stylised rather than scan-derived.

The zoo now has 99 animals, 17 habitats, 50 buildings and 3,631 props. Nine keepers cover the odd
number of habitats; the last habitat has its own keeper. Red Panda Canopy, Lynx Ridge and Snow
Leopard Heights each receive two water points. Feeding, watering and routine inspections stay
outside very shy habitats; a keeper makes only one brief inside cleaning pass per round. Over a
closed week the staffed zoo records 13 animal-hours in need against automatic care's 26; at visitor
pace the comparison is 21 against 30. Both plan seeds
retain the full habitat area, family, enrichment, water redundancy and reachable viewing loop.
The adult turntable, cub model and inspected 3,000-tick public view are in
`evidence/snow-leopard-heights-j81-2026-11-21.jpg`; that run loaded all 99 animals, kept all three
snow leopards outdoors and simulated 658 visitors.

Catalog limits: there is no low glass wall and no low rail that would hold a flamingo, so the
small animals stand behind the same 3.5 m glass as the lions. The meerkats' service boundary is
now a lower green wildlife hedge with its real containment mesh hidden in the foliage. Tigers and
bears need a 4 m barrier of strength 4: their viewing fronts are the 4.5 m `high_glass_wall`, the
rest of the boundary 4.5 m `heavy_mesh`.

## East and west garden walks (2026-11-21)

Both walks turn back into the zoo at a different habitat gallery. Walking west from Elephant
Valley, visitors pass willows and a stop for ice cream before reaching Polar Sound. On the
eastern side, the refreshment stop stands among acacias on the walk from the giraffes and
zebras at Acacia Plain to Tiger Forest. Each stop has a restroom. The 160-tile-wide layout is
still available to callers building a zoo to the previous dimensions.

On seeds 1 and 2 the visitor simulation offers all four picnic tables, six seats each.
The visitor map offers 27 distinct destinations, including both picnic stops. Clicking them
places the player on the west walk at (31, 269) m and on the east walk at (415, 267) m.
At 10:30 on seed 1, 15 visitors were within 20 m of the west plaza and 12 were within 20 m
of the east plaza. The plan and both views are in `evidence/east-west-gardens-2026-11-21.jpg`.
A gallery from each of the four bordering habitats is within 180 m of its respective
rest stop. In seed 1 the farthest path anywhere in the park is 78.1 m from a facility.
Tall grass in the woodland glades brings Bear Falls to 43.0% vegetation cover after
the expansion.

The following opening states were recorded before the marsh was stocked.

| Fill ticks | Seed | Opening state | After 500 ticks |
|---:|---:|---:|---:|
| 600 | 1 | 6149812909312757473 | 9743607491872979440 |
| 600 | 2 | 3581511720324067494 | 5080038278314224431 |
| 600 | 3 | 12504103261764293381 | 290601053194212960 |
| 6000 | 1 | 1284744869770772916 | 8424761712365639015 |
| 6000 | 2 | 7515209334301186647 | 17005395279587903364 |
| 6000 | 3 | 14847577528289621555 | 17632415333599148855 |

## Capybara Marsh (2026-11-21)

Between the west picnic stop and Penguin Cove, two short approaches join the marsh gallery to
the garden walk. The 988 m² enclosure has 54 public viewing tiles. A 272 m² pool has a deep
centre and a shallow shelf with reed stands along its banks. A heated barn shelters the group,
and low trays hold hay and grass pellets. One adult male and three adult females share the marsh
with a litter of four. The group, water and grazing areas follow descriptions of capybaras on
South American riverbanks. [San Diego Zoo Wildlife Alliance](https://animals.sandiegozoo.org/animals/capybara)

From the public gallery at 13:00 all eight capybaras were outdoors. The binoculars show
adults and pups paddling together while the mother eats on the bank. The visitor map offers
28 distinct destinations including the marsh and the adjacent picnic stop. Clicking the
marsh marker places the visitor at (69, 289) m on the public gallery.
[Afternoon gallery view and visitor map](evidence/capybara-marsh-2026-11-21.jpg)

Opening states for the default zoo with the marsh and the revision-7 flock resting behaviour,
measured again 500 ticks later:

| Fill ticks | Seed | Opening state | After 500 ticks |
|---:|---:|---:|---:|
| 600 | 1 | 9139815037628863271 | 2085363316060078965 |
| 600 | 2 | 16001145640700442961 | 5644383874109006456 |
| 600 | 3 | 10636984344542905811 | 6304807489940044530 |
| 6000 | 1 | 1054235427821063531 | 8043702145645978796 |
| 6000 | 2 | 11740243411003087215 | 9360760697049082900 |
| 6000 | 3 | 11257981172144800077 | 1213298953784258099 |

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


## The zoo's plan (G8, 2026-11-14)

Begun by a Codex task (G8, G8b, G8c) that was stopped three times by limits without a report;
finished, reviewed and measured by the lead. All of it is in `VisitorZoo.cpp`; the checks are
`tests/core/VisitorZooPlanTests.cpp` (both seeds unless said otherwise).

**Families.** Each habitat is stocked as a zoo keeps the species (`Family()`): a breeding group
with this season's young for the social species, a pair for tigers, and a mother with two cubs for
each bear species. The first of a family is its sire except in Elephant Valley and the bear mother
groups, where the sire lives elsewhere and is left unset. Ages in years on the opening day:

| Species | Adults | Young (mother) |
|---|---|---|
| Meerkat | M 4, F 5, F 3 | M 0.15, F 0.15 (F 5) |
| Flamingo | M 9, 14, 7, 20, 11; F 10, 16, 6.5, 12, 8 | F 0.8, M 0.9 |
| Giraffe | M 9, F 8, F 12 | F 3.0 (F 12), F 0.8 (F 8) |
| Zebra | M 8, F 6, 9, 12 | F 1.2, M 0.4 |
| Ostrich | M 7, F 6, 8, 5 | M, F, F 0.3 |
| Lion | M 7, F 6, F 9 | F 2.6; M, F, F 0.6 |
| Tiger | M 7, F 6 | none |
| Brown bear | F 10 | F, M 0.7 (F 10) |
| Polar bear | F 9 | F, M 0.6 (F 9) |
| Wolf | M 6, F 5 | F 1.3; M, F, M 0.25 |
| Penguin | M 5, 8, 11, 6, 14; F 6, 9, 12, 5, 10 | F, M, F 0.25 |
| Elephant | F 26, F 38, F 22 | F 7 (F 26), M 1.5 (F 38) |
| Rhino | M 14, F 16, F 10 | F 1.0 (F 16) |
| Camel | M 9, F 8, F 13 | F 2.0, M 0.7 |
| Kangaroo | M 6, F 4, 5, 7 | F 1.2; M 0.7, F 0.65 |

A young animal is set down beside its mother, knows her (`Animal::mother`, and the sire as
`father`), and her `lastBirthTick` is its birth, so she is not in calf again on the opening
day. Under a quarter of the age of maturity a juvenile suckles and follows its mother
(`docs/ANIMAL_BEHAVIOUR.md`); under half the renderer draws the juvenile model where one exists.
The calendar has 48 days a year, so a week ages an animal by 0.146 years: the youngest stay
sucklings for a simulated week, which `TheYoungAreStillYoungAndFedAfterAWeek` checks (no death,
health above 0.8, hunger below 0.9, mother in the same habitat). The kangaroos are the
exception: a joey under half a year lives in the pouch, which the simulation does not have, so
their joeys are young at foot (0.65 and 0.7 years), past the suckling quarter and drawn young.
Older daughters are female, so that no second adult male grows up into a fight. Both live bear
labels and both dedicated cub bodies are shown in
`evidence/bear-families-j47-2026-11-20.jpg`.

**Barriers for hoofstock.** Nothing hoofed stands behind chain-link any more. Two catalog rows
were added (`data/construction/fences.json`): `paddock_rail` (kind 13, timber, 2.4 m,
strength 3, visibility 0.9) for camels and kangaroos, `tall_cable_fence` (kind 14, cables,
3.0 m, strength 3, visibility 0.92) for giraffes, zebras and ostriches. The J18
`dry_moat` (kind 15, 2.4 m effective height, strength 5, visibility 1.0) now fronts elephants
and rhinos. Each is a barrier the species' own figures accept (containment 1.0).

**Dry moats and ha-has (J18, 2026-11-20).** Elephant Valley and Rhino Hollow use their existing
two-tile reservation as a four-metre-wide dry ditch. Only a vertex surrounded by four reserved
tiles is lowered, by 1.8 m, giving a V profile without moving either the habitat lip or the public
path lip. Rocky ground, sparse low scrub and a continuous drain sit below a 2.4 m animal-side
retaining wall; its 0.29 m coping marks the edge without interrupting the view. Edges beside the
rear service apron remain `cable_barrier`, including the keeper gate. The catalog exposes the moat
as a buildable style; a player grades its approach with the ordinary terrain tool.

Both habitat areas, welfare figures and 999 planted stand-off tiles are unchanged. The steepest
public path tile remains 2.8 % in seed 1 and 2.7 % in seed 2. Short and 6,000-step state hashes are
repeatable for seeds 1–3, and save/load, keeper access, containment and visitor visibility pass in
both plan seeds. Real seed-1/tick-3000 Elephant Valley and Rhino Hollow views are inspected in
`evidence/dry-moats-j18-2026-11-20.jpg`.
All 270 active CTest cases pass; 21 measurement and diagnostic cases remain intentionally disabled.

**Meerkat Lookout** is a sand mound (a 0.7 m rise, the sentries' place) with a two-tile drinking
dish at the builder's minimum depth instead of a pool, one outcrop, and scrub along the boundary.
J29 replaces exposed wire mesh on the service sides with a 1.8 m clipped wildlife hedge. Its
welded mesh and buried edge are mostly hidden by leaves but remain in the procedural model; the
catalog gives it strength 2, so the habitat retains containment 1.0 against the meerkat's 1.5 m /
strength-1 requirement. The glass public front, 252 m² area, five residents, 28 viewing tiles and
all furnishings remain. Both plan seeds pass the dedicated barrier regression, short hashes stay
unchanged, and the rebaselined 6,000-step hashes repeat twice for seeds 1–3. The public and service
views were opened and are recorded in `evidence/wildlife-hedge-j29-2026-11-20.jpg`.

**Flamingo Lagoon** has a deep heart (the nearest eighth of its water tiles, 1.4 m) and a 0.3 m
shelf round it: 74 of its 103 water tiles are under 0.45 m. A dry bank stays between the water
and every fence: digging at the back fence lowered vertices shared with the boardwalk and left
a wheelchair-steep edge in its paving (`WalksAreWideAndLevel` allows 5 %; the steepest paved
tile is 2.8 % in seed 1, 2.7 % in seed 2). The same test counts path widths: no ordinary paved
tile remains on a walk only one tile, 2 m, wide, down from 48; most paving is 6 m and wider.
Facility approaches are laid 4 m wide only after every building has chosen its site, so widening
does not push a later kiosk away or lengthen access (the farthest facility walk is 75.1 m, under
the 85 m limit). Paved shade-tree islands keep a two-tile ring, dead raster teeth are removed, and
a one-tile run gains a parallel row only when the whole row is unobstructed public lawn. J30
closes the last three exceptions: the food-court ellipse grows one tile west, the public rail
recesses around one outer planting tile between Elephant Valley and Kangaroo Grove to add a lane,
and a lone diagonal path tile is removed from Ostrich Rise's planted stand-off. A one-tile planted
buffer remains before both containment fences. `WalksAreWideAndLevel` now requires zero ordinary
2 m remnants in both seeds; `ElephantKangarooWalkKeepsItsPlantedBuffersAndRails` separately fixes
the lane, two adjacent planted tiles and their two rails in the plan contract. Kangaroo Grove
retains its original position, 64 viewing tiles, all fourteen habitat areas and resident groups
are unchanged, and the before/after plan plus opened kangaroo and ostrich views are in
`evidence/wide-walks-j30-2026-11-20.jpg`.
Flamingos stand and feed in the shallows (`FlamingosUseTheShallowShelfDuringAVisit`; the stall
probe counted 11 of 12 in the shallows after 6,000 ticks; seen in the game).

**Furnishing.** A prop's whole footprint has to lie on dry, unbuilt ground of its habitat (the
builder asks only about the tile under its centre, so rocks stood half in pools and feeders
leaned into shelter walls). Feeders and troughs stand in the front half, where the benches look
at them, on a fan instead of a row, two tiles and more inside the barrier (one on the mound and
beside the lagoon); the front half is searched first, because in the lagoon the nearest dry
place lay behind the middle. They are counted from the family's mouths, a suckling half.
Rocks crop out together on the rise and at the pool's rim. Toys are taken in turn from the
species' own list until the group's playfulness is met. The shelter's door apron and the ground
inside each keeper gate are kept clear.

**Room to walk.** Blocking props keep the largest resident's collision width (1.2 × body radius)
plus 1 m apart, so that no pocket forms that an animal walks into and not out of
(`BlockingPropsLeaveAnAdultWidthPlusOneMetre`). `ClearPlace` also rejects a routine destination
unless an animal has two body-wide, prop-free ways out of it. The seed-1 stall probe of
`docs/ANIMAL_BEHAVIOUR.md` now counts 1,603 stalled of 303,845 moving ticks (0.528 %; 1.49 %
before pocket rejection, 5.4 % on the old plan), with no continuous stall reaching 120 ticks.
An animal on an ordinary errand that makes no progress tries a sidestep after 0.75 seconds. This
is enforced by `AnimalMovementTests.VisitorZooAnimalsDoNotRemainStalledInPropPockets`.

**Planting.** Plants stand in groves with glades between them (smooth noise some eight metres
across), towards the back and the rim, in passes 3.6, 2.6 and 2.0 m apart, never in the lines
from the three benches to the habitat's middle (3 m to either side, 2 m on the mound), never
within 5 m of any viewing place. Every trunk also keeps the planted strip's depth farther back:
a slender acacia a metre inside Elephant Valley's cable still put its crown in the view. J31
removes the former Rhino Hollow and Red Dune exception: every slender trunk now keeps this full
distance too. If that leaves cover close to the welfare band, remaining suitable beds receive the
style's non-blocking low layer first. Rhino Hollow uses grass and savanna scrub; steppe grass joins
desert scrub in Red Dune while its palms remain behind the open crown line. The measured cover is
0.140 and 0.125 respectively, above the species figure less 0.13 guard (0.120). An ordinary land
plant's whole circle also keeps off a water tile and the
eight surrounding tiles whose shared corners the pool bed lowers; ferns no longer stand in the
flamingo lagoon. Twelve non-blocking reed stands are the deliberate marginal exception: their
whole circles remain on dry ground, but they root on the lowered bank around the lagoon, away
from the three bench sight lines. Non-blocking tall-grass tufts now form an herb layer in the
open savanna and grassland habitats. Each habitat has a mix of kinds (Bear Falls: pine, spruce, oak,
birch, juniper instead of a block of pines two metres apart). The aim is the species'
vegetation figure less 0.08 (0.12 above 0.4), which keeps views open; falling short is reported
as a problem only below the welfare band (figure − 0.15, `Animals.cpp`). Lawn trees in the
public grounds stand in groves as well (they stood on every third tile, an orchard), and a
broad crown is planted beside each viewing bench, on an island left in a paved bay where the
paving stays all round it. The lamp stands at the end of the bench, not before it.

`NoPlantOnAPathOrBeforeABench` applies the full tree set-back to every habitat and both seeds;
`HabitatCoverKeepsAMarginInsideTheWelfareBand` separately preserves the 0.02 margin within the
welfare tolerance. The inspected Rhino Hollow and Red Dune views are in
`evidence/open-habitat-fronts-j31-2026-11-20.jpg`.

**Keeper gates.** Where a habitat has a stand-off strip, a dry stone service apron two tiles
wide runs through the strip from the back walk to the keeper gate and is railed off from the
planting, so that no keeper walks through planting and the lead's "remove the planting before
the gate" is not needed (`EveryKeeperGateHasADryConnectedApproach`: the tile outside each gate
is walkable, not stand-off, not water, and joined to the entrance).
The rail is derived from the completed regions rather than only the expanded habitat outline:
every edge where public ground (including an apron) meets a planted strip is closed. This matters
at the irregular north corner of Red Dune, where one internal apron-to-strip edge used to be open.
The core checks the invariant for both plan seeds, and
`Walker.NoHabitatAndNoPlantedStripIsEnteredFromAnySide` physically runs at all 1,498 public
barrier edges. The repaired service side is shown in
`docs/evidence/red-dune-service-rail-2026-11-19.jpg`.

**Other checks:** `NothingStandsInWaterOrInAnotherThing`, `NoLandPlantInOrBesideWater`,
`LagoonHasReedsAndOpenHabitatsHaveTallGrass`,
`NoPlantOnAPathOrBeforeABench`,
`BenchesStandInShade`, `HoofstockStandsBehindCablesAndRails`, `FoodAndWaterStandInsideAndInView`,
`MeerkatsHaveADishAndTheLagoonIsShallow`, `TreesStandApartInGroves`,
`FamiliesWithTheirYoungOnTheOpeningDay`, `FamiliesAreWholeWhenTheVisitBegins`.
`VisitorZooPlanTests.DISABLED_WritePlanImage` writes the plan from above as a picture
(`ZOO_PLAN_IMAGE`, otherwise `build-probe/plan.ppm`).

Golden hashes of `tests/core/VisitStartTests.cpp`, recorded again for the plan:

| Fill ticks | Seed | At fill | Later |
|---:|---:|---:|---:|
| 600 | 1 | 594257938472033448 | 14337973029496189051 |
| 600 | 2 | 15406606244450351165 | 1215235191448489462 |
| 600 | 3 | 6169501891789178616 | 12609309977693402985 |
| 6000 | 1 | 9176597010523512527 | 11178446725631306708 |
| 6000 | 2 | 10900274889816509035 | 9192663283298467455 |
| 6000 | 3 | 9886221438050585804 | 15338513518932663782 |

## The zoo as the game visits it

The game builds this scenario without any species that has no model in `assets/animals/`
(`withoutSpecies`); such a habitat would stand finished but empty. All eighteen current species
have production models, so a seed-1 visit has all **99 animals in seventeen habitats**.

The ice-cream kiosk formerly in the narrow Elephant Valley/Kangaroo gap is relocated
near entrance coordinates `(-5,18)`, facing north, to keep refreshment access around the wider
walks. Four explicit planting tiles at `(14,27)`, `(20,28)`, `(21,29)` and `(22,30)` close the
southern stand-off spur and diagonal slivers between Ostrich Rise and Flamingo Lagoon; the
lagoon's containment is preserved.

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

The strip's arm's-length shrubs now use smaller leaf sprays and seven visible stems while their
far forms retain the old lean geometry. The scripted `--stand` chooser no longer mistakes a
keeper gate or a convenient service-mesh side for the public front:
glass and moat edges receive a strong preference, rail and cable a smaller one, and end panels
are devalued. These are deliberately soft preferences, so a tree-obscured glass panel may still
lose to an unobstructed animal rather than producing a formally correct but useless photograph.
Its sight-line test also uses a tree's existing shade/crown footprint rather than only the narrow
ground-collision trunk. At Bear Falls seed 1, the old ray missed a spruce trunk by 1.15 m and so
accepted `(164.8,57.0)`, although the 2.15 m crown filled the frame; it now selects the clear
water-edge position `(154.8,45.0)`. Lion Rock, Meerkat Lookout, Polar Sound and Acacia Plain
remain useful public-side controls. The before/after and four controls were opened and checked in
`evidence/clear-view-crowns-j48-2026-11-20.jpg`; two focused sight-radius regressions and all 25
game-side tests pass (J48, 2026-11-20).
