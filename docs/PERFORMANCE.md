# Performance: what was measured, what was changed

Everything here was measured on the development machine, on its real graphics hardware:
AMD Ryzen 7 PRO 7840U with the integrated Radeon 780M (Mesa, OpenGL through EGL), 1280x720,
without a window (`SDL_VIDEODRIVER=offscreen` renders on the GPU's render node, it is not a
software rasteriser) and without sound output (`SDL_AUDIODRIVER=dummy`). `tools/bench/bench.sh`
runs the matrix; each figure is one run of `./zoo-simulator --bench --frames 300`, the default
visit: 800 visitors, 74 animals, 10:30 in the morning. Nobody watched these runs on a monitor;
the pictures named below are screenshots the game wrote itself.

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

## What remains


- **Scenery**: see "the scenery is one cell" below; what is left of it is about 190 draw calls
  and two million triangles a frame at the entrance (both passes). Trees are 760,000 of the
  zoo's 1,063,415 scenery triangles (`--bench` prints the tally by kind); they have no simpler
  form for the distance yet.
- **Shadows**: about 1.8 ms at the entrance (262 draw calls, 520,000 triangles).
- **Animals** are submitted wherever they are: 1.19 million triangles a frame at the entrance
  for animals nobody can see from there. Cheap on this GPU (under 1 ms) but free to remove.
- **Loading**: 5.5 s now (see below); the ground painting (about 2.2 s on the worker threads)
  is the largest part left.
- **Memory**: about 1.05 GB resident; the avatar cast is about 180 MB of it.
