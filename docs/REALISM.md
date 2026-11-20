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
| Visit start runs in the core | fill of 800 visitors 7 s -> 1.2-1.6 s; with 88 animals the prop buckets are no longer rebuilt on every feeding tick (fastest fill 2.52 -> 2.05 s under load) | `docs/VISIT_START.md` |
| Scenery in one cell during a visit | engine time at the entrance 11.7 -> 5.7 ms | `docs/PERFORMANCE.md` |
| At most 18 full CNA avatars a frame | people pass at the meerkats 15.5 -> 8.0 ms | `docs/PERFORMANCE.md`, `evidence/people-*.jpg` |
| Groups at benches | a group sits only if half of it can; the rest stand in an arc; a bench is offered only when every advertised seat has a fence-safe last step from the same reachable path access | `docs/VISITORS.md` |
| Audio audit | 54 licensed recordings with a measured per-take mix and labelled substitutes; calls follow animal activity instead of a timer, day birds fade at night, crowds scale with people, steps match the walked surface, sources have no hard pan or immediate repeat, and the two fountains have their own spatial loop | `docs/AUDIO.md`, `assets/audio/`, `evidence/audio-g7-2026-11-19.jpg` |
| Regression tests | empty habitats, information boards, save/load of a visit, wading depth | `tests/core/` |
| Light from the sky and the ground | below | `src/game/SkyLight.*` |
| Water with ripples | below | `src/game/Surfaces.cpp` |
| Lion, lioness, cub and tiger on a professional sculpt (CC BY 4.0) | real cat anatomy; the lioness has no mane any more | `docs/ANIMALS.md`, `evidence/big-cats-sculpt-2026-11-13.jpg` |
| Visitors walk like people | they steer instead of sliding along a line, each at a pace and stride of its own; groups walk two abreast; walkers give way to oncoming and standing people and to the player; those who eat, read, watch or queue get ground of their own (pairs closer than 0.3 m in a crowd of 800: 81 -> 9), and a watcher yields if somebody later settles within half a metre; watchers lean on the rail, point and turn to follow an animal. Costs about 3 ms a frame with 800 people (`docs/PERFORMANCE.md`) | `docs/VISITORS.md`, `evidence/visitors-g4-2026-11-14.jpg` |
| Buildings, barriers, signs and furniture | clear viewing glass on a kerb instead of milky sheets; welded mesh with an overhang leaning towards the animals and DANGER plates at the big cats and bears; printed signs (ZOO over the gate, TOILETS, DRINKS, ICE CREAM, BURGERS, GIFT SHOP, EDUCATION CENTRE), kiosk counters, gutters, sheet-roofed barns, a ticket lodge, faceted rocks, printed habitat boards and entrance visitor boards with hours, admission, live animal counts and map guidance instead of blank lecterns. Scenery 2.18 -> 2.31 million triangles, 121 -> 130 draw calls; the entrance bench (3 runs each, load 12-17) showed no measurable cost | `src/game/{BuildingModels,FenceModels,PropModels,RockModels,SignTextures,BarrierTextures}.cpp`, `evidence/built-g3-2026-11-14.jpg`, `evidence/entrance-welcome-board-j25-2026-11-20.jpg` |
| Big cats, second pass: the sculpt's own colour and normal maps, a mane of hair cards | the male has a lion's head and a mane instead of a lioness's head in a collar; eyes, nose and lips are the sculptor's; irregular tiger stripes | `docs/ANIMALS.md`, `evidence/big-cats-s7-2026-11-14.jpg` |
| Rhinoceros and elephant on rigged models (CC BY 4.0 as published; authorship unconfirmed, `assets/ATTRIBUTION.md`) | photographic skin with normal maps, real proportions, bull, cow and calves; the elephant's second pass (2026-11-14) made it grey in the game's light with tusks of a believable length | `docs/ANIMALS.md`, `evidence/rhino-elephant-rigged-2026-11-13.jpg` |
| Zebra stallion, mare and foal from a static sculpt (CC BY 4.0), rigged by us | a real horse's body and stripes instead of a striped barrel; lying reads as lying; mirror-symmetric stripes from the front | `docs/ANIMALS.md`, `evidence/zebra-scan-2026-11-13.jpg` |
| Penguin (adult and juvenile) from a museum scan and flamingo from a sculpt (both CC BY 4.0), rigged by us | real birds' shapes instead of bowling pins; penguins have `swim` and `float` clips; the adult African plumage is painted on a Magellanic specimen's shape and labelled so | `docs/ANIMALS.md`, `assets/ATTRIBUTION.md`, `evidence/penguin-flamingo-scan-2026-11-13.jpg` |
| Penguins on the water | a resting penguin floats, one under way swims, each with its own clip and depth from the model's manifest (they walked upright through the pool before) | `src/game/AnimalRenderer.cpp`, `evidence/penguin-floating-2026-11-13.jpg` |
| Animals stand in the sun, not in their own shadow | every skinned model was drawn shadowed by itself (an engine defect, `docs/UPSTREAM.md` ZOO-UP-001); lit and shaded now, and normal maps on animals show for the first time | `src/game/SunShadows.cpp`, `evidence/animal-self-shadow-2026-11-13.jpg` |
| Habitat fronts a zoo would build | elephants and rhinos behind a 4 m planted stand-off with a leaning rail, hoofstock behind 2 m; heavy dark mesh instead of bars at bears and cats; mesh hidden inside a clipped wildlife hedge on the meerkats' service sides; chain-link one looks through; nobody walks on the planting; every tree crown starts behind the full viewing set-back, with low grass and scrub retaining cover at Rhino Hollow and Red Dune | `docs/VISITOR_ZOO.md`, `evidence/habitat-fronts-2026-11-13.jpg`, `evidence/wildlife-hedge-j29-2026-11-20.jpg`, `evidence/open-habitat-fronts-j31-2026-11-20.jpg` |
| Public walks with room to pass | all ordinary paved tiles now belong to a walk at least 4 m wide (48 isolated 2 m tiles on the original plan, three before J30); the deliberately narrow boardwalk remains exempt | `docs/VISITOR_ZOO.md`, `evidence/wide-walks-j30-2026-11-20.jpg` |
| People at every habitat | 192 to 260 of 800 stand watching (was 110 to 215) and no habitat is left without watchers (several were, most of the time) | `docs/VISITOR_ZOO.md` "Who watches where" |
| Trees and shrubs with leaves | every crown and bush was a lumpy green blob; now a dark core that casts the shadow, covered with cut-out sprays of leaves or needles (two painted 512 px atlases, alpha-tested with coverage kept through the mip levels); spruces are tiers of boughs; arm's-length shrubs use finer twig-end sprays and seven visible stems while their far form stays lean | `src/game/FoliageTextures.*`, `src/game/PlantModels.cpp`, `evidence/foliage-2026-11-13.jpg`, `evidence/fine-shrubs-j33-2026-11-20.jpg`, `docs/PERFORMANCE.md` |
| Giraffe bull, cow and calf: a scanned head and neck (CC BY 4.0) on a body modelled by us, a painted reticulated coat | a giraffe's silhouette and coat instead of a lofted toy; browsing reaches the crowns | `docs/ANIMALS.md`, `evidence/giraffe-2026-11-14.jpg` |
| Brown and polar bears with cubs from a sculpt (CC BY 4.0), rigged by us; second-pass resting face and coat | bears with fur, claws and a bear's head instead of blobs on block feet; the residual resting roar is closed, reposed shoulder folds are locally softened, outline fur is shorter and denser, and polar facial pigment is quieter. The converted upright sculpt, polar anatomy and scaled-adult cub bodies remain limitations | `docs/ANIMALS.md`, `evidence/bears-sculpt-2026-11-14.jpg`, `evidence/bear-anatomy-j15-2026-11-20.jpg` |
| The zoo's plan: families with young, barriers a zoo builds, believable habitats | 88 animals instead of 74, every social species with this season's young beside their mothers (lion cubs, an elephant calf in a cow group, a rhino calf, a foal, chicks, pups, joeys at foot), pairs of tigers and bears; hoofstock behind cables and timber paddock fences instead of chain-link; a sand mound with a drinking dish for the meerkats; a lagoon with a 0.3 m shelf where the flamingos wade; feeders in view, nothing in water or on the sloping ground dug around a pool; plants in groves with glades and no trunk before a viewing place; a dry service apron to every keeper gate; animals stalled on their way 5.4 % -> 1.08 % of the moving ticks | `docs/VISITOR_ZOO.md` "The zoo's plan", `evidence/zoo-plan-g8-2026-11-14.jpg` |
| Dry moats and ha-has | Elephant Valley and Rhino Hollow now meet the public across a four-metre stony V-ditch with a 1.8 m centre drop and hidden 2.4 m retaining wall instead of visible cables. The habitat and public path lips, animal areas, welfare and keeper gates are unchanged; the rear service sides retain cable barriers | `docs/VISITOR_ZOO.md`, `evidence/dry-moats-j18-2026-11-20.jpg` |
| The ground and the water's edge | paths, lawns, earth and mulch with soft, wandering borders instead of tile corners; paving as slabs with joints; a detail layer of stones and blades near the eye; dry land at every pool stays dry (it was drawn under water: penguins on land were more than 90 % submerged in 30 % of their samples, the meerkats' dish was a 6 x 4 m pond); banks fall to the bed inside the water along a wandering, rounded waterline; animals, visitors, keepers, props and the player stand on the drawn ground; start 0.6 to 1.0 s shorter, frame up to 0.6 ms dearer, +60 MB | `docs/GROUND.md`, `evidence/ground-g1-2026-11-14.jpg` |
| Palms, ferns, bamboo, flower beds, reeds, tall grass and hedges like the trees; trees by species; a far form for plants | feather palms with leaflet fronds, ringed stems and hanging dead fronds; fern shuttlecocks; jointed bamboo with fans of narrow leaves; flower beds of daisies and umbels; a clipped hedge of leaves on a dark core; twelve dry-margin reed stands around Flamingo Lagoon and walk-through tall grass beneath the open savanna canopy; oaks with flared feet, roots and visible limbs, white birches with lenticels and hanging twigs, weeping willow curtains; 500 broadleaf trees scatter dry leaf litter around their roots; every plant has an opaque part that casts its shadow. Plants are drawn in 16 groves, far ones (beyond 48-56 m) with a quarter of the triangles in two draw calls a grove: triangles a frame at the entrance 4.67 -> 3.29 million, about 1 ms faster (3 interleaved Release runs, load 3-4), no visible switch on a walk | `docs/PLANTS.md`, `src/game/PlantModels.cpp`, `evidence/plants-g2-2026-11-14.jpg`, `evidence/reeds-grass-j8-2026-11-20.jpg`, `evidence/leaf-litter-j9-2026-11-20.jpg` |
| Keepers, a gardener and a cleaner at work | nine people in a green polo, khaki trousers and a bucket hat go about the zoo all day: keepers fill feeders and troughs, scatter feed at two public feedings a habitat (the board says when), muck out, hose and check fences; dangerous groups are atomically assigned to capacity-checked houses and held there for one short rake just inside the service gate, then released only after the keeper exits. The gardener tends the beds; the cleaner prioritises a bin catchment with fresh litter, empties the bin and sweeps the current piece rather than a stale target. The cast is repeatable and the tools cast contact shadows. Automatic care stays behind them; a week records 7 animal-hours in need against 9 without visible staff care | `docs/KEEPERS.md`, `evidence/keepers-g5-2026-11-14.jpg`, `evidence/staff-tool-shadows-j23-2026-11-20.jpg` |
| Staff have bodies; shy animals respond | moving staff slide around visitors, visible animals and one another in the same walking region, while visitors steer around every working or walking member. Species above 0.3 shyness take a clear four-metre retreat when staff enter their scaled flight distance; a direct zebra/flamingo regression distinguishes the response, and the 3,600-tick morning's staff/animal body-clearance approaches fell 75 -> 0 | `docs/KEEPERS.md`, `docs/ANIMAL_BEHAVIOUR.md`, `evidence/staff-personal-space-j19-2026-11-20.jpg`, `evidence/shy-animal-keeper-j20-2026-11-20.jpg` |
| Animals with a species' day | each species has its own shares of resting, feeding, walking and a signature activity (browse, ruminate, bathe, dust bathe, preen, sentinel, dig, filter feed, patrol, huddle), chooses its place in the habitat (shade at midday, high ground for the sentry, the group), and keeps them through a save; lions rest 70 % of the day, elephants move or feed 62 %, a meerkat sentry is posted 85 % of daylight | `docs/ANIMAL_BEHAVIOUR.md`, `evidence/animal-routines-2026-11-13.jpg` |
| Signature activities move | dedicated pipeline clips now show the meerkat standing sentry and digging, adult and young penguins preening and huddling, flamingos preening, huddling and sweeping their inverted bills while filter-feeding, an ostrich dust-bathing, all three elephant variants gathering and tossing dust or wet mud, camels chewing the cud, all three giraffe variants chewing the cud and browsing both sides of a crown, bears, tigers and rhinoceroses wallowing, and all three zebra variants holding a high-headed watch; the exact remaining fallbacks are listed rather than hidden | `docs/ANIMALS.md`, `evidence/animal-clips-a1-2026-11-14.jpg`, `evidence/animal-clips-a2-2026-11-20.jpg`, `evidence/animal-clips-a3-2026-11-20.jpg`, `evidence/animal-clips-a4-2026-11-20.jpg`, `evidence/animal-clips-a5-2026-11-20.jpg` |
| Wolf pup with juvenile proportions | the one-year-old animal at Wolf Ridge now uses a dedicated 0.46 m pup: shorter body and muzzle, broader skull, larger eyes, ears and paws, lighter coat and shorter-stride locomotion instead of a uniformly scaled adult | `docs/ANIMALS.md`, `evidence/wolf-juvenile-j1-2026-11-20.jpg` |
| Grey-wolf double coat and silhouette, adult and pup | an anisotropic tangent-space normal map gives the smooth painted coat guard-hair relief; a fused neck ruff, leaner shoulder transitions and a tapered rough brush replace the cylindrical chest, ball joints and cord tail without increasing triangle count. The procedural anatomy remains stylised | `docs/ANIMALS.md`, `evidence/wolf-coat-j10-2026-11-20.jpg` |
| Bactrian-camel winter coat, adult and calf | anisotropic normal relief replaces a flat painted coat; a fused rough neck mane and forelock, a tapered tail tuft and eight separate dark nails give the two-humped silhouette species-specific detail. The procedural face and underlying anatomy remain stylised | `docs/ANIMALS.md`, `evidence/camel-coat-j11-2026-11-20.jpg` |
| Meerkat short coat and digging feet, adult and pup | a leaner torso and legs replace the barrel body; all four feet now carry four separate toes and non-retractile claws, with long front digging claws, while an anisotropic normal map gives the flat coat short guard-hair relief. Sentry and dig remain grounded | `docs/ANIMALS.md`, `evidence/meerkat-coat-j12-2026-11-20.jpg` |
| Ostrich layered plumage, adult and chick | four overlapping feathers on each folded wing and five separated tail feathers replace fused plume blobs; a feather-only tangent-space normal map adds directional barb relief while keeping the bare skin smooth. The procedural egg body remains stylised | `docs/ANIMALS.md`, `evidence/ostrich-feathers-j13-2026-11-20.jpg` |
| Red-kangaroo short fur, feet and hands, adult and joey | a dense short-fur normal map replaces the flat painted coat; each hind foot has the dominant fourth digit, smaller fifth and tiny syndactylous inner pair, while five separate fingers replace each forepaw mitten. Hop and five-point crawl contacts are unchanged | `docs/ANIMALS.md`, `evidence/kangaroo-coat-j14-2026-11-20.jpg` |
| Tiger cheek ruff | 150 narrow locks with irregular roots and warm cream, sparse black and orange atlas groups replace 72 broad neutral-grey cards in three rows. The alpha-tested fringe remains card hair and the animal remains a conversion of the lioness sculpt | `docs/ANIMALS.md`, `evidence/tiger-ruff-j16-2026-11-20.jpg` |
| Male-lion mane, second hair pass | 2,199 short narrow curved mane locks over the opaque undercoat replace 1,879 long wide shingle-like cards; a denser radial face layer and short laid-back forehead bridge give a compact silhouette within the existing 42,000-triangle model budget. Individual alpha cards remain visible at extreme close range | `docs/ANIMALS.md`, `evidence/lion-mane-j17-2026-11-20.jpg` |
| Meerkat pups with juvenile proportions | both two-month-old animals at Meerkat Lookout now use a dedicated 0.17 m pup: rounder skull, shorter face, larger eyes, ears and paws, lighter coat and pup-scaled locomotion; its sentry and digging clips retain their grounded feet and tail | `docs/ANIMALS.md`, `evidence/meerkat-juvenile-j2-2026-11-20.jpg` |
| Ostrich chicks instead of miniature adult males | all three four-month-old birds at Ostrich Rise now use a dedicated 1.13 m chick: shorter neck and face, larger round head, eyes and feet, compact wings and tail, brown striped down, chick-scaled locomotion and a retained dust-bathing clip | `docs/ANIMALS.md`, `evidence/ostrich-juvenile-j3-2026-11-20.jpg` |
| Camel calves with developing humps | both young animals at Red Dune now use a dedicated 1.26 m calf: a slimmer body, shorter neck and face, larger head, eyes, ears and pads, a lighter coat, low developing humps, shorter-stride locomotion and the retained cud-chewing clip | `docs/ANIMALS.md`, `evidence/camel-juvenile-j4-2026-11-20.jpg` |
| Kangaroo joeys with juvenile proportions | the two joeys below half the age of maturity at Kangaroo Grove now use a dedicated 0.89 m model: shorter face, larger head, eyes, ears and feet, slimmer body, less muscular tail and thighs, a grey-brown coat, joey-scaled crawling and synchronized hopping | `docs/ANIMALS.md`, `evidence/kangaroo-juvenile-j5-2026-11-20.jpg` |
| First-year flamingos instead of miniature pink adults | both ten- and eleven-month-old birds at Flamingo Lagoon now use a dedicated 1.20 m variant: a nearly grown scan silhouette with light grey-pink first-year plumage, grey legs and bill, scaled locomotion and all three dedicated flamingo activity clips | `docs/ANIMALS.md`, `evidence/flamingo-juvenile-j6-2026-11-20.jpg` |
| Resting zebra and elephant poses | the zebra family's grounded sternal rest now breathes, scans, flicks its ears and sways its tail without sliding a hoof; the elephant family's broken rolled-side sleep is replaced by a supported standing doze with lowered head, quiet ears, relaxed trunk and one unweighted hind foot | `docs/ANIMALS.md`, `evidence/resting-poses-j7-2026-11-20.jpg` |

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

Not done about water: the bank wanders inside the water tiles since G1c (`docs/GROUND.md`), nothing splashes or rings
where an animal wades. Penguins swim and float with their own clips now, and polar bears swim with theirs.

## Open (in the order they hurt)

1. Animals: five of fifteen species (wolf, camel, meerkat, ostrich, kangaroo) still use the
   stylised procedural first-generation anatomy. All five now have second-pass coats or plumage
   and species-specific silhouette details, but remain recognisably lofted; the bears now have
   closed resting mouths, softer shoulders and shorter outline fur but remain converted from an
   upright brown-bear sculpt (the polar anatomy and scaled-adult cub bodies are still crude)
   (`docs/ANIMALS.md`). The cats are believable now but not photographs: the tiger's formerly
   grey three-row cheek comb is a finer layered fringe and the lion's long shingle-like mane is
   denser and compact, but both remain alpha-card hair on converted lioness bodies; individual
   mane cards still show at extreme close range.
2. Plants: cut-out edges far away are aliased (no alpha-to-coverage, no MSAA); the scenery still
   costs about 7 ms at the entrance, mostly overlapping
   alpha-tested cards (fill), which the far form does not reduce (`docs/PLANTS.md`).
3. Dangerous-habitat entry now uses a whole-group shelter lockout, but the transfer into the
   houses is immediate rather than animated (`docs/KEEPERS.md` "Open"). Only penguin, polar bear,
   rhinoceros and elephant have swimming clips.
4. Loading about 8 s with the visit's fill on this machine, 1.6 s of it the ground. Banks at the
   deep pools are steep walls (the core digs pools square). The paint still ends square where a
   fence ends or meets a gate.
5. Animals get stuck less since the plan keeps room between blocking props and routes are
   kept off them (1.2 to 1.5 % of the moving ticks, was 5.4 %; a regression test holds it under
   2 %), but the navigator still does not know what plants and rocks block (`docs/ANIMAL_BEHAVIOUR.md` "In the game"). No model
   has a dedicated paired social-contact clip; lion contact still uses the closest standard pose.
   Penguin and flamingo huddling and zebra watch now have dedicated single-animal clips, but
   huddles and social contact are not synchronised between animals.
