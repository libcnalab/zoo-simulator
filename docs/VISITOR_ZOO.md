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

The seed-1 build contains **74 animals, 14 habitats, 41 buildings and 1,912 props** when every
species is included (the game leaves out a species that has no model, see below). Every
public path and visitor door is reachable; the farthest path tile is **77.1 m walking distance**
from a visitor facility. These totals and the habitat figures are printed by `VisitorZooTests.DISABLED_PrintPlan` (seed 1).
Groups are adult males + females. **S** = south; **W/E** = west/east. Glass fronts occupy S
and the side facing the zoo's centre; the remaining boundary uses the listed base fence.
All sides have accessible viewing walks.

| Habitat | Species and group | Area, m² | Base fence; viewing front |
|---|---|---:|---|
| Meerkat Lookout | Meerkat 1 + 5 | 252 | Wire mesh; glass S/W |
| Flamingo Lagoon | Flamingo 3 + 9 | 796 | Wire mesh; glass S/W |
| Acacia Plain | Giraffe 1 + 3; zebra 1 + 5 | 4,284 | Tall mesh; glass S/W |
| Ostrich Rise | Ostrich 1 + 3 | 1,016 | Tall mesh; glass S/W |
| Lion Rock | Lion 1 + 2 | 2,236 | Steel bars; glass S/W |
| Tiger Forest | Tiger 1 + 1 | 1,788 | Steel bars; high glass S/W |
| Wolf Wood | Wolf 1 + 4 | 1,788 | Tall mesh; glass S/W |
| Bear Falls | Brown bear 1 + 1 | 2,236 | Steel bars; high glass S/E |
| Polar Sound | Polar bear 1 + 1 | 2,812 | Steel bars; high glass S/E; cooled den |
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

Catalog limits: there is no low glass wall and no low rail that would hold a flamingo, so the
small animals stand behind the same 3.5 m glass as the lions. Tigers and bears need a 4 m barrier of strength 4: their viewing fronts are the
4.5 m `high_glass_wall`, the rest of the boundary steel bars.

## The zoo as the game visits it

The game builds this scenario without any species that has no model in `assets/animals/`
(`withoutSpecies`); such a habitat would stand finished but empty. Since 2026-11-12 all fifteen
species have one, so a seed-1 visit has all **74 animals in fourteen habitats**.

The visitor system opens at 09:00. A visit starts at 10:30: the game steps the zoo from the
opening before the first frame, then `Zoo::SpreadVisitors()` sets the groups that have arrived
down over all the paths (a visit on foot would otherwise meet the whole morning's crowd in a
column at the gate), and the visit pace (`VisitorPace`: linger, stay, arrivals) keeps them at the
habitats. Measured at the first frame of a seed-1 visit (`zoo: visit for ...` lines of the game):

| | |
|---|---:|
| Visitors simulated (the cap, `Visitors::kMaxVisitors`) | 800 |
| Walking | 415 |
| Watching animals | 177 |
| Eating | 153 |
| Queueing | 15 |
| Sitting | 10 |
| Reading a board | 8 |
| Habitats being watched | 10 |

The core scenario on its own (no spread, all species, the simulation's default pace) is measured
by the disabled test `VisitorZooTests.DISABLED_MorningAndCrowdFigures`; under AddressSanitizer it
runs for about 1,000 seconds.
