# The ground and the water's edge

What the ground of Greenhill Zoo is drawn from, how the banks of its pools are shaped, the
height lookup everything standing on the ground uses, what it costs, and what is open.
Tasks G1 (2026-11-13), G1b and G1c (2026-11-14). Code: `src/game/GroundPaint.{hpp,cpp}`
(colours and heights), `src/game/GroundDetail.{hpp,cpp}` (the detail layer's textures),
`src/game/TerrainRenderer.{hpp,cpp}` (meshes, textures, drawing). Evidence:
`docs/evidence/ground-g1-2026-11-14.jpg`.

## What the ground is made of

- **Baked colour.** Every chunk of 16 x 16 tiles has its own texture, 32 texels a tile
  (6.25 cm a texel), painted at start-up from the world's tiles: lawn in mown and rough
  tones, dry savanna grass, sand, dry earth, forest floor, rock, mud; bound gravel, asphalt
  with a flush concrete kerb, paving slabs (`Cobblestone` is laid as 0.5 m slabs in a running
  bond with a border course of setts), boardwalk planks; building foundations; bark mulch on
  planted stand-offs; ground worn bare along fences and wider at gates, a soil verge along
  paths, stones kicked off gravel paths; bare rock where the ground is steep.
  The editor retains the CPU image and tile signatures so it can repaint changed chunks.
  A read-only visitor zoo discards that 100 MB edit cache after upload; animated water keeps
  updating, while a later load builds a fresh renderer and cache before discarding it again.
- **Soft, irregular borders.** Kinds of ground and paths are spread over the tiles with a
  quadratic B-spline (a field that is smooth across tile sides), then pushed about by noise;
  the winner takes the colour over a narrow soft edge. No border follows a tile's side.
- **Rock on steep ground** follows the slope at the tile corners (the steeper of the two
  tiles on either side, interpolated), so it fades out across the next tile instead of
  stopping at a tile's side (until G1c it was the tile's own slope, and on the banks of the
  deep pools it ended in a straight line along the tiles: 85 tile sides with a jump of up to
  0.61 in linear colour on the real map).
- **Detail layer.** The baked colour cannot carry a stone or a blade of grass, so an
  alpha-blended layer lies over the ground near the eye: three 1024 x 1024 textures over 4 m
  (grain, blades, slabs; 3.9 mm a texel), tinted per kind of ground (`GroundDetail`), one
  welded mesh per kind and chunk, cut along 25 cm cells. Its alpha fades out down the mip
  levels and is gone at level 4, where its texels are as coarse as the baked colour's, so
  it never shimmers far away and needs no distance fade; it is not drawn beyond the
  distance where that happens (53 m at 720 lines, 160 m at most). It has its own render
  pass ("ground detail", `ZooGame::LoadGame`) before the visitors' pass, so the people's
  shade lies on it, and it receives shadows and casts none.
- **Water.** A flat sheet over every water tile (only those since G1c), with ripples
  (`Surfaces`) and 62 % opacity, so the painted bed shows: pale in the shallows, dark in the
  deep, damp mud on the land along the water.

## The banks of a pool

`World::SetWater` digs all four corners of a water tile down to the pool's bed. Those
corners are shared with the dry tiles around the pool, so in the world every dry tile at a
pool slopes down to the bed and much of it lies below the water. Drawn that way (as up to
G1b), dry land around every pool was flooded: the water sheet reached the barrier at
Penguin Cove, the meerkats' two-tile dish was a 6 x 4 m pond with its termite mound
standing in it, and animals the simulation keeps on dry land walked on the drawn bed under
the water. Measured on the real map over 6,000 ticks after the visit's fill (a sample every
10 ticks, animals on dry tiles only), standing on `World::HeightAt`: penguins more than 90 %
of their shoulder height under water in 1,932 of 6,465 samples, brown bears in 65 of 951,
polar bears 12 of 797, tigers 11 of 1,200; meerkats more than 40 % (chest-deep) in 109 of
3,000, wolves in 132 of 3,499.

Since G1c the drawn ground keeps dry land dry, without touching the simulation:

- **Rim.** A vertex that both water and dry land touch is drawn `kBankLipM` (5 cm) above the
  water (`DrawnVertexHeight`). Dry tiles are drawn as before, over these corners: from the
  land down to the water's edge, above the water.
- **Bank tiles.** A water tile with dry land on a side or at a corner is cut into 8 x 8
  cells of 25 cm. Its ground stays at the rim near the dry land, then falls to the world's
  bed over `kBankWidthM` (0.9 m), beginning anywhere between 0 and `kBankWanderM` (0.9 m)
  from the dry land, along two octaves of noise: the waterline wanders inside the water
  tile instead of following the tiles. Where dry land lies along two sides of the tile that
  meet, the water's corner is rounded off (1 m). Land corners that reach into the water get
  round banks from the distance to that corner.
- **No seams.** Everything along a tile's side is computed from what both tiles at that side
  know (the side's corners; the dry sides and corners at its ends, which is why the wander
  and the fall together stay under a tile's width); the fall uses `std::lerp`, exact at both
  ends. Tiles beside a bank tile split their sides towards it with fans in their own planes,
  so there is no T-junction and no crack between chunks. This also cured G1's pond-corner
  defect (a light wedge over the wet rim where shore cells were folded into fans).
- **Damp ground** on the land within about 25 cm above the water and a metre or so from it,
  along a ragged line that reaches zero at the far side of the tile.

After the change, the same measurement finds no animal on dry land standing in drawn water at
all (0 samples over 10 % of the shoulder height, all 15 species).

What it costs in realism: the drawn banks are steep (up to 1.4 m over 0.9 m at the deep
pools of the bears, the penguins and the tigers), more like the rock or concrete edges real
pools of those animals have than like a natural pond; the dry ground beside a pool is never
under water, so the land is at most 5 cm above the water at the edge (no low, flooded
margins); a few dry tiles whose own land lies below the water (Flamingo Lagoon 4, Ostrich
Rise 2) are still flooded by up to 3.4 cm.

## Drawn height: `DrawnGroundHeight`

```cpp
#include "GroundPaint.hpp"
float zoogame::DrawnGroundHeight(const zoo::World& world, zoo::Vec2 at);
float zoogame::DrawnVertexHeight(const zoo::World& world, int vx, int vz);  // a grid vertex
```

The height of the ground as it is drawn, on the very triangles of the mesh (on bank tiles it
interpolates the 25 cm cells, as the mesh does). Away from water it is `World::HeightAt`; at a
pool it differs from that by up to the pool's depth. It reads the world only (nine tiles'
water, their corners), allocates nothing, keeps no cache and is safe on any thread. Measured
in a probe on the real map: 50-66 ns a call at random points, 180-270 ns on bank tiles (the
machine was loaded); against the mesh, 470,320 sample points agree within 0.11 mm.

Users: `AnimalRenderer` (the ground under and in front of and behind each animal; a
swimmer's surface stays the world's water level), `VisitorRenderer` (feet), `Walker` (the
player's collision field at tile resolution from `DrawnVertexHeight`, and the start
position). Still at `World::HeightAt`: props in `SceneryRenderer` (84 of 2,508 props stand
where the two differ by more than 3 cm, up to 1.45 m: trees, rocks, log piles, ice blocks at
the pools; a two-line change, see the G1c report), keepers (`KeeperRenderer`, not yet merged),
sounds, the binoculars' aim, the editor's tools. The simulation itself keeps its heights.

## Start-up

Painting, mip levels and meshes of every chunk, and the three detail textures, are jobs taken
from an atomic counter by up to 6 threads (one in the browser build); the main thread hands
finished chunks to the GPU as they arrive. Each job reads the world and writes only its own
output, so the result does not depend on the number of threads. The log prints
`zoo: ground painted in X s; built in Y s`: X is the chunks (painting, mips, meshes, upload),
Y the whole `Build` (with the detail textures and the land beyond the map). Before G1 the
line timed the painting alone; the mips, meshes and uploads followed untimed.

## Measurements (2026-11-14, G1c)

Two Release binaries from the same tree: "before" is develop `0b13f7c` (the G1/G1b/G1c diff
reversed), "after" is the G1c tree with the two-line `SceneryRenderer` change applied (it
does not change what is drawn at the entrance). Interleaved, each run alone under the shared
CPU lock, load average 2 to 4 (another agent's tests ran in between, not during). The bench
is `--bench --frames 300` at the entrance (259 frames measured).

| run | load | frame mean ms (fps) | p95 ms | draw calls | triangles | resident |
|---|---|---|---|---|---|---|
| before 1 | 2.56 | 13.45 (74.3) | 15.86 | 479 | 4,671,890 | 2,198 MB |
| after 1 | 2.45 | 13.52 (73.9) | 16.29 | 502 | 4,776,400 | 2,264 MB |
| before 2 | 2.31 | 12.78 (78.2) | 15.14 | 479 | 4,672,555 | 2,198 MB |
| after 2 | 2.82 | 13.26 (75.4) | 16.66 | 502 | 4,775,972 | 2,259 MB |
| before 3 | 2.56 | 14.57 (68.6) | 17.85 | 478 | 4,668,754 | 2,199 MB |
| after 3 | 2.79 | 15.21 (65.7) | 18.88 | 502 | 4,772,646 | 2,257 MB |

(Draw calls, triangles and memory come from a first series at load 2 to 4 whose fps lines were
garbled in the log; the frame times are from a second series right after it.)

- **Frame:** after is slower in every pair, by 0.07, 0.48 and 0.64 ms (0.5 to 4 %): the
  detail layer's 23 draw calls and about 104,000 triangles. `--without terrain` (one run
  each): 87.8 fps before, 88.9 after, 335 draw calls both, so the layer is the whole
  difference and the rest of the ground costs nothing new. The ground as a whole costs about
  2 ms a frame at the entrance (74 against 88 fps).
- **Memory:** about +60 MB resident (2,198 against 2,257 to 2,264 MB): the three detail
  textures with their mip levels and the detail meshes.
- **Start-up** (`--frames 2`, wall clock of the whole run): before 9.88 / 8.88 / 8.68 s,
  after 8.90 / 8.22 / 8.07 s, at load 3.1 to 3.9. The ground lines: before "painted"
  2.15 / 1.86 / 1.82 s (painting only), after "painted" 1.91 / 1.56 / 1.57 s and "built"
  2.01 / 1.62 / 1.63 s (everything). The whole start is 0.6 to 1.0 s shorter although the
  ground now does more (mips and meshes on the workers, detail textures).

## Checks

`build-probe/ground-check-zoo.cpp` in the G1c working tree (not part of the build; it includes
`TerrainRenderer.cpp` and links the game's objects) builds the Visitor Zoo and checks, on the
real map: every vertex two chunks share has the same height and normal to the bit; every
interior edge of the ground mesh is shared by exactly two triangles (no T-junction); the
lookup agrees with the mesh; the paint's colour along every tile side as both tiles paint
it; the animals' drawn water depth over 6,000 ticks. Seams of paint that remain are at
paths, buildings, fences and mulch, not at the water (see "Open").

## Open

- Paint seams on land: the bare strip along a fence ends square where a fence ends or turns
  and is wider along a gate than beside it (up to 0.22 in linear colour across a tile side);
  building foundations end at the tile (intentional so far).
- Detail layer outlines are 25 cm cells: the edge between mud and grass is stepped close up.
- Props at the water stand at `World::HeightAt` until `SceneryRenderer` uses
  `DrawnGroundHeight`.
- Shallow against deep water: the bed's colour changes with depth, but the banks are steep,
  so the pale shallow band is narrow.
- Leaf litter under trees: the painter sees tiles, not where trees stand.
- Not verified: shimmer and repetition in motion (stills only), resolutions above 1280 x 720,
  the editor's repaint after an edit (`TerrainRenderer::Update`), the browser build.
