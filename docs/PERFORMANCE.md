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

The nearest people (within 16 m, at most 40) are full CNA avatars, as before. Everybody else in
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

## What remains

- **Scenery** is now the largest cost at the entrance: 1,063,415 triangles in 1,114 draw calls
  for the whole zoo, 6 ms of a 13 ms frame at the plaza (138.9 fps without it). The cells are
  culled by the view only, not by distance, and every finish of a cell is its own draw call.
- **Shadows**: about 1.8 ms at the entrance (262 draw calls, 520,000 triangles).
- **Animals** are submitted wherever they are: 1.19 million triangles a frame at the entrance
  for animals nobody can see from there. Cheap on this GPU (under 1 ms) but free to remove.
- **Loading**: the warm-up steps (see `docs/VISITOR_ZOO.md` once the visit start is moved into
  the core).
- **Memory**: about 1.05 GB resident; the avatar cast is about 180 MB of it.
