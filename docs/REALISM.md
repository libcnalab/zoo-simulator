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
| Visitors walk like people | they steer instead of sliding along a line, each at a pace and stride of its own; groups walk two abreast; walkers give way to oncoming and standing people and to the player; those who eat, read, watch or queue get ground of their own (pairs closer than 0.3 m in a crowd of 800: 81 -> 9); watchers lean on the rail, point and turn to follow an animal. Costs about 3 ms a frame with 800 people (`docs/PERFORMANCE.md`) | `docs/VISITORS.md`, `evidence/visitors-g4-2026-11-14.jpg` |
| Buildings, barriers, signs and furniture | clear viewing glass on a kerb instead of milky sheets; welded mesh with an overhang leaning towards the animals and DANGER plates at the big cats and bears; printed signs (ZOO over the gate, TOILETS, DRINKS, ICE CREAM, BURGERS, GIFT SHOP, EDUCATION CENTRE), kiosk counters, gutters, sheet-roofed barns, a ticket lodge, faceted rocks, printed info boards instead of blank lecterns. Scenery 2.18 -> 2.31 million triangles, 121 -> 130 draw calls; the entrance bench (3 runs each, load 12-17) showed no measurable cost | `src/game/{BuildingModels,FenceModels,PropModels,RockModels,SignTextures,BarrierTextures}.cpp`, `evidence/built-g3-2026-11-14.jpg` |
| Big cats, second pass: the sculpt's own colour and normal maps, a mane of hair cards | the male has a lion's head and a mane instead of a lioness's head in a collar; eyes, nose and lips are the sculptor's; irregular tiger stripes | `docs/ANIMALS.md`, `evidence/big-cats-s7-2026-11-14.jpg` |
| Rhinoceros and elephant on rigged models (CC BY 4.0 as published; authorship unconfirmed, `assets/ATTRIBUTION.md`) | photographic skin with normal maps, real proportions, bull, cow and calves; the elephant's second pass (2026-11-14) made it grey in the game's light with tusks of a believable length | `docs/ANIMALS.md`, `evidence/rhino-elephant-rigged-2026-11-13.jpg` |
| Zebra stallion, mare and foal from a static sculpt (CC BY 4.0), rigged by us | a real horse's body and stripes instead of a striped barrel; lying reads as lying; mirror-symmetric stripes from the front | `docs/ANIMALS.md`, `evidence/zebra-scan-2026-11-13.jpg` |
| Penguin (adult and juvenile) from a museum scan and flamingo from a sculpt (both CC BY 4.0), rigged by us | real birds' shapes instead of bowling pins; penguins have `swim` and `float` clips; the adult African plumage is painted on a Magellanic specimen's shape and labelled so | `docs/ANIMALS.md`, `assets/ATTRIBUTION.md`, `evidence/penguin-flamingo-scan-2026-11-13.jpg` |
| Penguins on the water | a resting penguin floats, one under way swims, each with its own clip and depth from the model's manifest (they walked upright through the pool before) | `src/game/AnimalRenderer.cpp`, `evidence/penguin-floating-2026-11-13.jpg` |
| Animals stand in the sun, not in their own shadow | every skinned model was drawn shadowed by itself (an engine defect, `docs/UPSTREAM.md` ZOO-UP-001); lit and shaded now, and normal maps on animals show for the first time | `src/game/SunShadows.cpp`, `evidence/animal-self-shadow-2026-11-13.jpg` |
| Habitat fronts a zoo would build | elephants and rhinos behind a 4 m planted stand-off with a leaning rail, hoofstock behind 2 m; heavy dark mesh instead of bars at bears and cats; chain-link one looks through; nobody walks on the planting | `docs/VISITOR_ZOO.md`, `evidence/habitat-fronts-2026-11-13.jpg` |
| People at every habitat | 192 to 260 of 800 stand watching (was 110 to 215) and no habitat is left without watchers (several were, most of the time) | `docs/VISITOR_ZOO.md` "Who watches where" |
| Trees and shrubs with leaves | every crown and bush was a lumpy green blob; now a dark core that casts the shadow, covered with cut-out sprays of leaves or needles (two painted 512 px atlases, alpha-tested with coverage kept through the mip levels); spruces are tiers of boughs | `src/game/FoliageTextures.*`, `src/game/PropModels.cpp`, `evidence/foliage-2026-11-13.jpg`, `docs/PERFORMANCE.md` |
| Giraffe bull, cow and calf: a scanned head and neck (CC BY 4.0) on a body modelled by us, a painted reticulated coat | a giraffe's silhouette and coat instead of a lofted toy; browsing reaches the crowns | `docs/ANIMALS.md`, `evidence/giraffe-2026-11-14.jpg` |
| Brown and polar bears with cubs from a sculpt (CC BY 4.0), rigged by us | bears with fur, claws and a bear's head instead of blobs on block feet; crude still (angular shoulders, a snarling polar bear) | `docs/ANIMALS.md`, `evidence/bears-sculpt-2026-11-14.jpg` |
| Animals with a species' day | each species has its own shares of resting, feeding, walking and a signature activity (browse, ruminate, bathe, dust bathe, preen, sentinel, dig, filter feed, patrol, huddle), chooses its place in the habitat (shade at midday, high ground for the sentry, the group), and keeps them through a save; lions rest 70 % of the day, elephants move or feed 62 %, a meerkat sentry is posted 85 % of daylight | `docs/ANIMAL_BEHAVIOUR.md`, `evidence/animal-routines-2026-11-13.jpg` |

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
where an animal wades. Penguins swim and float with their own clips now, and polar bears swim with theirs.

## Open (in the order they hurt)

1. Animals: five of fifteen species (wolf, camel, meerkat, ostrich, kangaroo) are still the stylised first generation; the bears are sculpt-based but crude
   (`docs/ANIMALS.md`). The cats are believable now but not photographs (the tiger's cheek
   ruff reads as a grey comb, the mane's cards show close up). No young animal lives in the zoo yet.
2. Habitat fronts: hoofstock is still seen through chain-link behind its strip.
3. Meerkat Lookout's pool is too large and props stand in it.
4. Plants: trees and shrubs have leaves now, but there is no distance form (1.93 million
   of the scenery's 2.18 million triangles are plants), cut-out edges far away are pixelated (no
   alpha-to-coverage), cut-out leaves cast no shadow (the engine's shadow pass skips
   alpha-tested items; the crown's core casts it), and palms, ferns and hedge fences are
   still the old solid shapes. The pines of Bear Ridge stand in an odd dense clump.
5. Keepers are not seen. Only penguin, polar bear, rhinoceros and elephant have swimming clips. Bear growl
   is quiet in the mix.
6. Loading 5.5 s, 2.4 s of it painting the ground.
7. Animals get stuck: 5.4 % of the ticks in which an animal is under way it does not move
   (3.0 % before the species routines), and one tiger stands pinned in the bamboo thicket for
   most of an hour; the navigator does not know what plants and rocks block
   (`docs/ANIMAL_BEHAVIOUR.md` "In the game"). Flamingos keep to the bank because the lagoon
   has no shallows. No model has clips for preening, dust bathing, digging or sentry duty.
