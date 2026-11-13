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
| Lion, lioness, cub and tiger on a professional sculpt (CC BY 4.0) | real cat anatomy; the lioness has no mane any more | `docs/ANIMALS.md`, `evidence/big-cats-sculpt-2026-11-13.jpg` |
| Animals stand in the sun, not in their own shadow | every skinned model was drawn shadowed by itself (an engine defect, `docs/UPSTREAM.md` ZOO-UP-001); lit and shaded now, and normal maps on animals show for the first time | `src/game/SunShadows.cpp`, `evidence/animal-self-shadow-2026-11-13.jpg` |
| Habitat fronts a zoo would build | elephants and rhinos behind a 4 m planted stand-off with a leaning rail, hoofstock behind 2 m; heavy dark mesh instead of bars at bears and cats; chain-link one looks through; nobody walks on the planting | `docs/VISITOR_ZOO.md`, `evidence/habitat-fronts-2026-11-13.jpg` |
| People at every habitat | 192 to 260 of 800 stand watching (was 110 to 215) and no habitat is left without watchers (several were, most of the time) | `docs/VISITOR_ZOO.md` "Who watches where" |

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

1. Animals: thirteen of fifteen species are still the stylised first generation
   (`docs/ANIMALS.md`). Of the sculpt-based cats only the lioness is good: the male needs a
   real head and mane, the tiger a face. No young animal lives in the zoo yet.
2. Habitat fronts: hoofstock is still seen through chain-link behind its strip; shrubs in
   the strips are coarse blobs.
3. Meerkat Lookout's pool is too large and props stand in it.
4. Trees: 760,000 of 1,063,415 scenery triangles, no distance form, canopies read as blobs.
5. Keepers are not seen. No swimming clips. Bear growl is quiet in the mix.
6. Loading 5.5 s, 2.4 s of it painting the ground.
