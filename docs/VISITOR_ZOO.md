# Greenhill Zoo — visitor plan

A 160 × 160 tile (320 × 320 m) zoo, with free admission and automatic animal care.
North leads into the zoo from the south gate. Entrance coordinates `(east, north)` mean
world tile `(entrance.x + east, entrance.z - north)`; one tile is 2 m. The entrance tile is
`(79,153)`, centred at world `(159,307)` metres. Construct with `VisitorZooConfig(seed)`,
then call `BuildVisitorZoo(zoo, options)`. `withoutSpecies` suppresses animals only.

```text
                              NORTH
             Polar Sound     Bear Falls — Wolf Wood — Tiger Forest
                  |              \    northern court    /       |
             Penguin Cove ======= woodland shortcut ======= Lion Rock
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

The asphalt round bends between habitats; gravel circuits, curved viewing bays and shortcuts
make shorter visits possible. Boardwalks follow the lagoon and Penguin Cove. Habitat outlines
combine elliptical lobes. Every habitat has a keeper gate, shelter, stocked feeders, water,
enrichment, a public information board and benches. Planting concentrates behind the open
viewing ground, with woods between habitats and around the perimeter.

The seed-1 build contains **73 animals, 14 habitats, 41 buildings and 1,912 props**. Every
public path and visitor door is reachable; the farthest path tile is **77.1 m walking distance**
from a visitor facility. These totals and the habitat figures are printed by `VisitorZooTests.DISABLED_PrintPlan` (seed 1).
Groups are adult males + females. **S** = south; **W/E** = west/east. Glass fronts occupy S
and the side facing the zoo's centre; the remaining boundary uses the listed base fence.
All sides have accessible viewing walks.

| Habitat | Species and group | Area, m² | Base fence; viewing front |
|---|---|---:|---|
| Meerkat Lookout | Meerkat 1 + 5 | 252 | Wire mesh; glass S/W |
| Flamingo Lagoon | Flamingo 3 + 9 | 796 | Wire mesh throughout |
| Acacia Plain | Giraffe 1 + 3; zebra 1 + 5 | 4,284 | Tall mesh; glass S/W |
| Ostrich Rise | Ostrich 1 + 3 | 1,016 | Tall mesh; glass S/W |
| Lion Rock | Lion 1 + 2 | 2,236 | Steel bars; glass S/W |
| Tiger Forest | Tiger 0 + 1 | 1,788 | Steel bars throughout |
| Wolf Wood | Wolf 1 + 4 | 1,788 | Tall mesh; glass S/W |
| Bear Falls | Brown bear 1 + 1 | 2,236 | Steel bars throughout |
| Polar Sound | Polar bear 1 + 1 | 2,812 | Steel bars; cooled den |
| Penguin Cove | Penguin 4 + 8 | 1,044 | Wire mesh; glass S/E |
| Elephant Valley | Elephant 1 + 3 | 5,436 | Cable barrier throughout |
| Rhino Hollow | Rhino 1 + 2 | 1,692 | Cable barrier throughout |
| Red Dune | Camel 1 + 3 | 1,616 | Tall mesh; glass S/E |
| Kangaroo Grove | Kangaroo 1 + 4 | 1,136 | Tall mesh; glass S/E |

The entrance has the gift shop, education centre, refreshments, restroom, fountain and statue.
The central food court groups burger, drinks and ice-cream stalls around picnic tables and
acacia islands; another court sits below the northern predators. Restrooms serve the entrance,
central court and northern court. Additional kiosks serve the outer circuits. There are two
fountains, two statues, lamps, bins, flower beds and groves along the walks. Feed reserves cover
at least eighteen hours for the principal species, spanning the 15:00–07:00 care interval.

Catalog limits: there is no low glass wall, no flamingo-safe low rail, and no 4 m glass barrier
for tigers or bears. Safe mesh/bars are used instead. The visitor system opens at 09:00; the
scenario starts at 08:00 with no pre-seeded crowd. Measured seed-1 crowd figures:

| Time | Visitors total / inside | Habitats watched | Sitting/eating | Mean happiness | Rating |
|---|---:|---:|---:|---:|---:|
| 11:00 | 196 / 160 | 1 | 6 | 0.672 | 0.600 |
| 13:00 | 419 / 383 | 3 | 15 | 0.758 | 0.600 |
| 16:00 | 726 / 699 | 4 | 10 | 0.826 | 0.600 |

The five-habitat watching target at 11:00 is unmet with the current visitor clock/planner.
The longest walking standstill was 53 ticks (5.3 game minutes). The diagnostic retains this
acceptance assertion in `DISABLED_MorningAndCrowdFigures`; its ASan run took 1,003 seconds.
