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
  Children pay half at the gate, are shorter and a little slower; one adult in
  eight is old (`Elderly()`, from the visitor's `look`) and walks at four fifths
  of an adult's pace. `PaceOf()` is the pace a visitor walks at on its own.
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
  alarm per game hour. The tick of that alarm is saved, so loading during the
  cooldown cannot repeat it for a second crowd.
- **Litter.** Wrappers go into a bin when one is within `kBinSearchM`, on the
  ground otherwise. Litter in sight lowers happiness. The cleaner empties bins
  on its daytime round, prioritises the nearest bin with ground litter within
  30 m, and refreshes that target when it arrives;
  with `autoCare`, the midnight shift remains a safety net for anything left.

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

Visitors walk on paths only (`NavProfile::Visitor()`). Routes come from the shared `Navigator`;
at most `kPathSearchesPerTick` searches run per tick for all groups together.
When the map changes under a route (`World::NavRevision`), the group plans
again; a destination that can no longer be reached is given up, and a group that
cannot even reach the gate thinks `LostWay` and is taken out after a while.
What the map offers (viewing places, stalls and where their queues stand,
benches, bins, boards, how pleasant each tile is) is derived from the world in
`VisitorSites.cpp` and rebuilt only when a revision changes; it is not saved.

A group remembers up to 64 services whose queue, capacity or price it rejected
during its visit and will not immediately choose any of them again. The complete
list is saved in `avoided_buildings`; the legacy `skip_building` field retains
the most recent one so an older game can still open a new save. Conversely, a
save from before the list existed promotes its single value on load.

Benches and picnic tables stand beside the paths: the last step from the paving
to a seat is a straight line. A bench is offered only when one reachable path
tile has a fence-, water- and boundary-safe straight step to every advertised
seat; a path tile on the other side of a barrier no longer makes it usable.
Arrival and departure routing both use that recorded access instead of independently
choosing a nearer path tile across a barrier. Members that find no seat stand in
front of the bench or at the ends of the table. Choosing an unrelated departure
path once sent a person from the outer end of a picnic table across 19 ticks of
open lawn (`VisitorDayTests.ADayInTheStarterZoo`, 2026-11-19).

## How they walk and stand

`Stepper::Move` (`VisitorActions.cpp`) walks a group as people, not as points on
a line:

- **Pace.** A group walks at the pace of its slowest member. Everybody gains
  speed at `kGainSpeed`, loses it at `kDropSpeed` and brakes for its place with
  `kBrake` (v = sqrt(2 x brake x distance)), so nobody is at full stride in one
  tick or stops dead from it. `Visitor::speed` is the ground it really covers:
  the renderer times the steps by it.
- **Turning.** A walker steers: it turns at `kTurnWalking` rad/s at full stride
  and up to `kTurnOnTheSpot` faster the slower it goes, and walks where it
  looks as long as that crosses no fence and leaves no paving (`Stepper::Clear`);
  otherwise, and for the last `kDirectM`, straight at its waypoint. It rounds a
  corner from `kCornerM` when the way to the next waypoint is open.
- **Formation.** The leader walks the route; the others keep a place beside it
  or in rows behind (`kAbreastM` apart, rows `kRowM` behind one another, two
  abreast), each on its own lane of the route (`Stepper::Lanes`, `kLaneOffset`
  to the side of the waypoints). The leader does not walk away from members who are still
  catching up. A child runs up to `kChildAheadM` ahead now and then and lets the
  others catch up.
- **Giving way.** `Stepper::Dodge` looks `kAheadM` ahead along the tiles the next
  steps reach (per-tile chains of who stands where, rebuilt each tick in
  `BuildFrame`): somebody nearer to the walker's line than `kShoulderM`
  (`kOwnShoulderM` is now the same 0.55 m for its own group) is walked around on
  the side with more room; somebody going the same way is followed and only passed when nearer
  than `kFollowM`; for people coming the other way the walker also slows, never
  below 45 % of its pace. Within `kSettleM` of its own place it normally settles directly,
  but a person who arrived after the route was planned remains a hard obstruction.
- **The player.** `Visitors::SetBystander(position)` names somebody who stands on
  the paths and is no visitor (the game passes the walking player every frame,
  `ClearBystander()` otherwise). Walkers go round it like round a standing
  visitor, with a 0.36 m hard body floor beneath the steering so nobody clips through it.
  It is not saved and not part of `StateHash()`: a zoo that is watched
  from within is no longer bit-identical to one that is not.
- **Staff.** Before the visitor step, the zoo supplies every keeper, gardener and cleaner as a
  transient steering obstacle. A walker gives them an adult's shoulder room, whether the member
  is walking or working, but ignores a member behind a habitat fence. The list is rebuilt every
  tick and is neither saved nor hashed; see `docs/KEEPERS.md` "Where they walk and stand".
- **Standing.** People who stand to eat, read a board or use a bin, and those
  who find no seat, are placed by `Stepper::MakeRoom`: the group's little circle
  is moved as a whole (rings of 0.8 m) to where every member is `kRoomM` from
  everybody already standing or bound for a place there, and from all
  `kLineKeptClear` usable places and intervening segments of a stall's line;
  when that fails, or when the places of the group fell on top of one another,
  each member finds its own.
- **Watching.** Places at a fence are 0.5 m apart, the front row 0.34 m from the
  rail. A group only reserves places which also keep `kRoomM` from unrelated
  people already standing or walking there. If somebody settles inside that
  space later, the watchers leave the crowded place and choose another view.
  `Stepper::Aim` gives each watcher an animal of the enclosure to look at
  (within `kAimSpread` of the straight line across the fence; mostly the one
  its group looks at, sometimes its own; the nearer the likelier), anew every
  `kAimSeconds` at a moment of its own, and the watcher turns to it at
  `kAimEase` times what is left of the turn, never slower than `kAimTurn`.

Because walkers now speed up, brake, turn and give way, the paces are 6 % higher
than they were (`kWalkSpeed` 1.37, `kChildSpeed` 1.22), so that a visit takes as
long as it did.

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
| `kWalkSpeed` (+`kWalkSpeedSpread`), `kChildSpeed` | 1.37 (+0.25), 1.22 m/s | walking |
| `kElderlyShare`, `kElderlyPace` | 0.12, 0.8 | old people: share of the adults, share of the pace |
| `kGainSpeed`, `kDropSpeed`, `kBrake` | 2.4, 4.0, 1.2 m/s2 | speeding up, slowing down, braking for one's place |
| `kTurnWalking`, `kTurnOnTheSpot` | 3.4, +2.6 rad/s | turning at full stride, and more at a standstill |
| `kAheadM`, `kShoulderM`, `kOwnShoulderM`, `kFollowM` | 1.4, 0.55, 0.55, 0.5 m | whom a walker goes round |
| `kAbreastM`, `kRowM`, `kChildAheadM` | 0.62, 0.85, 1.9 m | a group's formation |
| `kRoomM`, `kLineKeptClear` | 0.5 m, 16 | room between people who stand; every occupiable queue place kept free |
| `kAimSpread`, `kAimSeconds`, `kAimEase`, `kAimTurn` | 0.5 rad, 2.5 s, 4 /s, 0.5 rad/s | a watcher's look at an animal |
| `kHungerPerHour` .. `kTirednessPerHour` | 0.06 .. 0.09 | needs per game hour |
| `kUrgent`, `kNotable`, `kHandyM` | 0.62, 0.4, 36 m | when a need is seen to |
| `kWatchSeconds` (+spread) | 8 (+0..6) s | one look at an enclosure |
| `kLooksPerEnclosure`, `kSecondLookM` | 2, 10 m | a second look from another place |
| `kSlotsPerViewingTile` | 12 | people at one viewing tile before it is crowded |
| `kQueueLimit`, `kQueuePatienceSeconds` | 16, 24 s | queues nobody joins, or leaves |
| `kThreatM` | 28 m | a loose dangerous animal this near: run |

Limits: `Visitors::kMaxVisitors` 1200 (800 until 2026-11-14; what a zoo can hold and a save
may contain), `kMaxGroupSize` 5, `kMaxLitter` 1500.

**The crowd setting** (Settings, "Visitors"; `visit.crowd` in the preferences; `--crowd` for
runs that read no preferences): Nobody 0, Few 150, Fewer 500, Normal 800, More 1200 people.
It is `VisitorPace::most`, the most people let in at once, with the arrivals (and those of
the morning's fill) scaled by the same share, so a small crowd is not a full zoo with a queue
at the gate. Normal is the pace the visit was tuned with, unchanged (the golden hashes did not
move). Like the rest of the pace it is a setting, neither saved nor hashed. A smaller crowd
chosen during a visit sends whole groups home at once, those farthest from the player first
(`Visitors::SendHome`); a larger one fills up as people come in at the gate. After the
morning's fill: 0, 150, 500, 800 and 1,200 people (2026-11-14).

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

## How the game shows them

`src/game/VisitorRenderer` draws the at most 18 nearest visitors in view as CNA
avatars and everybody else as flat figures from sheets made at loading (see the
comment at the top of `VisitorRenderer.hpp`). What
makes them people is posed in `src/game/AvatarPose` and chosen in
`VisitorRenderer::Carry`, for the avatars only; all of it follows from the
simulation's clock and the visitor's id and group, so a lockstep run shows the
same people doing the same things:

- **Steps.** The step's length follows the pace (`ApplyGait(..., stride, swing)`:
  stride = sqrt(pace / (1.45 sqrt(scale))) of the body's full stride, between
  0.5 and 1.06, times the person's own 0.92..1.06 or an old person's 0.8..0.94),
  and the phase advances by the ground covered, so feet do not slide; a slow
  walker takes shorter, lower steps with less bob and arm swing. Arm swing is
  the person's own (0.65..1.35; old people 0.45..1.15), old people stoop
  (`ApplyStoop`). The phase starts from the id: neighbours are not in step.
- **Standing.** Weight on one leg for 9 to 18 s at a time, then the other or both
  (`ApplyWeight`); the head looks about, less while watching, more while waiting
  (`ApplyLook`).
- **Talking.** A group talks in spells (7 s of every 11, in 55 % of them): one
  member speaks to another, everybody's head turns to the speaker, the speaker's
  to the one spoken to, and a speaker who stands moves a hand (`ApplyTalk`).
- **At the fence.** An adult in the front row leans on the rail (`ApplyLean`),
  points at what it looks at (`ApplyPoint`), holds up a camera with both hands
  (`ApplyCamera`) or crouches beside a child of its group (`ApplyCrouch`); a
  child points. One of these at a time, for part of a spell of 12 to 20 s.
- **Waiting.** Adults who stand idle, queue or sit look at a phone now and then
  (`ApplyPhone`).

The flat figures show none of this (they have one standing, one sitting and
eight walking pictures per side). The avatar catalogue has no child bodies,
pushchairs, bags or carried objects: children are small adults, hands hold
nothing, and nobody is carried.

`ZOO_VISITOR_DEBUG=1` prints every visitor once (id, group, activity, place,
heading, speed, height) when the first frame with visitors is drawn: for
finding a scene to look at. `ZOO_VISITOR_POSE=lean|point|camera|phone|crouch|old`
makes everybody who could do that one thing do it all the time (`phone` every
standing adult, the others the watchers at a fence, `old` gives every adult the
old people's stoop and step): for looking at one pose, never set in the game.

## Saving

`ZooSave` writes a `"visitors"` section (every visitor with its plan and route,
the litter, the price, the rating, the generator's state) and a visitor count in
the summary. A save without the section loads as a zoo with no visitors, so
saves from before visitors existed still open (the visitors subdocument remains
version 1). Damaged values are refused with a message that starts `visitors:`.
The top-level zoo state also saves `last_scare_note`, the tick which gates the
once-per-hour loose-animal alarm. It is optional for old format-3 documents;
present values before the initial sentinel or after the saved clock are refused.
After loading, a zoo continues tick for tick as the one that was saved
(`VisitorTests.ASaveKeepsEveryVisitorWhereItWas`).

## Determinism and cost

The visitors draw from their own generator seeded from the zoo's seed and are
part of `Zoo::StateHash()`. With about 240 visitors in the starter zoo the whole
zoo steps in about 0.1 ms a tick (release build, 2026-11-12;
`VisitorDayTests.ADayInTheStarterZoo` fails above 1 ms).

Measured in the starter zoo at a $15 ticket (the same test, 2026-11-13): 240
visitors on day one, 1.7 different enclosures seen per group, happiness on
leaving 0.68, 254 purchases, 9 donations, 34 pieces of litter; day two 197
visitors, happiness 0.71. The zoo's balance went from $50,000 to about $53,000
over day one.

Giving way costs a look at the people on the tiles ahead for every walker and
tick; the chains of who stands on which tile are rebuilt each tick.

### Visitors' frame cost (P1, 2026-11-19)

The old rendering path recomputed a visible person's feet as many as three times in one frame.
It also called `DrawUserPrimitives` once for the shared shadow and once for every cast member
whose sprite sheet was visible (usually about thirty calls). CNA made, filled and deleted a GPU
buffer and its vertex array for each call.

The visible-person selection now carries the already computed feet through avatar ranking,
shadow collection and posing. Shadows and flat people each retain a dynamic vertex buffer. All
flat vertices are uploaded once, then the texture runs are drawn from their offsets in that
buffer. This does not change simulation, avatar selection, poses, sprite sides, lighting or
geometry. A standalone comparison of the old vector expressions with the new scalar writes
matched all 44,571,410 coordinates bit for bit. An entrance screenshot with 155 people drawn was
inspected, the 18 game-side Walker/texture tests pass, and an AddressSanitizer entrance run with
the full crowd is clean.

The saved historical before/after binaries could not provide a trustworthy new frame-time result:
the current offscreen renderer ran them at 2.6 fps with 310 ms in its software driver, unlike the
historical GPU measurements in `docs/PERFORMANCE.md`. The result claimed here is therefore
structural: about thirty temporary buffers and all repeated feet calculations are gone, not a
particular fps increase.

## Tests

`tests/core/VisitorWalkTests.cpp` (nine cases: walkers neither snap round nor
jolt and cover the ground their speed says, the members of a walking group are
not inside each other and stay with their leader, watchers look at an animal
and never away from the fence, people who stand keep apart, a live stall queue
waits for an unrelated standing person, watchers keep half a metre from one, a
sitter is on its seat facing out with its neighbours beside it, somebody
standing on the path is walked around, children and old people are slower),
`tests/core/VisitorTests.cpp` (seventeen cases: the gate's hours, the price and
the weather, an empty zoo, determinism, groups, places at the fence and on
seats, saving and loading, a torn-up path, a loose lion and its saved alarm
cooldown, litter and bins, the price limits) and `tests/core/VisitorDayTests.cpp` (two whole days in the
starter zoo with statistics printed, and invariants: nobody off the paths except
at a seat, nobody stuck, everybody gone by night). `VisitorSitesTests` directly
puts a bench across a fence from its only reachable path and proves it is no
longer offered; removing the fence restores all of its seats. A second case gives
the bench a nearer path access across a fence and a verified one around it, then
checks that arrival ends and departure begins at the verified access. The complete
suite passes all 287 active cases, including the 364.69-second animation test; 21
diagnostics remain disabled (J36, 2026-11-20).

J37 adds the saved alarm-cooldown regression described above. All 30 focused
visitor/save/hash cases pass, and the complete suite passes all 288 active cases,
including the 365.89-second animation test; 21 diagnostics remain disabled
(2026-11-20).

J38 removes a stale limitation from this document and makes the existing daytime
cleaning behaviour a regression: a filled public visit produces real litter, the
test observes the exact midpoint at which the cleaner's visible sweep applies,
and the ground-litter count falls while the calendar is still on the same day
(2026-11-20). The complete suite passes all 289 active cases, including the
362.86-second animation test; 21 diagnostics remain disabled.

J39 stops a dissatisfied group oscillating between services it has already
rejected. The leader now keeps a bounded visit-long history rather than only the
last building, every service choice consults it, and all three rejection paths
(queue patience, price and indoor capacity) add to it. A compatible JSON field
preserves the history while old saves and old readers retain their one-building
fallback (2026-11-20). The complete suite passes all 290 active cases, including
the 367.51-second animation test; 21 diagnostics remain disabled.

J40 closes the live gap between a mapped stall line and somebody already
standing beside it. Standing plans reserve the continuous line through all 16
places a queue may occupy, not only eight sampled points; while shuffling, a
whole queue also waits before entering an unrelated stationary visitor. A
6,000-tick regression inspects every tick and keeps active food and drink lines
at least 0.3 m from eating, watching, reading and idle visitors (2026-11-20).
The complete suite passes all 291 active cases, including the 363.81-second
animation test; 21 diagnostics remain disabled.

J41 closes the corresponding late-arrival gap at a viewing rail. A viewing
place is rejected when it lies within `kRoomM` of any unrelated person's
current or reserved destination; because somebody can still settle after that
choice, an active watcher also gives up the crowded place and replans. The
6,000-tick every-frame regression first reproduced a watcher 0.257 m from a
visitor who had just finished eating and now holds every such pair at least
0.5 m apart. All 29 focused visitor/day/walk cases pass (2026-11-20).

J42 keeps J38's visible daytime cleanup reliable under that changed crowd
history. Instead of completing a full bin circuit while fresh litter waits,
the cleaner goes next to the nearest bin whose 25 m catchment contains a piece
and refreshes the saved sweep target before the walk and on arrival. The public
regression removes real litter before midnight again, and all 13 focused keeper
cases pass. The combined J41/J42 suite passes all 292 active cases, including the
366.68-second animation pipeline; 21 diagnostics remain disabled. The changed
short and 6,000-tick hashes repeat twice for seeds 1–3 (2026-11-20).

J43 gives companions the same shoulder room as unrelated walkers instead of a
narrower 0.4 m corridor. The group regression now samples every tick rather
than every tenth: pairs below 0.3 m fall from 1,422 / 98,545 (1.44%) to
562 / 97,096 (0.58%), while the two-day visit still gets everybody home, buys
195/198 items, has no astray ticks and keeps its longest walking standstill at
2.5/1.9 seconds. All 29 focused visitor/day/walk cases pass. The changed short
and 6,000-tick hashes repeat twice for seeds 1–3; the complete suite passes all
292 active cases including its 367.09-second animation pipeline, with 21
diagnostics disabled (2026-11-20).

## Known limits

- Peak presence is close to the day's arrivals: visits are long against the
  opening hours.
- Walkers go round each other, staff and the player. Most people who stand do
  not step aside for anybody; occupied late-arrival destinations are not entered, and a watcher
  is the exception which leaves a viewing place if somebody else settles within half a metre.
- Two walkers of a group can still cross inside 0.3 m for a moment at a corner
  (0.58 % of the every-tick pairs in `VisitorWalkTests`, down from 1.44 %).
