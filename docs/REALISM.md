# Realism, immersion and stabilisation: progress log

What was changed since 2026-11-13 to make Greenhill Zoo look and behave more like a real
zoological garden, with what was measured and where the pictures are. Detailed figures live
in the documents each row names; this file is the index and the honest list of what is not
done.

**Conditions of every measurement:** Radeon 780M (Mesa), 1280x720, off-screen driver on the
real GPU, machine shared with other agents (load average 3 to 6, said where it matters).
**Sound:** the game's output was captured through SDL's file driver and measured; the
loudspeaker stays muted and nobody has listened to any of it.

## Done

| What | Result | Where |
|---|---|---|
| Visit start runs in the core | fill of 800 visitors 7 s -> 1.2-1.6 s | `docs/VISIT_START.md` |
| Scenery in one cell during a visit | engine time at the entrance 11.7 -> 5.7 ms | `docs/PERFORMANCE.md` |
| At most 18 full CNA avatars a frame | people pass at the meerkats 15.5 -> 8.0 ms | `docs/PERFORMANCE.md`, `evidence/people-*.jpg` |
| Groups at benches | a group sits only if half of it can; the rest stand in an arc | `docs/VISITORS.md` |
| Audio, second round | more recordings, a measured per-take mix, labelled substitutes | `assets/audio/`, `assets/ATTRIBUTION.md` |
| Regression tests | empty habitats, information boards, save/load of a visit, wading depth | `tests/core/` |
| Light from the sky and the ground | below | `src/game/SkyLight.*` |
| Water with ripples | below | `src/game/Surfaces.cpp` |

### Light from the sky and the ground (2026-11-13)

Before, everything not in the sun got one flat ambient colour: an elephant in the shade was a
grey silhouette and water a matt teal sheet. Now every physically based draw (animals,
terrain, scenery, fences, glass, water) takes its ambient light from an environment the game
computes from the sky's colours of the hour (`SkyLight`): sky above, a band of trees on the
horizon, sunlit ground below. The cube maps are 8 and 32 texels on an edge, filled on the CPU
when the sky changes by more than 0.8 %.

Calibration, as measured on screenshots (linear averages of regions, before -> after):

- ground at the lions 0.206 -> 0.224 (green channel): a surface facing up keeps the ambient
  it had; the 8 % is the sky's reflection on a rough surface;
- shed wall at the lions 0.172 -> 0.157 and yellower: walls get less sky and the ground's tint;
- the first version doubled the light on everything facing up and turned shade blue; it was
  not kept (the fill light from above already stands for the sky there).

Cost: 8.58 -> 8.75 ms mean frame at the penguins, 11.36 -> 11.48 ms at the entrance (500
frames each, load average 3.6: within the noise of the machine). `--without skylight` gives
the old light for comparison.

### Water (2026-11-13)

The sheet over a pool now carries a normal map of ripples: 48 frames of a 4 s loop, 128
texels over 4 m, fourteen waves that fit the tile and the loop a whole number of times (no
seam in space or time), drawn out across one wind direction. With the sky light the far side
of a pool reflects the trees and the near side shows the water's own colour.

Picture: `evidence/sky-light-water-2026-11-13.jpg` (left before, right after: meerkats,
penguins, elephant).

Not done about water: the bank is still a straight tile edge, nothing splashes or rings
where an animal wades, and there are no swimming clips yet.

## Open (in the order they hurt)

1. Animals: twelve of fifteen species are still the stylised first generation
   (`docs/ANIMALS.md`); sculpt-based lion, lioness, cub and tiger are under review.
2. Habitat fronts: elephants and rhinos at arm's length, bars at the big cats (task E1).
3. Meerkat Lookout draws 59 of 177 watchers at 10:30; its pool is too large and props stand
   in it.
4. Trees: 760,000 of 1,063,415 scenery triangles, no distance form, canopies read as blobs.
5. Keepers are not seen. No swimming clips. Bear growl is quiet in the mix.
6. Loading 5.5 s, 2.4 s of it painting the ground.
