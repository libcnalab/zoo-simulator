# Plants: what they are made of, how far ones are drawn, what they cost

Every plant of the zoo is a prop of the catalog (`data/construction/props.json`, `look`), placed
by the zoo's plan (`src/core/zoo/sim/VisitorZoo.cpp`). The game writes its model in
`src/game/PlantModels.cpp` (`BuildPlant`; the clipped hedge, a fence style, is `BuildHedge`),
paints the cut-out pictures in `src/game/FoliageTextures.cpp`, and draws plants apart from the
rest of the scenery in groves (`src/game/SceneryRenderer.cpp`), each grove in a near and a far
form (`PlantDetail`), the far one in the two materials of `src/game/PlantPalette.cpp`.

## What each kind is made of

Cut-out cards cast no shadow in the engine (its shadow pass draws only opaque items), so every
plant has an opaque part of about its silhouette that the sun casts from: the "heart" of a
crown, the wood, a strip under every frond.

| Kind (`look`) | Model |
|---|---|
| oak | short thick trunk with a flared foot and five roots into the ground, six or seven scaffold limbs that rise and then reach out almost level, a crown wider than high of sprays over dark hearts |
| birch | slender white trunk to the top (paper bark with short dark lenticels and a few dark scars, `Pattern::BirchBark`), nine to eleven thin ascending branches, small airy masses of leaves with twig ends hanging under them |
| willow | stout short trunk, a low dome, and 46 curtains of long narrow leaves hanging almost to the ground (two cards one under the other); the curtains have no heart, the dome casts the shadow |
| acacia | leaning stem forking into four or five limbs, each carrying a thin flat table of leaves, all at one height |
| baobab | bottle trunk with a belly, six to eight short thick limbs with tufts at their ends |
| spruce | cone to the ground: seven to nine whorls of drooping boughs (every other one on edge so some face every onlooker), short steep boughs between them near the stem, small opaque hearts per whorl, a leader |
| pine (Scots) | bare, slightly crooked stem, grey below and fox-red above, a flat crown of five to seven needle pads on crooked limbs, dead stubs |
| palm (feather palm) | ringed stem (the frond scars as steps, swollen foot), a fibrous crown shaft, 19 to 23 fronds from the upright young ones to the hanging old ones: each frond is a rachis with leaflets painted on two rows of cards along an arching line, an opaque strip under it; two to four dead brown fronds hang down the stem |
| bamboo | 13 to 16 jointed canes (swollen nodes as high as one can see them), close at the foot, leaning apart and nodding over at the top, fans of narrow leaves on twigs from the upper nodes; solid blades among the leaves dapple the ground |
| fern | a shuttlecock of 15 to 19 twice-divided fronds, the young ones upright, the old ones arching out, over a low dark heart |
| shrub, scrub | two to five masses of fine twig-end sprays over hearts, seven visible stems from the root, and a few tufts of dry grass at the foot (what grazers leave under a thorny bush) |
| juniper | three or four tall masses of needle sprays |
| flower bed | a stone kerb, soil, a cushion of leaves, 30 clumps of flower heads (daisies and umbels, the colour from the material) leaning every way, in patches of one colour |
| hedge (fence style) | a dark core box a hand inside the leaves, coated with leaf sprays lying against both faces and the top like tiles, the top ones standing a little proud |
| reeds, grass | tufts of crossed cards over a low heart; catalogued as walk-through `wetland_reeds` and `tall_grass` |

Every individual differs: height, girth, lean, the number of limbs, whorls, fronds and canes,
where each goes, are drawn from the prop's seed. Trunks and limbs are one smooth skin through
several points (`MeshWriter::Pipe`) instead of tubes laid end to end.

## Reeds and grass in the visitor zoo (J8, 2026-11-20)

The two existing procedural looks now have catalog props. Both are non-blocking undergrowth:
animals can walk through stems and blades instead of treating every tuft as a rock. The open
savanna and grassland planting mixes use `tall_grass`; because it contributes only 2.5 m2 of
cover per tuft, it makes a visible herb layer between the much larger acacias and shrubs rather
than replacing their crowns.

Flamingo Lagoon receives twelve deterministic `wetland_reeds` stands. They use dry tiles whose
corners the adjacent pool bed lowers, which ordinary land plants still reject. Placement checks
the whole scaled catalog circle against actual water, buildings, other props and the enclosure,
keeps the shelter and keeper approaches clear, and excludes the three bench-to-lagoon sight
lines. `VisitorZooPlanTests` verifies this for seeds 1 and 2. The J8 seed-1 visit built 2,689
props and submitted 2,201,274 scenery triangles in 461 draw calls; the inspected lagoon and close
savanna views are in `evidence/reeds-grass-j8-2026-11-20.jpg`.

## Fallen leaves under broadleaf trees (J9, 2026-11-20)

The near model of every oak, birch, willow, acacia and baobab now scatters 32 deterministic dry
leaves around its roots. Each is a gently folded 18-34 cm quad with a mottled dry-leaf finish;
the scatter reaches 1.15-2.4 m according to the catalog radius. It belongs to the procedural
tree rather than the terrain painter, so it follows the individual tree and needs no map-specific
placement. The far model consumes the same random values but omits the sub-pixel leaves.

Seed 1 has exactly 500 affected trees. The litter therefore adds 32,000 near triangles to the J8
baseline (2,201,274 -> 2,233,274 scenery triangles) and one dry-leaf finish to each of the 16
visible near groves (461 -> 477 draw calls); the all-far tally is unchanged. The normal Tiger
Forest view and a close crop are in `evidence/leaf-litter-j9-2026-11-20.jpg`. The game-side
`PlantModels` regression checks all five broadleaf looks at 64 litter triangles each, a zero-triangle
far litter part, and a pine without litter.

## Fine shrubs at visitor distance (J33, 2026-11-20)

The near shrub and scrub forms use leaf-spray cards about one fifth smaller than the old cards,
with seven rather than four stems visible through the foliage. Their lower coverage keeps this a
silhouette refinement rather than an indiscriminate geometry increase. The far form retains the
old larger four-spray cards and four stems exactly: the detail is for an arm's-length planted
strip, not for a bush fifty metres away.

An initial finer setting looked good but raised the real visitor zoo's scenery from 2,258,980 to
2,708,192 triangles (+19.9%); it was rejected. The retained setting reports 2,364,188 triangles
(+105,208, 4.66%) in the same Lion Rock run and the same 472 draw calls. A seed-1 savanna shrub
is 552 triangles rather than 500, while its far form remains 128. The `PlantModels` regression
requires at least 280 near leaf-spray triangles, fewer than 160 far ones, a greater-than-fourfold
near/far spray ratio, and a sub-900-triangle near model. The opened close crop is
`evidence/fine-shrubs-j33-2026-11-20.jpg`.

## Distance form

`BuildPlant(..., PlantDetail::Far)` writes the same individual with the same random numbers in
the same order (nothing may draw a number only when near), and decides afterwards what to leave
out: one spray card in four, twice the size and carrying all four sprays of its picture (the
same leaves at the same size cover the same area); hearts, trunks and limbs with fewer sides;
limbs under 7 cm in radius, roots and twigs left out; a palm's rings as four pieces; every other
fern frond and grass tuft, one bamboo fan and one flower clump in three (each larger). A far plant has about a quarter of the
triangles of the near one (savanna shrub 552 -> 128, acacia 1,622 -> 460, oak 2,184 -> 614,
palm 3,036 -> 672).

The far form of a grove is merged into **two meshes** whatever grows in it: the opaque parts in
one material, the cut-outs in another, both reading one 1024 x 1024 picture (`PlantPalette`):
every solid finish as a swatch of its colour times its pattern's mean brightness (a pattern is
too small to make out that far), every card picture as a 128-texel miniature already tinted with
its finish's colour, its alpha raised so that as much of it passes the alpha test as of the
full-size picture.

Groves are squares of 40 tiles (80 m; 16 for the visitor zoo; in the editor 32 tiles, the
cells' size, so that an edit rebuilds little). A grove is drawn far when all of it is farther
than 56 m from the eye and near again when some of it is nearer than 48 m (hysteresis, so pacing
does not flip it). At 50 m one pixel is about 7 cm.

`ZOO_PLANTS_FULL_DETAIL=1` in the environment draws every grove near: for comparing the two
forms from one viewpoint. It does not build the unused far batches, so the comparison no longer
pays for a second plant zoo that it never submits.

### Does it pop?

A scripted walk (`--view 159,220,180,-5 --script` holding W, `--lockstep`, a shot every fourth
frame) crosses the switch of the grove row 48 to 56 m ahead; the same walk with everything near
gives a reference for every frame. Before the switch the far grove differs from the near one in
scattered pixels of its crowns (slightly thinner leaves); after it the difference to the
reference drops (RMSE 3,150 -> 2,690, the rest is moving people and the farther groves). Side by
side, the frame just before the switch and the all-near reference of the same frame cannot be
told apart by eye (the crops are in the task's scratch output, not in the repository). Forced close (all groves far at 10 to 30 m) the far
form shows its flat, coarse hearts as smooth dark blobs between the cards: it is not meant to be
seen there.

## Far-away quality of cut-outs

All three card textures (leaf sprays, needle boughs, the plant cards) and the far palette keep
their alpha-test coverage through the mip levels (`MipOptions::alphaTestReference`, engine;
the palette's miniatures by their own bisection), so crowns do not thin out with distance. The
edges are still hard (aliased) far away: the engine has no alpha-to-coverage and the game runs
without MSAA (open; not judged in motion).

## What it costs

Scenery on develop before this work: 1,904,657 triangles in 134 draw calls (G8's plan has 2,507
props, of them 941 savanna shrubs, 241 acacias, 145 scrub, 134 birches, 129 junipers, 81
willows...). After: the plants in full detail are 1,877,784 triangles (the zoo's scenery
2,272,893 in all, 441 draw calls if everything were near: about 20 finishes a grove), all of
them far 507,452 triangles in 32 draw calls. Per plant, near / far: savanna shrub 500 / 128,
acacia 1,622 / 460, baobab 3,323 / 982, birch 1,352 / 332, willow 1,790 / 602, oak 2,184 / 614,
palm 3,036 / 672, pine 1,144 / 284, spruce 1,096 / 288, bamboo 3,548 / 520, juniper 378 / 100,
fern 1,334 / 272, flower bed 344 / 132 (`--bench` prints this tally).

`--bench --frames 300` at the entrance (800 visitors), Release, both binaries from the same
tree, interleaved, each run alone under the shared CPU lock, other agents' games and builds
running (load 2.6 to 4.1):

| run | before fps (frame ms) | after fps (frame ms) |
|---|---|---|
| 1 | 68.8 (14.5) | 73.0 (13.7) |
| 2 | 63.4 (15.8) | 74.8 (13.4) |
| 3 | 70.3 (14.2) | 78.6 (12.7) |

Triangles a frame through the engine's passes 4.67 million -> 3.29 million, draw calls 478 ->
516, resident memory 2,190 -> 2,275 MB (the grove's second form and the palette). Every pair
is faster, by 6 to 18 %; on this shared GPU single runs move by 10 % and more, so read it as
"about 1 ms a frame faster", not as an exact figure. Without any scenery the same view runs at
147 fps (6.8 ms): the scenery still costs some 7 ms here, and drawing every grove far only
brings 3.11 million triangles and about 71 fps, so what is left is fill (alpha-tested cards over
one another) as much as triangles.

Re-measured 2026-11-19 after static recovery copies were removed: the normal loading marks rose
from 875 MB after ground to 1,244 MB after scenery. Full-detail mode with its unused far batches
skipped rose from 874 to 1,169 MB. The dual far batches therefore account for about **74-75 MB**.
They remain in the normal visit because the measured benefit above is about 1 ms per frame; a
streaming replacement would need to rebuild procedural plants without causing a visible hitch.

## Open habitat fronts (J31, 2026-11-20)

Every habitat tree now stands at least five metres plus the planted strip's depth from every
viewing tile. Rhino Hollow and Red Dune formerly allowed slender acacias or palms inside that full
line because a second tree-only planting attempt otherwise crossed the vegetation welfare band.
The builder now fills free beds with the habitat style's non-blocking low layer before it considers
that fallback. Rhino Hollow retains 0.140 cover with grass and savanna scrub; Red Dune gains steppe
grass and retains 0.125 cover, above the 0.120 guarded minimum, while its palms move behind the
open crown line. The seed-1 zoo has 47 more low plants (2,752 props total); the real views are in
`evidence/open-habitat-fronts-j31-2026-11-20.jpg`.

How the grove size was chosen (the same binary with the size from the environment; frames a
second, runs of different rounds): 80 tiles offset by half a grove (the earlier run's choice)
53.0, 52.1, the slowest every time; 20 tiles 59.7, 66.0 (632 draw calls); 32 tiles 56.0, 77.5,
57.4, 77.6, 73.7; 40 tiles 72.1, 76.1, 64.3, 64.3, 77.1, 75.2; before 60.1, 75.6, 77.4, 67.4,
66.4, 67.2, 71.8.

## Open

- Cut-out edges far away are aliased (no alpha-to-coverage, no MSAA).
- The far form forced close shows flat hearts; at the switching distance it does not.
- Plants are fill-bound as much as triangle-bound: the far form cuts the triangles a frame by
  1.4 million but buys about 1 ms; overlapping alpha-tested cards remain the larger cost.
- The hedge is in no habitat of the visitor zoo (fence kind "hedge" exists, the plan does not
  use it); it was looked at as a specimen placed by a probe build.
