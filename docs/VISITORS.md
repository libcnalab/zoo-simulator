# Visitors

The people who come to the zoo: who comes, what they want, how they get about and
what they think when they leave. The simulation lives in `src/core/zoo/visitors/`
(no CNA; part of `zoo_core`), is stepped by `Zoo::Step()` right after the animals
and is saved with the zoo. The game draws each visitor as a CNA avatar.

## The model in one page

- **Groups.** People come in groups of one to five (families, couples, friends).
  The first member leads: the leader decides where the group goes and holds the
  group's purse; every member walks its own route to its own place at the
  destination (a slot at the fence, a seat on the bench, a place in the queue).
  Children pay half at the gate, are shorter and a little slower.
- **Arrivals.** Each tick the gate expects
  `ExpectedArrivalsPerHour()` people per game hour: a base rate shaped over the
  day, times the zoo's *attraction* (what there is to see), the *price* (against
  `FairTicketPrice()`), the *rating* (the mean happiness of those who left
  lately) and the weather. Groups appear on the approach road, walk to the gate
  and buy their tickets there (`Account::Tickets`).
- **Needs.** Hunger, thirst, bladder and tiredness rise per game hour. A need above
  `kUrgent` is seen to before anything else; one above `kNotable` when what
  satisfies it is within `kHandyM`. A need nothing can be found for is
  remembered (`Visitor::unmet`), costs happiness, and shows as a thought.
- **What they do.** Otherwise the leader picks the most attractive enclosure not
  yet looked at twice (appeal of the animals on show, visibility of the viewing
  place, distance, crowding), walks there and watches. Watching animals that are
  visible and well raises happiness and education; watching an empty or hidden
  enclosure, or animals in poor welfare, lowers it. Between enclosures groups
  read information boards, sit down, eat what they bought at a table or a bench,
  look for a bin for the wrappers (or drop them), and once per visit go to the
  gift shop and the education centre if there is one.
- **Money.** Stalls and shops sell at the price of their `BuildingDef`; a group
  buys when the price is within what its mood makes it willing to pay and its
  purse holds it (`Account::FoodSales`, `Account::GiftSales`). Delighted groups
  sometimes donate on the way out (`Account::Donations`).
- **Leaving.** A group leaves when its visit is over (`kVisitHours` plus spread),
  when the zoo closes, when everything was seen, or when it is fed up
  (mean happiness below 0.18). Its happiness on leaving feeds the zoo's
  `Rating()` (a moving mean over `kRatingMemory` visitors), its education
  `MeanEducation()`.
- **Danger.** A dangerous animal loose within `kThreatM` makes a group run for the
  gate (`VisitorActivity::Fleeing`, event `VisitorScared`); the zoo raises one
  alarm per game hour.
- **Litter.** Wrappers go into a bin when one is within `kBinSearchM`, on the
  ground otherwise. Litter in sight lowers happiness. With `autoCare` the bins
  are emptied and the litter swept at midnight; staff will take that over.

## Time

The simulation runs 20 ticks a second and a game hour lasts 600 ticks: bodies
move in real metres per second, so a game hour is thirty seconds of walking.
Everything a body does (walking, watching, eating) is tuned in motion seconds;
everything slow (needs, the length of a visit, opening hours) in game hours. A
visit of six to eight game hours is therefore three to four minutes of motion:
enough for the starter zoo's three enclosures, a snack and the shop. This
compression is why needs rise slowly per hour and why the gate takes its last
visitors at 16:30 while the zoo closes at 19:00.

## Open and closed

`Visitors::SetOpen(false)` stops admissions: groups on the approach road turn
round at the gate and those inside finish their visit. The gate admits between
`kOpeningHour` (9:00) and `kLastAdmissionHour` (16:30); at `kClosingHour` (19:00)
everybody still inside heads out. Nobody stays the night.

## Getting about

Visitors walk on paths only (`NavProfile::Visitor()`), each in its own lane
`kLaneOffset` to the side of the route. Routes come from the shared `Navigator`;
at most `kPathSearchesPerTick` searches run per tick for all groups together.
When the map changes under a route (`World::NavRevision`), the group plans
again; a destination that can no longer be reached is given up, and a group that
cannot even reach the gate thinks `LostWay` and is taken out after a while.
What the map offers (viewing places, stalls and where their queues stand,
benches, bins, boards, how pleasant each tile is) is derived from the world in
`VisitorSites.cpp` and rebuilt only when a revision changes; it is not saved.

Benches and picnic tables stand beside the paths: the last step from the paving
to a seat is a straight line. Members that find no seat stand in front of the
bench or at the ends of the table.

## The numbers

All in `VisitorTuning.hpp`; the more important ones:

| Constant | Value | Meaning |
|---|---|---|
| `kOpeningHour`, `kLastAdmissionHour`, `kClosingHour` | 9, 16.5, 19 | the day |
| `kVisitHours` + `kVisitSpreadHours` | 6 + 0..2 | longest stay, game hours |
| `kBaseArrivalsPerHour` | 110 | people per game hour at the peak for a perfect zoo |
| `kAttractionScale` | 4.5 | attraction = 1 - exp(-sum(appeal/100) / this) over the species on show |
| `kFairPriceBase`, `kFairPriceRange` | $4, $46 | fair ticket = base + range x attraction |
| `kPriceSensitivity` | 2.2 | arrivals fall by exp(-this x (price/fair - 1)) above the fair price |
| `kRatingMemory` | 300 | visitors the rating remembers |
| `kWalkSpeed` (+`kWalkSpeedSpread`), `kChildSpeed` | 1.3 (+0.25), 1.15 m/s | walking |
| `kHungerPerHour` .. `kTirednessPerHour` | 0.06 .. 0.09 | needs per game hour |
| `kUrgent`, `kNotable`, `kHandyM` | 0.62, 0.4, 36 m | when a need is seen to |
| `kWatchSeconds` (+spread) | 8 (+0..6) s | one look at an enclosure |
| `kLooksPerEnclosure`, `kSecondLookM` | 2, 10 m | a second look from another place |
| `kSlotsPerViewingTile` | 12 | people at one viewing tile before it is crowded |
| `kQueueLimit`, `kQueuePatienceSeconds` | 16, 24 s | queues nobody joins, or leaves |
| `kThreatM` | 28 m | a loose dangerous animal this near: run |

Limits: `Visitors::kMaxVisitors` 800, `kMaxGroupSize` 5, `kMaxLitter` 1500.

## What the player sees of it

- `Rating()`, `MeanHappiness()`, `MeanEducation()`, `Attraction()`,
  `FairTicketPrice()`, `ExpectedArrivalsPerHour()`, `ArrivedToday()`.
- Each visitor's `thought` (hungry, thirsty, nothing to see, too crowded, litter,
  loved an animal, ...): the best clue to what the zoo lacks.
- Events: `VisitorArrived`, `VisitorLeft` (detail: happiness in percent),
  `VisitorBought`, `VisitorDonated`, `VisitorLittered`, `VisitorScared`,
  `VisitorThought`. The day's summary notification names the number of visitors.
- Animals know who watches them: `Zoo` passes `Visitors::Watching(enclosure)` to
  `Animals::SetViewers` once a second (crowds stress shy species).

## Saving

`ZooSave` writes a `"visitors"` section (every visitor with its plan and route,
the litter, the price, the rating, the generator's state) and a visitor count in
the summary. A save without the section loads as a zoo with no visitors, so
saves from before visitors existed still open (`kSaveVersion` stays 1). Damaged
values are refused with a message that starts `visitors:`. After loading, a zoo
continues tick for tick as the one that was saved
(`VisitorTests.ASaveKeepsEveryVisitorWhereItWas`).

## Determinism and cost

The visitors draw from their own generator seeded from the zoo's seed and are
part of `Zoo::StateHash()`. With about 240 visitors in the starter zoo the whole
zoo steps in about 0.1 ms a tick (release build, 2026-11-12;
`VisitorDayTests.ADayInTheStarterZoo` fails above 1 ms).

Measured in the starter zoo at a $15 ticket (the same test): 237 visitors on day
one, 2.1 different enclosures seen per group, happiness on leaving 0.75, 304
purchases, 17 donations, 64 pieces of litter; day two 211 visitors, happiness
0.69. The zoo's balance went from $50,000 to about $53,300 over day one.

## Tests

`tests/core/VisitorTests.cpp` (sixteen cases: the gate's hours, the price and
the weather, an empty zoo, determinism, groups, places at the fence and on
seats, saving and loading, a torn-up path, a loose lion, litter and bins, the
price limits) and `tests/core/VisitorDayTests.cpp` (two whole days in the
starter zoo with statistics printed, and invariants: nobody off the paths except
at a seat, nobody stuck, everybody gone by night).

## Known limits

- Peak presence is close to the day's arrivals: visits are long against the
  opening hours.
- Nothing is swept before midnight, so litter complaints pile up during a busy
  day until cleaners exist.
- A group remembers only the last stall whose queue it gave up on.
- The step from the paving to a seat is not checked against fences.
- The once-an-hour limit on the "animal on the loose" alarm is not saved.
- Visitors do not avoid each other while walking; lanes keep them apart mostly.
