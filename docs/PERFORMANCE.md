# Performance: what was measured, what was changed

The initial frame-rate measurements used the development machine's graphics hardware:
AMD Ryzen 7 PRO 7840U with the integrated Radeon 780M (Mesa, OpenGL through EGL), 1280x720,
without a window (`SDL_VIDEODRIVER=offscreen` renders on the GPU's render node, it is not a
software rasteriser) and without sound output (`SDL_AUDIODRIVER=dummy`). `tools/bench/bench.sh`
runs the matrix. Each figure in that matrix is one run of `./zoo-simulator --bench --frames 300`, the default
visit: 800 visitors, 74 animals, 10:30 in the morning. Nobody watched these runs on a monitor;
the pictures named below are screenshots the game wrote itself. Later memory measurements
identify the software-GL environment in their respective sections.

The machine is shared with other compile jobs, so single runs differ by 10-30 % between
repetitions. The first run of the second matrix ("entrance", 53.5 fps) ran while tests were being
compiled; the same configuration measured twenty minutes later ("avatars-40") gave 75.2 fps.
Compare groups, not single digits.

## Where the time went (2026-11-13, before any change)

| Place | fps | frame ms | people pass ms | engine and driver ms |
|---|---|---|---|---|
| entrance plaza | 33.5 | 29.9 | 19.5 | 8.1 |
| lions | 80.5 | 12.4 | 8.2 | 2.7 |
| penguins | 89.2 | 11.2 | | |
| elephants | 33.4 | 29.9 | | |
| giraffes | 34.9 | 28.7 | | |
| meerkats | 32.6 | 30.7 | | |
| flamingos | 31.8 | 31.4 | | |

"People pass" is posing and drawing the CNA Avatar visitors (each is a 15-part skinned figure
drawn with its own effect; about 0.17-0.19 ms a person). The game drew the nearest 120 in view
and nobody else. Leaving parts out at the entrance showed the order of the costs:

| Left out | fps | what it shows |
|---|---|---|
| nothing | 33.5 | |
| people | 112.1 | two thirds of the frame was the avatars |
| scenery | 42.6 | engine and driver 8.1 -> 1.7 ms: the scenery is the second cost |
| shadows | 34.9 | about 2 ms |
| animals | 32.5 | 1.19 million of the 2.36 million triangles, but under 1 ms |
| terrain | 33.3 | negligible |
| simulation | 31.5 | negligible (0.9 ms) |

Avatars alone, at the entrance (`--without flat --people N`, measured again after the change):

| Avatars drawn | 1 | 30 | 60 | 120 | 250 | 400 |
|---|---|---|---|---|---|---|
| fps | 88.4 | 63.6 | 40.5 | 29.3 | 15.5 | 11.1 |
| people pass ms | 0.35 | 6.0 | 12.9 | 22.5 | 51.0 | 76.3 |

Memory: 864 MB resident with one avatar drawn, 1.0-1.1 GB with the whole cast loaded.

Loading the visit took 10-11 s: building the zoo 0.6 s, **6,000 warm-up steps of the simulation
7-9 s** (so that the visitors are spread over the zoo when the gates open for the player),
painting the ground texture 1.4-2.3 s.

## What was changed: people near and far

The nearest people (within 16 m, at most 40; since the same day at most 18, see below) are full
CNA avatars, as before. Everybody else in
view is drawn as a flat, upright picture of the same person: for each member of the cast the game
renders, once, a sheet of that avatar in ten poses (standing, sitting, eight phases of the walk)
from eight directions, and far people are quads with the matching cell of that sheet, lit by the
scene's sun and ambient colour, all of one cast member in one draw call. A soft dark patch under
every person replaces the missing ground contact. The switch has a hysteresis (16 m in, 20 m out)
so nobody flickers between the two at the border. `--without flat` gives the old behaviour.

| Place | before fps | after fps | people pass ms before -> after |
|---|---|---|---|
| entrance plaza | 33.5 | 75.2 (53.5 under load) | 19.5 -> 3.5 |
| lions | 80.5 | 142.7 | 8.2 -> 0.4 |
| penguins | 89.2 | 134.2 | |
| elephants | 33.4 | 115.8 | |
| giraffes | 34.9 | 120.7 | |
| meerkats | 32.6 | 58.1 | -> 5.4 |
| flamingos | 31.8 | 68.5 | -> 3.5 |

At the entrance the game now draws every one of the about 590 people in view (it drew 120);
drawing the flat ones costs about 0.6 ms. Frame times at the entrance with 40 avatars: mean 13.3,
median 12.9, p95 16.5, p99 18.3, max 21.7 ms.

Pictures (same moment, same view; the simulation frozen):

- `docs/evidence/people-flat-entrance.jpg`: as the game is played.
- `docs/evidence/people-avatars-only-entrance.jpg`: `--without flat`, the nearest 120 as avatars.

What the flat people are not: they do not turn their heads, do not show the fine phases of
sitting down, and a person changing from flat to avatar changes its lighting slightly (the sheet
is lit from one studio direction and tinted, the avatar is lit by the scene). At 16 m and more
this was not noticeable in the screenshots; it has not been judged in motion by a person.

## What was changed: at most 18 full avatars a frame

Measured 2026-11-13 with `ZOO_AVATAR_DEBUG=1` (the game then prints, when it closes, how many
people a frame drew, how many of them as avatars, and the time spent posing them, drawing the
avatars and drawing the flat people). The machine was under load from other work the whole time
(load average 4.5-5.6), so these are not best-case figures.

| Place | avatars a frame | drawing avatars ms | per avatar ms |
|---|---|---|---|
| kangaroos | 5.9 | 3.2 | 0.54 |
| entrance plaza | 12 | 5.1-5.5 | 0.44 |
| meerkats (59 people watching) | 35.9 | 14.6 | 0.41 |

A CNA avatar costs about **0.45 ms to draw**, and the cost is linear in their number. Sixty
samples of the main thread's stack at the meerkats (gdb, one a second) put 33 inside
`AvatarRenderer::DrawAvatar`: for each of an avatar's about 15 parts it applies a `SkinnedEffect`
and issues one indexed draw. That is CNA's code, not this game's, and CNA is read-only for this
project; what the game controls is how many avatars it asks for.

So the game now draws at most the **18 nearest** people in view as avatars (it was 40) and the
others as their pictures, with the same reluctance to switch back (somebody who was an avatar in
the last frame counts as 20 % nearer). At the meerkats the people pass went from 15.5 ms to
8.0 ms (frame mean 15.5 ms, 64.5 fps under the same load); at the entrance, where 12 were drawn
before, nothing changes. A person 5 m away drawn as a picture is recognisably flatter than an
avatar when one walks around them; in the screenshot of the meerkat glass
(`build-probe/shots/avatars18standhabitatmeerkat.png`, not kept in the repository) the eighteen
nearest are the row at the glass beside the player, and the people across the habitat are
pictures, as they were before. It has not been judged in motion by a person.

## What was changed: the start of a visit

The 6,000 steps that fill the zoo before the player walks in took 6.8-7.5 s of an 11.5-12.2 s
start. Nearly all of it was one mistake: the animals looked their feeders, shelters and toys
up through `env.objects.FindProp(id)`, and `Env` holds a mutable reference, so this was the
mutable overload, which marks the spatial index of all 1,915 props as stale; the next
`PropsNear` rebuilt it. Read-only lookups now go through `std::as_const` (Codex task T1,
`docs/VISIT_START.md`); the lookups that do change a feeder or a bin still invalidate.

Measured on the game (offscreen, Radeon 780M, `--frames 5`, wall time of the whole process):

| | before | after |
|---|---|---|
| filling the zoo, 6,000 steps | 6.84 s, 7.49 s | 1.19 s, 1.18 s, 1.18 s |
| whole start to the fifth frame | 11.46 s, 12.15 s | 5.52 s, 5.46 s, 5.52 s |

The state after the fill is the same bit for bit: the hashes of three seeds after 600 and after
6,000 steps, and after 500 steps more, were recorded before the change and are tests
(`VisitStart.FillHashesShort`; the 6,000-step one is disabled for the routine run and passed
when run by hand, 5.5 s). The sequence itself moved from the game into the core
(`zoo::FillForVisit`), so that the tests fill the zoo the way the game does; the loading bar
moves while it runs.

What the 5.5 s are now: GL and data 0.3 s, building the zoo 0.6 s, filling 1.2 s, sound 0.2 s,
sky/shadows/avatars and the ground painting about 2.5 s (painting 2.2 s), the animal models
0.7 s.

J27 (2026-11-20) removes the repeated build/fill cost after the first visit by loading a private,
input-keyed save cache. In matched software-GL runs the phase fell from 2.58 s to 0.04 s with the
same state hash, and startup through the populated renderers fell 8.15 -> 5.65 s. These are a
sandbox measurement, not a replacement for the Radeon figures above; `docs/VISIT_START.md`
documents the key, invalidation, corruption fallback and retention rules.

## What was changed: the scenery is one cell during a visit

The scenery was merged in cells of 32 x 32 tiles, one mesh per finish in each: 1,114 draw calls
for the zoo, of which the entrance sees nearly all, and again in the shadow pass. Three builds
with different cell sizes, the same view from the entrance, minutes apart on a busy machine
(load average 6.5-7.5, so every figure is slower than on a quiet one; the comparison holds, the
absolute values do not):

| cell | draw calls a frame | triangles a frame | engine and driver ms |
|---|---|---|---|
| 32 tiles (25 cells) | 1,365 | 2.36 million | 11.70 |
| 80 tiles (4 cells) | 648 | 2.86 million | 8.46 |
| 160 tiles (1 cell) | 419 | 3.32 million | 5.63, 5.72 |

A million triangles more and half the time: a draw call costs about 6 microseconds on this
machine whatever it draws (CNA, EasyGL, Mesa), a million triangles about one millisecond.
So culling by cells loses where the view takes most of them in anyway. A visit now builds the
whole zoo as one cell (108 draw calls in all); the editor keeps the small cells, because an
edit rebuilds its cell. Where culling did help, at a habitat in a corner with the back to the
zoo, the model says the one cell costs up to 0.7 ms more (the penguins: 253 draw calls and
2.3 million triangles instead of 301 and 1.3 million); those views have the most headroom.
Not re-measured on a quiet machine yet.

The lever that is left is the number of finishes (a mesh per colour and pattern): one mesh
with vertex colours for each pattern would need the engine's PBR binder to take vertex colours
(it hands such materials to the stock effect), which is cna-engine's to change, not ours.

## What remained at this checkpoint


- **Scenery**: at this checkpoint it was about 190 draw calls and two million triangles a frame at
  the entrance (both passes). The later nine-sheet distance impostors replace the missing simpler
  tree form; see “Nine-sheet distance impostors” below.
- **Shadows**: about 1.8 ms at the entrance (262 draw calls, 520,000 triangles).
- **Loading**: 5.5 s now (see below); the ground painting (about 2.2 s on the worker threads)
  is the largest part left.
- **Memory**: about 1.05 GB resident; the avatar cast is about 180 MB of it.

## After the habitat fronts (2026-11-13)

The strips added 1,015 planted props (scenery 1,063,415 -> 1,347,525 triangles, still 108
draw calls for the whole zoo's scenery). One run of `--bench --frames 300` at the entrance,
load average 1.6: **109.6 fps**, frame 9.1 ms mean, 12.2 ms p95, 15.0 ms max; people pass
1.83 ms, engine and driver 5.55 ms; 417 draw calls and 3.82 million triangles a frame
through the engine's passes; 1,333 MB resident (about 1,050 MB before the sculpted cats and
the strips; not broken down yet). The planting did not cost a visible frame rate here; the
memory growth is open.

## After the cut-out foliage (2026-11-13)

Crowns and bushes went from one solid blob each to a dark core and 20 to 120 two-sided
cards (`docs/REALISM.md`). The whole zoo's scenery grew from 1,347,525 to 2,177,095
triangles in 121 draw calls (was 108); 1.93 million of them are plants. A shrub is 460
triangles (929 of them: 427,340), an acacia 1,602 (273: 437,346), a willow 2,262, a baobab
2,134, a bamboo clump 1,592, a pine 908, a birch 708, a spruce 450, an oak 1,372.

`--bench --frames 300` at the entrance, three runs within a percent of each other (a
fourth, later: 106.2 fps, 9.42 ms mean, 13.26 p95, 1,405 MB), load average about 2 (another task's Blender builds were running, so the comparison with the
109.6 fps above is not clean; one earlier run with a quieter machine and heavier shrubs
gave 132.7 fps): **100.5 fps**, frame 9.95 ms mean, 10.73 median, 12.88 p95, 15.34 p99,
16.75 max; 428 draw calls (was 417: two more finishes in a cell) and 4.37 million
triangles a frame through the engine's passes (was 3.82 million); 1,423 MB resident (was
1,333 MB; the two atlases with mip levels are under 3 MB, the rest is vertex data in the
static batches, and the whole figure is still not broken down).

What this says: the foliage costs about 0.55 million triangles a frame and 90 MB, and at
most a millisecond of frame time on this machine. A distance form for trees would win
both back and is the next step if the triangle count has to drop.

## After the buildings and the visitors' walk (G3, G4; 2026-11-14)

G3 (buildings, barriers, signs): scenery 2,177,095 -> 2,312,141 triangles, 121 -> 130 draw
calls. `--bench --frames 300` at the entrance, three runs each, interleaved, load average 12
to 17 (other agents): frame mean 35.1 / 24.2 / 27.4 ms before, 28.0 / 26.7 / 24.8 ms after.
No cost measurable under that load.

G4 (visitors steer, give way, walk abreast, lean, look and point): `--people 800 --hour 11
--seed 7 --bench --frames 300`, three runs each, interleaved with a build of the commit
before it, load average 4 to 7: frame mean **22.7 / 22.6 / 21.4 ms before, 24.2 / 27.9 /
25.3 ms after** (44 -> 39 fps). Every pair is slower. Of the game's own parts the people pass
went from 9.4 / 9.1 / 8.4 ms to 11.2 / 12.7 / 12.0 ms and the simulation from 1.0 to about
1.3 ms; set against the animals' time, which G4 did not touch, the people pass is about 30 %
dearer. Not found yet (no profiler on this machine): turning off the new pose layers
(weight, lean, point, talk, stoop, look) gave no measurable change, so the cost is in the
per-visitor bookkeeping of `VisitorRenderer::Update`/`Carry`, which runs for every one of
the 800 people each frame, drawn or not.

The P1 audit on 2026-11-19 disproved that last attribution. The separately timed `people` update
is small; samples and the renderer's own counters put almost all of `people pass` in CNA avatar
drawing, followed by flat people. Flat figures formerly used `DrawUserPrimitives` once per visible
cast member (about thirty temporary buffers and uploads a frame), plus another for the shared
shadow, and recomputed a person's feet up to three times. They now reuse the first feet result and
two retained dynamic buffers, uploading all flat vertices together. A 44,571,410-coordinate probe
matched the old geometry bit for bit; entrance and lion screenshots, 18 game-side tests and a
current AddressSanitizer crowd smoke pass. No new GPU frame-time number is claimed: today's
offscreen environment took 310 ms in its software driver for the preserved baseline binary, so
the new result is the removed buffer churn rather than an unreliable fps comparison. See
`docs/VISITORS.md`.

## After the keepers (G5; 2026-11-14)

Nine members of staff are simulated and all fit the 12-nearest full CNA avatar budget with tools
built on the processor each frame (`docs/KEEPERS.md` "Cost"). Entrance bench, interleaved, three
runs each, load 3.6 to 4.9: frame ms mean 15.05 / 14.22 / 14.53 before against 16.49 / 14.18 /
13.98 after; the people pass (visitors and keepers) 3.16 to 3.41 ms against 4.29 to 4.85 ms; the
simulation 0.27 to 0.30 ms against 0.33 to 0.48 ms. The frame as a whole shows no cost beyond
the spread of the runs.

## Plants in groves with a far form (G2c, 2026-11-14)

Plants are drawn apart from the other scenery in 16 groves of 80 m, each built twice: near (one
draw call a finish) and far (the same individuals with a quarter of the triangles, two draw calls
through one palette texture), far when the whole grove is beyond 56 m, near again within 48 m.
Scenery triangles: all plants near 1.88 million, all far 0.51 million. Entrance bench, Release,
interleaved with develop, load 2.6-4.1: 68.8 / 63.4 / 70.3 fps before, 73.0 / 74.8 / 78.6 after;
triangles a frame 4.67 -> 3.29 million, draw calls 478 -> 516, resident 2,190 -> 2,275 MB. Without
scenery the view runs at 147 fps and with every grove far at about 71: the rest of the scenery's
cost is fill by overlapping alpha-tested cards more than triangles (`docs/PLANTS.md`).

## Finer near shrubs (J33, 2026-11-20)

Smaller leaf-spray cards and three extra visible stems apply only to near shrub and scrub models;
the far forms are byte-for-geometry unchanged. The first visually useful setting was rejected
because it raised the real visitor-zoo scenery 2,258,980 -> 2,708,192 triangles (+19.9%). The
retained lower-coverage setting raises it only to 2,364,188 (+105,208, 4.66%) and keeps 472 draw
calls. A seed-1 savanna shrub is 500 -> 552 triangles while its far form stays at 128. This is a
geometry budget, not an fps claim; the alpha-tested fill cost remains the more important open
plant expense. The direct near/far regression and opened crop are described in `docs/PLANTS.md`.

## After the ground (G1 to G1c, 2026-11-14)

Ground painting, mip levels, meshes and the detail textures run as jobs on up to 6 threads; the
log line is now `ground painted in X s; built in Y s`. Interleaved, three runs each, load 2 to 4:
the whole start (`--frames 2`) went from 9.88 / 8.88 / 8.68 s to 8.90 / 8.22 / 8.07 s. At the
entrance the ground's detail layer adds 23 draw calls and about 104,000 triangles; the frame mean
went from 13.45 / 12.78 / 14.57 ms to 13.52 / 13.26 / 15.21 ms (slower in every pair by 0.07 to
0.64 ms); with `--without terrain` the two builds are equal. Resident memory +60 MB.
`DrawnGroundHeight`, called per animal, visitor and keeper every frame, costs 50 to 66 ns (180 to
270 ns on bank tiles).

## Animal camera culling (2026-11-19)

`AnimalRenderer` used to submit every outdoor animal to the frame, irrespective of the main
camera. The entrance therefore sent all 88 current animals through both engine passes, including
models in habitats behind the camera. Submission now rejects a body only when a conservative
bounding sphere is wholly outside the camera frustum. The sphere contains half the actual
length/height diagonal and another full standing height for animation and a morning shadow;
simulation and animation still update every animal as before.

Short deterministic offscreen runs reported the following submitted bodies (all were 88 before):

| view | submitted |
|---|---:|
| entrance, looking through much of the zoo | 72 |
| Lion Rock | 7 |
| Meerkat Lookout | 17 |
| Penguin Cove | 15 |
| Acacia Plain / giraffes | 46 |

All five 1280x720 screenshots were opened. The local animals, bodies at the edges and shadows
remain present, including the giraffes (the tallest culling case). This is a submission-count
result, not a frame-time claim: animals were already under 1 ms on this GPU. The 18 game-side
Walker/texture tests also pass, as does a current AddressSanitizer lion-view game smoke.

## Animal texture memory (2026-11-19)

The animal renderer made mipmapped copies of every imported texture but kept the original
single-level GPU texture and its CPU recovery pixels through the content manager. It now replaces
single-purpose textures in their model-owned wrapper and disposes the content manager's retained
old renderer. Desktop Linux also omits level-zero CPU recovery shadows while loading static
resources, restoring recovery before runtime capture textures are made.

`tools/bench/memory.sh 2 --bench --lockstep`, software OpenGL because this sandbox has no
`/dev/dri`: largest sampled resident set 3,873 -> 3,381 MB, heap 3,037 -> 2,548 MB. The absolute
result is not a Radeon measurement; the same-machine reduction is 492 MB (12.7%). Immediately
after animal load,
`StartMark` fell 3,475 -> 3,096 MB. A lion viewer image before and after is pixel-identical.

Atlases larger than 1,024 texels on either side now start at their 1K mip, generated with the same
role-aware colour and alpha rules. The same script then reports 2,870 MB resident, 2,373 MB heap,
368 MB anonymous and 127 MB mapped files: another 511 MB and **1,003 MB (25.9%)** below the
original. The animal `StartMark` is 2,467 MB. Elephant, giraffe, brown-bear and zebra viewer pairs
show no visible loss at 720p (the giraffe pair is pixel-identical).

## Shadow-map memory (2026-11-19)

The visit's 90 m-radius sun-shadow map is 2K rather than 4K. It still resolves less than 9 cm per
texel over the 180 m span. The matched start phase with/without shadows measures 413/266 MB at 4K
and 317/266 MB at 2K, so the smaller render targets save **96 MB**. The latest whole-run peak was
2,687 MB; its extra change includes heap variance and is not attributed to the map. Entrance,
lion, zebra and flamingo screenshot pairs show no visible loss at 720p.

The dual plant groves were isolated as well: normal loading adds 369 MB for scenery (875 ->
1,244 MB), while full-detail mode without its unused far batches adds 295 MB (874 -> 1,169 MB).
The far forms therefore cost about 74-75 MB. Normal visits retain them because they previously
bought about 1 ms per frame; `ZOO_PLANTS_FULL_DETAIL=1` now correctly skips them.

## Ground edit-cache memory (2026-11-19)

A visit cannot repaint terrain, but its 100 chunks retained their 512 x 512 RGBA CPU paint images
and tile signatures for the editor's incremental update path. Visitor renderers now discard that
100 MB logical cache after upload; animated water still advances, and the editor keeps its cache.
On glibc the release is followed by `malloc_trim`, because otherwise the empty arenas remained in
RSS. The matched ground loading mark fell from 875 to 826 MB, a conservative **49 MB resident**;
the editor sample stayed 1,175 -> 1,174 MB. The latest whole-run peak is 2,682 MB, only 5 MB below
the preceding run and therefore within heap variation. A zebra view remains visually correct.

The save/load script also reached two reloads and its final screenshot without a renderer fault.
It exposed a separate issue: rebuilding an unchanged visit made software GL retain/defer-release
the old static GPU resources while replacements were uploaded, and the run reached about 3.1 GB.
The old `StartMark` interval labelled "ground" also included all frames since startup, so its
reported 107 s was not solely terrain construction.

An in-place visit load now compares the fields that determine the static scene: tile and vertex
geometry, fence kind and gate state, and each building's and prop's drawn identity and transform.
Simulation-only values such as fence/building condition, sales counts and feeder contents are
deliberately ignored. When that scene is identical, `Adopt` keeps the existing ground and scenery
uploads. Animal actors are cleared so a loaded animal cannot interpolate from its former position,
but the expensive species models and textures are retained. A different map or any actual static
edit still rebuilds normally; editor loads always rebuild so their incremental caches remain valid.

In the two-reload software-GL check, both retained-scene paths recorded 0.00 s for ground,
scenery and animals. RSS was 3,023 MB after the first load and 3,032 MB after the second, and the
run ended at 3,033 MB versus 3,120 MB in the earlier longer script. The absolute peaks are not a
matched GPU measurement, but the eliminated uploads and the 9 MB second-load growth demonstrate
that the old replacement overlap is gone. Both post-load screenshots were visually correct.

## Lazy audio decoding (2026-11-19)

`Soundscape` used to decode all 53 Ogg recordings in its constructor; those files total 22.71 MB
as PCM. The current catalog has 54, including the later fountain loop,
but decoder and audio-backend copies made the measured loading phase add 46 MB resident. It now
reads the manifest and mix at start and decodes each take only on its first play. The sound loading
mark remains 220 -> 220 MB instead of 220 -> 266 MB. A 60-frame visit decoded three recordings,
kept four voices audible (one crowd take is positioned twice), and reported no failure. Calls and
footstep surfaces therefore cost nothing until selected. The full-run sample moved 2,682 ->
2,711 MB in this run, opposite to the isolated phase because heap sampling varies; it is not used
as evidence of a regression or a saving.

## Animal variants on demand (2026-11-20)

`AnimalRenderer` used to import a species' adult, female and juvenile GLBs together as soon as it
met the first animal. Greenhill has adult brown- and polar-bear pairs but no cubs, so their 5.12 MB
and 5.78 MB compressed juvenile files, decoded meshes, clips and textures stayed resident without
ever producing an actor. Female and juvenile variants are now imported independently on the first
animal that needs each one; a later birth follows the same path on the next update.

With `ZOO_LOAD_TIMING=1`, the normal visit loads 31 models rather than 33. The sum of per-model
import/view and mipmap times in one matched one-frame run fell from 1.443 + 1.584 s to
1.307 + 1.441 s, a 0.279 s reduction. The same immediate before/after
`tools/bench/memory.sh 2 --bench --lockstep` software-GL sample fell from 2,796 to 2,736 MB
resident (heap 2,461 -> 2,394 MB). That **60 MB** is a matched sandbox reduction, not a Radeon
absolute figure; the other five script modes moved in the same direction by 77 to 101 MB but are
reported only as corroboration because short-run heap variation is large. A final off-screen
Brown Bear habitat load imported both adult bear models, neither cub model, and completed cleanly.
The warning-clean game build and all 272 active functional CTest cases pass; 21 diagnostics remain
disabled.

## Imported animal timelines (2026-11-20)

Each glTF import owns XNA `SkinningData` timelines, and `AnimalRenderer` used to retain those after
deep-copying every bone track into the engine-neutral clips its actors actually play. Once that
conversion succeeds, the renderer now releases the imported timelines while retaining the model,
GPU buffers, materials, textures, skeleton and converted clips. The content-manager copy is safe
to consume here: one `AnimalRenderer` owns all animal use in a game, and its per-model cache serves
every later actor and save reload.

J47's bear families mean the current visit legitimately imports 33 variants rather than J24's 31.
On that current zoo, an immediate matched `tools/bench/memory.sh 2 --bench --lockstep` software-GL
run fell from 2,848 to **2,799 MB** resident and from 2,499 to **2,457 MB** heap. The animal loading
mark fell 2,601 -> 2,550 MB. The five corroborating modes moved by 64, 59, 43, 20 and 16 MB; the
small editor result is expected because its starter zoo imports fewer species. These are
same-machine short-run figures, not Radeon absolutes.

A real Lion Rock run after 3,000 simulation ticks completed 60 frames, selected a resting lion and
produced a visually intact frame. The same warm-up plus a three-frame game run passed under
AddressSanitizer and UndefinedBehaviorSanitizer. All 29 game-side C++ tests pass; leak detection
was disabled for the sanitizer smoke because it is unavailable under this managed ptrace session.
The complete C++ suite passes all 301 active cases in 311.90 seconds, with 21 intended diagnostics
disabled; the unchanged Python animation pipeline passed in the immediately preceding full run.

## Animal mip staging memory (2026-11-20)

CNA's static `Texture2D` path already releases level-zero CPU pixels when desktop context recovery
is disabled, but `UploadTexture` retains CPU staging for every smaller mip. After an animal atlas
has uploaded its complete chain, the desktop Linux visit now re-wraps that same GPU renderer
without those staging vectors. It does this only where `ZooGame` deliberately disabled context
recovery and the renderer only samples the static atlas; Windows, Android, the web build, the
viewer and the default helper path retain the former readback/recovery behaviour.

Immediately after J53, the same short software-GL profile moved from **2,799 to 2,760 MB** resident
and 2,457 to **2,405 MB** heap. The animal loading mark moved 2,550 -> **2,486 MB**. The five
alternate modes moved by 60, 33, 73, 96 and 13 MB in the same direction, but their spread is wider
than the change, so only the direct 39 MB resident reduction is claimed. This remains a sandbox
comparison, not a Radeon absolute measurement.

A direct regression requests CPU staging removal, proves level-zero pixels are gone while the
wrapper and complete level count remain, and the six texture tests pass under AddressSanitizer.
All 30 game-side tests pass. A matched 3,000-tick Lion Rock pair differs in 1,321 of 921,600 pixels
(0.14%), confined to moving distant people and animals; the scene and textured surfaces are
unchanged. A separate sanitized real visit completed its 6,000-step fill, loaded all 33 variants
and drew 65 of 90 animals without an ASan/UBSan finding. Leak detection remains unavailable under
this managed ptrace session. The final complete C++ suite passes all **302 active cases** in
366.28 seconds, with 21 intended diagnostics disabled; the unchanged Python animation pipeline
passed in the immediately preceding full run.

## Terrain mip staging memory (2026-11-20)

The read-only visitor terrain has one complete 512 px mip chain for each of its 100 chunks,
three 1024 px ground-detail chains and a 512 px apron. CNA already releases their level-zero CPU
pixels when desktop context recovery is disabled, but retained every smaller level after upload.
Those smaller staging vectors total 37.7 MiB logically. `DiscardEditCache` now re-wraps the
same completed GPU renderers without those vectors, preserving each wrapper's address so all
material pointers remain valid. This is restricted to desktop Linux visits: the editor keeps its
paint cache and staging, and platforms which retain context recovery are unchanged.

Immediately after J54, the same `tools/bench/memory.sh 2 --bench --lockstep` software-GL profile
moved from **2,760 to 2,665 MB** resident and 2,405 to **2,301 MB** heap. All four alternate visit
modes also moved down, by 50, 15, 23 and 14 MB; the unchanged editor path moved up by 49 MB in
short-run allocator noise. The direct 95 MB observation is larger than the allocations removed,
so only the 37.7 MiB logical release is attributed to the change; the samples corroborate its
direction but remain sandbox measurements rather than Radeon absolute figures.

The common in-place operation is covered by the texture regression, including wrapper identity,
GPU renderer lifetime and complete mip count. All 30 game-side tests pass, as do the six texture
tests under AddressSanitizer. A real sanitized visit completed its 6,000-step fill and rendered
65 of 90 animals without an ASan/UBSan finding. A matched 3,000-tick Lion Rock image remains
visually intact; 13,253 of 921,600 pixels (1.44%) differ from J54, localized by the difference
image to moving people and animals rather than the terrain. The full animation pipeline passes
all 131 cases in 442.306 seconds, and the complete C++ suite passes all 302 active cases in
253.16 seconds, with 21 intended diagnostics disabled.

## Common static-texture memory (2026-11-20)

The same read-only desktop visit now releases CPU staging for the immutable procedural surface
textures after upload: the pattern set, foliage cards, sign sheet, viewing pane, fence textures
and all 48 water-normal frames. Their smaller mip levels total **5,046,296 bytes (4.81 MiB)**.
Water animation is unaffected because its material still selects among the same GPU-backed
texture wrappers. The completed far-plant palette also releases its 1024-square source image,
three 128-square source miniatures, allocation map and smaller mip staging, another **5,789,012
bytes (5.52 MiB)**. Thus 10,835,308 known bytes (**10.33 MiB**), plus small map allocations, no
longer remain on the CPU. The editor retains all of these resources, and context-recovering
platforms retain texture staging; non-editor visits on those platforms may still release only the
far palette's source image and allocation caches.

No resident-memory reduction is claimed from the short-run sampler. Its direct as-played result
moved from 2,665 to 2,698 MB while four alternate visitor modes moved down by 9, 39, 13 and 11 MB;
the unchanged editor path moved down by 29 MB. That contradictory spread is allocator noise, so
the exact allocation accounting above is the useful result.

All 30 game-side tests pass. A matched 3,000-tick Lion Rock frame differs from J55 in 1,361 of
921,600 pixels (0.15%), localized to moving people and animals; the static scene is intact. A real
ASan/UBSan visit builds the complete scenery, loads all 33 animal variants and draws 65 of 90
animals without a finding. The editor starts cleanly, and the 400-frame end-to-end visit script
saves, loads the same zoo twice, saves again and exits through its menu without rebuilding the
discarded static cache. The complete suite passes all **303 active tests** in 420.97 seconds,
including the animation pipeline, with 21 intended diagnostics disabled.

## Static mesh CPU shadows (2026-11-20)

`--bench` now prints the process-wide `GpuMesh` ownership totals beside RSS. In the ordinary
Visitor Zoo it reports **1,023 meshes, 112.2 MiB of vertex buffers and 20.7 MiB of index buffers**.
CNA retains a complete CPU shadow for both buffer kinds even though `GpuMesh::Upload` creates them
`WriteOnly`, so their `GetData` is unavailable and these static meshes are never updated. The
132.9 MiB is only a lower bound because imported animal-model buffers are borrowed and not present
in this ownership counter.

This is documented as ZOO-UP-004 in `docs/UPSTREAM.md`, not worked around with private state in the
game. CNA currently exposes no operation that preserves a buffer's GPU renderer while releasing
its shadow. The new counter is diagnostic only and does not change memory use or drawing. Its
warning-clean build, real 45-frame benchmark and all 30 game-side tests pass.

## Trees between a visitor and an animal (2026-11-21)

The visitor checks the opaque surfaces of a tree only when an animal is under the crosshair.
Each grove stores a bounding box for its wood and crown cores and another for each tree.
At most sixteen recently queried trees keep a CPU triangle index. The scene builds each
index using the same plant seed, placement and detail level as the visible tree.

In three seed-1 software-GL visits at 13:00, the `--bench` animal gaze section averaged
0.22 ms per frame looking into the tree fork that covers Ravi, 0.12 ms looking past it
toward Esme, and 0.05 ms looking across Capybara Marsh. Each run measured 29 frame
intervals after 40 warm-up frames at 1280 × 720. This section includes both matching the
displayed animal and checking tree cover. [Views used for the measurements](evidence/trees-and-binoculars-2026-11-21.jpg)

## Textures shared by animal models (2026-11-21)

Several GLBs contain the same image. All six brown and polar bear models use the same
normal map, for example. The adult lions and tigers also share a normal map, and the
lion family shares its mane cutout texture. In a Linux desktop visit, animal models
can now share an uploaded texture and its smaller mip levels. The renderer matches
the dimensions, every channel of the capped base image (SHA-256), and the settings
that determine the smaller levels, including colour space and alpha cutoff.

Each model still owns its texture wrapper. Unloading one wrapper leaves the others
usable. A weak reference in the index lets the GPU allocation go when its last owner
releases it. Set `ZOO_LOAD_TIMING=1` to report the cumulative number of uploads
avoided and their uncompressed RGBA8 size, including mip levels.

The seed-1 visit at 13:00 loads 49 model variants for 107 animals. It reused 14
chains, avoiding 67,108,856 bytes of GPU image storage (64.0 MiB). At Lion Rock,
the 1280 × 720 lockstep images with visitor drawing disabled matched at all 921,600
RGB pixels. Both runs showed Zain resting and drew 53 animals. All 40 game tests
passed, including the two checks for independent model lifetimes, filtering settings
and changes to a single pixel.

| Lion Rock, 13:00 (software GL) | Resident immediately after loading animals, before / after | Total time preparing animal mipmaps, before / after |
|---|---:|---:|
| Full visit | 3,677 / 3,569 MB | 2.96 / 3.79 s |
| Visitor drawing disabled | 3,677 / 3,508 MB | 3.64 / 3.41 s |

The timings include image matching and uploading. They varied in both directions under
host load. The first implementation used Sharp Runtime's portable SHA-256 and added
roughly 1.1 seconds to mip preparation in the first paired run. Desktop Linux builds
now use OpenSSL's accelerated implementation when available, with the portable one
as a fallback. The allocation counter measures the 64.0 MiB saving directly. Resident
memory includes the software renderer, driver allocations and allocator behaviour.

## Peak memory while loading the terrain (2026-11-21)

The expanded park has 196 ground chunks. Their base paint images alone total 196 MiB.
Previously a visit held all of them until every chunk, the ground detail and the land
beyond the fence were ready. The render thread now releases each paint image as soon
as it uploads that chunk. On desktop Linux it also releases the CPU copies of the
smaller mip levels at the same point. A visit can skip computing tile signatures,
which only the terrain editor uses when repainting a changed tile.

At Lion Rock in the same seed-1 visit, Linux reported a process high-water mark of
1,248.5 MiB by the time the ground finished loading. With per-chunk release it reported
1,011.2 MiB and 1,010.8 MiB in two runs. The matched reduction was **237.3 MiB while
loading the terrain**. Resident memory just after this phase was 971 MiB before and
982 MiB after. Animals and scenery load later and have their own peaks. These runs used
the software OpenGL driver and the same two lockstep frames with visitor drawing disabled.

Both new Lion Rock captures matched the reference at every RGB pixel. The editor kept
its paint images and tile signatures. All eight frames from the scripted editor run
also matched the reference, including the exposed-earth brush stroke and the new pond.

## Buildings in the sightline (2026-11-21)

The expanded park places 55 buildings drawn from 11 designs. Each design shares one
local triangle index across all its placements. A ray enters the building's local
coordinates and checks its bounding box before testing triangles. Rotating a kiosk
or placing it on raised ground uses the same placement as its rendered model.

Two 70-frame software-GL visits measured 0.05 ms per frame for animal identification
while looking at the kiosk that hid Ines and 0.06 ms looking past the restroom towards
Sara. Each timing covers the last 29 frame intervals and includes the existing checks
against animated bodies and trees.

## Fences in the sightline (2026-11-21)

In seed 1 the fence batch draws 261,618 triangles. During construction, each row of
up to eight tiles records the bounds of the opaque vertices it actually wrote. This
includes sloping rails, posts, gates and the ticket lodge attached to the entrance arch.
A visit groups those bounds in squares of 32 tiles. Only rows intersected by the
sightline rebuild their triangles for testing. The cache keeps the last 32 rows and
discards them when a fence batch is rebuilt.

From the public path at the camel paddock, animal identification took 0.11 ms per
frame with the crosshair on the board and 0.11 ms with it on Ines's head. Checking
every row's bounds individually in the first implementation took 0.32 and 0.30 ms
from those spots. All four measurements cover the last 29 intervals of a 70-frame
software-GL visit with 800 visitors, 107 animals and 7× binoculars.

## Storage for animal animation keys (2026-11-21)

The animation importer grows each bone's array as it converts a clip. Once all
keys are available, the game releases the unused capacity. With 49 models loaded
for the 107 animals in seed 1 at 13:00, the before/after capacities differ by
14,932,148 bytes (14.24 MiB). `ZOO_LOAD_TIMING=1` reports the measured bytes
for each model. The key times, translations, rotations and scales stay in order.

Two 24-frame lockstep visits at Lion Rock used the same save and disabled visitor
drawing for the image comparison. Both identified the resting cub Zain and their
1280 × 720 frames agreed at every RGB pixel. Immediately after loading the animals,
the process reported 3,529 and 3,515 MiB resident. The capacity measurement counts
the animation arrays directly.

### Countryside mesh outside the property (2026-11-21)

Each of the six field materials now uploads only the terrain vertices its triangles use.
Shared border vertices keep the normals calculated across the whole landscape. Smaller
batches and the perimeter skirt fit in 16-bit indices. In the same editor overview,
the renderer reported 20.2 MiB of vertex buffers and 3.7 MiB of index buffers across
591 meshes, down from 20.8 and 3.8 MiB. Three matched 1280 × 720 captures had identical
RGB pixels, covering the editor opening, its countryside overview and the east garden
in a populated visit.

## Nine-sheet estate vegetation (2026-11-22)

The 1,272 × 636 tile property contains 73,587 props, including 28,552 pines and 16,783
birches. The former `PlantDetail::Far` still built between roughly one hundred and six hundred
triangles for each plant. It was a useful lower-detail tree in the old estate, but not a distance
impostor at the scale of the nine sheets.

The far form now uses a four-sided stem and crossed cut-out foliage cards. Common trees use 20
triangles (24 for palms); shrubs and ground plants use 8. An intermediate form retained a coarse
opaque crown heart, but entrance screenshots exposed it as a dark diamond between the cards, even
after shrinking it and changing its normals. The rendered heart was removed. A rare, cached animal
sight query reconstructs the full physical tree instead, so a card's rectangular gaps do not make
its dense crown transparent to identification. The detailed visible form inside the 48/56 m
hysteresis is unchanged and is still built lazily.

Across the saved 118-habitat zoo, all-far vegetation fell from about **18.7 million to 1.11
million triangles**, total static scenery from **22.52 million to 4.93 million**, and scenery
preparation from 7.6 to about 2 seconds. The final entrance screenshot is
`build-probe/shots/nine-sheet-lod-entrance-final.png`; unlike both rejected heart variants, it has
no geometric knots in the canopies.

A matched 120-frame software-OpenGL entrance run with shadows at the intermediate 1.82-million
form improved from **0.5 to 0.8 fps** and from 21.08 to 11.92 million engine-pass triangles per
frame. Resident memory fell from 7,921 to 5,847 MiB; owned GPU vertex storage fell from 1,124 to
547 MiB. These absolute frame rates do not predict a hardware GPU, but the matched direction and
allocation counts do. With shadows disabled the final 1.11-million form measured 0.9 fps, 10.71
million triangles per frame, 5,637 MiB resident and 512.5 MiB of owned GPU vertices. The
intermediate no-shadow form was also 0.9 fps and 11.09 million triangles; rasterisation is now
dominated by other visible geometry and overlapping cut-outs rather than raw plant triangle count.

After the final plant form and the visit distance below were both in place, the same full run with
shadows measured **1.3 fps**, 1,996 engine-pass draws and 8.02 million triangles per frame, with
5,673 MiB resident. Against the original enlarged scene, that is 0.5 -> 1.3 fps, 3,538 -> 1,996
draws, 21.08 -> 8.02 million triangles and 7,921 -> 5,673 MiB. The relative software-renderer
speedup is 2.6×; a hardware result still needs measurement on the player's machine.

Two plausible follow-ups were first measured at the 3 km view and rejected. Reducing visit cells
from 256 to 128 tiles cut triangles only from 11.09 to 10.73 million but raised engine-pass draws
from 3,387 to 4,339 and reduced the software result from 0.9 to 0.8 fps. Supplying tight boxes in
addition to the engine's existing batch spheres saved only ten draws and 0.8 percent of triangles.
The latter stayed reverted. The smaller cells were tested again below after shortening the view;
there they can actually discard enough distant geometry to win.

The visit and overhead editor also inherited the same 3 km camera far plane. That is necessary to
show the whole estate from above, but on foot a two-metre object is already only a few pixels tall
hundreds of metres away. The visit now uses 600 m while the editor explicitly retains 3 km. An
intermediate 800 m run improved 0.9 to 1.3 fps, 3,387 to 1,848 engine-pass draws and 10.71 to 7.20
million triangles. The final 600 m no-shadow run measured **1.6 fps**, 1,115 draws and 4.83 million
triangles. Entrance captures at 3 km, 800 m and 600 m show no static-scene or horizon loss
(`nine-sheet-lod-entrance-final.png`, `nine-sheet-800m.png` and `nine-sheet-600m.png` in
`build-probe/shots`). The shorter plane also leaves more depth-buffer precision for fences and
terrain; it does not remove or simplify any zoo content.

## Nine-sheet estate furniture and animal LOD (2026-11-22)

At 600 m, retesting 128-tile (256 m) visit cells improved the no-shadow entrance from **1.6 to
1.7 fps**, mean frame time 614 to 583 ms and engine-pass geometry 4.83 to 4.56 million triangles.
Draws rose from 1,115 to 1,468, but unlike the 3 km experiment the finer culling now saved enough
work to cover them. This cell size is retained.

Path furniture is separated from the other static objects in each cell. Within 144 m it keeps the
original models. Beyond 176 m, with hysteresis in between, lamps retain a pole and lantern, bins
their body and rain hood, and benches and picnic tables their readable slabs and frames rather
than individual staves, rings, slats and fixings. This changed **1.7 to 1.8 fps**, 583 to 553 ms
and 4.56 to 4.29 million engine-pass triangles; draws were effectively unchanged (1,468 to
1,478). Close furniture is byte-for-byte the existing geometry. A matching far fence model was
rejected: despite lowering 4.29 to 4.06 million triangles it worsened 553 to 565 ms and added
about 105 MiB resident memory by retaining a second copy of 1.88 million detailed fence
triangles.

Category isolation then found the dominant remaining cost. Removing only scenery changed 553 to
490 ms; removing only terrain changed it to 518 ms. Removing only animal drawing, while retaining
all 589 animals and their simulation, changed **1.8 to 4.5 fps**, 553 to 225 ms and 4.29 to 1.47
million triangles. The entrance frustum contained 141 fully skinned bodies, most too small to
resolve but still carrying their complete rigs, materials and topology.

An animal whose largest body dimension falls below ten vertical screen pixels now uses a
low-poly quadruped or bird silhouette scaled to its real body length, radius and standing height,
with a restrained species/group colour. It still moves, turns, sits at its true ground or water
height, casts a shadow and retains its water disturbance. Above fourteen pixels the original
animated and textured model returns; between the thresholds the previous choice remains. The
calculation uses the actual viewport and camera field of view, so 7x binoculars restore the full
model without a special case. At the entrance 117 of 141 visible animals used this form.

With no shadows the animal LOD changed **1.8 to 3.5 fps**, mean frame time 553 to 285 ms and
engine-pass geometry 4.29 to 2.00 million triangles. With shadows, the final enlarged zoo measures
**2.9 fps**, 345 ms, 1,668 draws and 2.64 million triangles, versus 1.3 fps before animal and
furniture LOD and 0.5 fps in the first enlarged rendering. The absolute llvmpipe rates are not a
hardware forecast; they are matched evidence that the enlargement regression is now primarily
removed rather than hidden by deleting zoo content. Reviewed 1280x720 captures are
`animal-lod-entrance.png`, `animal-lod-giraffe-near.png` and `animal-lod-lion-near.png` in
`build-probe/shots`.

## Experimental 3,000-person crowd (2026-11-22)

The sixth crowd setting, **Many (experimental)**, keeps 3,000 real simulated visitors rather than
decorative extras. A fresh seed-1 nine-sheet visit built all 118 habitats in 40.8 s, filled to
exactly 3,000 people in 81.3 s and spread 2,693 of them around the estate. The resulting cache
saved and reloaded all 3,000 people in 0.64--0.67 s with the same state hash. No crash, failed
allocation, avatar failure or visitor-count truncation occurred.

These matched 1280x720 offscreen runs used `--bench --frames 300 --lockstep --without shadows
--without animals`; they measure 259 settled frame intervals after each fresh fill. This Mesa
offscreen path is dominated by a roughly 100 ms engine/driver pass and is not a hardware-GPU
forecast. It is useful for the within-run comparison, but the exact local crowd in the entrance
frustum makes the people pass non-monotonic between More and Many.

| Setting | Simulated / visible / drawn | Fill | fps | mean / p95 | simulation | people pass | peak RSS |
|---|---:|---:|---:|---:|---:|---:|---:|
| Normal | 800 / 95 / 95 | 31.9 s | 8.1 | 123.3 / 127.7 ms | 1.82 ms | 8.62 ms | 5,595 MB |
| More | 1,200 / 110 / 110 | 38.1 s | 4.9 | 202.2 / 208.4 ms | 1.97 ms | 92.34 ms | 5,624 MB |
| Many | 3,000 / 302 / 302 | 81.3 s | 5.0 | 198.9 / 207.7 ms | 3.25 ms | 89.05 ms | 5,631 MB |

Many is therefore deliberately labelled experimental and the settings window warns that it can
be much slower. Normal remains 800. The inspected evidence is
`build-probe/shots/many-entrance.png`, `many-meerkat-hub.png` and
`many-remote-okapi.png`; the last is the actual north-east Okapi Rainforest view selected by
`--stand habitat:2:okapi`. The entrance capture shows the intended dense plaza without replacing
visitors by scenery.

## Apocalypse gate transitions (2026-11-23)

Opening 119 keeper gates originally invalidated the player collision world and synchronously
rebuilt the full 1,284 × 648-tile estate, presenting as a frozen game. Fence collision shapes are
now retained by edge and patched only for the recorded breaches. A full Greenhill regression
opens and closes the mode, checks the exact shape-count change and requires each update to remain
under two seconds. Static fence drawing is refreshed separately at no more than two 128-tile cells
per frame; the discarded plant-atlas edit cache is not reconstructed. Dangerous loose-animal
lookups use 32 m spatial cells rather than scanning every threat for every visitor group.

A second freeze was in the simulation after the transition. Some collision sidesteps ended on the
planted stand-off beside an opened gate. The invalid-position recovery then searched all 832k map
tiles for an impossible passable tile in that same stand-off on every tick, while route obstacle
queries called `PropsInTiles` but linearly scanned all 74,683 props. Escape steps now require a
passable destination, old invalid loose positions recover locally to public ground, and
`PropsInTiles` uses the existing 16 m prop buckets. Visitor sites defer their full-estate rebuild
while Apocalypse only opens gates. The 200-step full-zoo measurement dropped from 8.5--11.9 s to
0.89 s; its regression ceiling is 5 s.
