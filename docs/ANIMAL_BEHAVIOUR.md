# Animal behaviour

`Animals::Step` advances living animals in ascending id order. Crated animals do
not advance. Needs and health are updated in `AnimalNeeds.cpp`, activity choices
in `AnimalBehaviour.cpp`, and path following and steering in `AnimalMovement.cpp`.
The simulation owns its `Rng`; animal state, route waypoints, alert latches and the
random stream are saved and included in `StateHash()`.

## Needs and health

Hunger rises by 0.5 per game day and thirst by 1.0, both clamped to 1. A fresh,
accepted feeder is selected when hunger is above 0.4. The meal amount scales with
the animal's body size and daily food requirement. An animal drinks from the
nearest stocked trough when thirst is above 0.45, or from an adjacent dry tile at
open water when no usable trough is available. Feeder food expires according to
its catalog shelf life. Up to four animals can reserve a feeder or trough at once;
animals reserve one of four approach angles in id order.

Alerts are emitted when hunger or thirst crosses 0.9. Each alert bit resets after
that need falls below 0.6. Fatigue rises while awake and falls during Rest and
Sleep. Legacy sleeping animals wake for urgent hunger or thirst above 0.8; profiled
sleepers wake at 0.65 and sentinels are relieved at 0.5. Other activities use
the lower eating and drinking thresholds. Boredom rises with
playfulness and falls during Play, Socialize and
Wander. Stress follows a target from poor welfare, visitor crowds, predators or
being loose; shelter occupancy halves that target. A juvenile younger than one
quarter of maturity age is kept below 0.3 hunger and thirst while its living
mother is in the same enclosure and has hunger below 0.9.

Health damage covers critical hunger or thirst, untreated illness, age beyond an
id-specific lifespan and severe temperature exposure without a suitable heated
or cooled shelter. Otherwise a healthy animal recovers. Health reaching zero
emits `AnimalDied`, increments the death count and removes the animal.

## Routine and decisions

An activity is reconsidered when its duration ends, its target disappears or
empties, or hunger or thirst becomes urgent. Urgent drinking and eating take
priority. Weather sends animals to open shelters that fit the enclosure; cool
shelters are required for heat. Species that need shelter sleep there at night
when capacity is available (night alone does not send an animal in: it goes when
it is tired). Sleep timing uses a day/night bias, with a midday
rest period for crepuscular species. Loose animals roam nearby public tiles,
drink at open water and keep accumulating needs.

The fifteen shipped species use the internal table in `AnimalProfile.cpp`.
Unknown species ids still use the original weighted routine; the species JSON
schema and loader are unchanged. Table weights and all times and distances below
are **game-design estimates**, intended to produce plausible ethograms, not
claims about measured wild or captive populations. Rest includes awake rest;
Sleep remains a separate need-driven activity. Giraffe resting choices become
standing rumination, and flock resting choices become huddling.

| Species | Rest/feed/walk/signature weights | Signature | Spread (m) | Awake still bound (motion s) |
|---|---|---|---:|---:|
| Lion | 18/0/2/1 | social contact | 5 | 18 |
| Tiger | 12/0/3/1 | bathing | solitary | 18 |
| Elephant | 1/10/7/2 | dust, mud in heat | 7 | 14 |
| Giraffe | 3/9/3/4 | browse, rumination | 8 | 16 |
| Zebra | 2/12/4/1 | grazing, head-up watch | 7 | 12 |
| Penguin | 3/0/7/4 | land/water commute, preening | 3 | 10 |
| Meerkat | 2/8/4/5 | digging, rotating watch | 4 | 8 |
| Flamingo | 3/12/2/4 | shallow filter feeding, preening | 2 | 12 |
| Brown bear | 4/9/4/3 | enrichment, bathing in heat | solitary | 15 |
| Wolf | 10/0/5/2 | boundary patrol | 5 | 12 |
| Rhino | 5/10/3/3 | shallow wallowing | solitary | 18 |
| Camel | 4/10/4/3 | rumination | 7 | 16 |
| Ostrich | 2/11/6/2 | dust bathing | 6 | 10 |
| Kangaroo | 6/10/3/1 | social rest, dusk grazing | 5 | 14 |
| Polar bear | 4/2/5/5 | bathing/swimming | solitary | 15 |

Lions, tigers, wolves, brown bears, rhinos and kangaroos receive a twilight
activity multiplier of 2.4 from 05:00–08:00 and 17:00–21:00, 0.45 at
11:00–16:00, and 0.8 otherwise. Midday rest weights multiply by 1.5. The other
profiles have staggered night sleep windows derived from their existing sleep
need, capped at ten game hours; fatigue and urgent needs can still change sleep.
A resting bout is 20–100 motion seconds with a cubic, bounded long tail; other
stationary routines last 4 seconds plus a cubic tail bounded by the species'
awake still limit. These are bout lengths, not guaranteed percentages: travel,
food, water, weather, age and health affect the measured budgets.

Habitat choices sample at most 112 points: a fixed lattice, nearby offsets,
favorite sites, group positions and points outside tree trunks. Sites are checked
against enclosure boundaries, water depth, fences, blocking props and companions.
Each animal has stable favorite areas derived from its id and enclosure id;
non-periodic hashed choices revisit these areas, producing recurring routes.
Rest favors shade at midday and in heat; cold mornings favor unshaded sites.
Sentinels and resting cats favor high ground. Browsing favors foliage. Digging
and dust bathing favor dirt and sand. Bathing seeks water, with shallow limits
for wallowing and filter feeding; absent suitable water, it falls back to a dry
routine. Penguins alternate land and water destinations during commuting bouts.
Large crowds bias shy animals away from the first registered viewing front.
This is an enclosure-level proxy: `Env` supplies no individual visitor or keeper
positions, and no keeper feeding appointment. Gate anticipation is therefore
not implemented; a future feeding-time/keeper-position input belongs in `Env`.
Rain and dangerous temperature shelter priority remain ahead of routines.

The lowest-id visible adult female leads a social group, falling back to the
lowest-id adult male. Shared, hashed stimuli vary by motion-time epoch; id offsets
and different bout lengths spread the starts. Followers choose spaced positions
around the leader's destination and regroup when they stray. Adjacent resting
pairs can lie close; adult male rivals leave additional space. Young follow their
living mother, rest beside her, and can seek other juveniles for play. Maternal
food dependence retains the existing needs rule. This is a local group model,
not a hierarchy, pair-bond or kinship simulation.

Meerkats and zebras rotate adult watch duty every 45 motion seconds in daylight.
The outgoing sentinel stays until its replacement is posted. Duty can defer
ordinary food/water interruption to 0.5, but never critical needs. Only adults
with both needs below 0.45 are eligible to take over. A lone animal
is not assigned group watch. Lions and wolves call at dusk; companions answer
once after different short delays. A held visitor calendar does not cause a
call every tick: call cooldowns and answers use the motion clock.

Awake stationary animals glance toward companions and, when supplied, a viewing
front. Attention offsets vary, and a bounded heading shift also covers repeated
resting bouts. Heading and position together define the measured motionless
bound; this does not force a resting lion to stand up every few seconds.
Unsuccessful need searches have a two-second retry interval instead of repeatedly
restarting activities every tick. Foraging never raises already-satisfied hunger.
Same-species carnivores no longer count as predatory threats to each other.
Enrichment lowers the probability of stress pacing; purposeful wolf patrol is
separate from stereotypic pacing. Low containment escape and recapture rules
remain in effect.

## Life events

Illness checks run once per game hour. Risk starts at `kIllnessBaseChancePerDay` and
adds `kIllnessPoorWelfareChancePerDay` times how far welfare is below
`kIllnessWelfareLevel` (nothing above it), `kIllnessDirtyChancePerDay` times enclosure
dirt, `kIllnessUrgentNeedChancePerDay` for hunger and for thirst above
`kIllnessNeedLevel` (going without, not the appetite before a meal),
`kIllnessTemperatureChancePerDay` scaled by how far the weather is outside the species'
range (full at `kIllnessTemperatureSpanC` degrees; not inside a shelter),
`kIllnessOldAgeChancePerDay` after `kSicknessOldAgeFraction` of lifespan, and
`kIllnessContagionChancePerDay` when a sick same-species animal shares its enclosure.
A pre-F4 starter-zoo measurement found about one illness per animal in half a year
(`ZooSeasonTests`, disabled because it is slow: ninety days, 13 illnesses among 12 to
27 animals, 15 births, no deaths). That historical run has not been re-recorded
for the independent health rolls introduced here.
Illness and untreated-recovery outcomes use separate stateless hashes of animal
id, birth tick and motion hour. Routine RNG consumption therefore cannot reroll
health. The risk formula is unchanged, and these inputs already exist in old saves.
The daily risk is divided across `kLifeChecksPerGameDay` hourly rolls. A sick animal cannot play, mate,
or fight. Treatment recovers it after `kTreatedRecoveryDays`; an untreated animal
can recover after `kUntreatedRecoveryMinimumDays` when welfare is at least `kUntreatedRecoveryWelfare`,
with `kUntreatedRecoveryChancePerHour` per hour. Untreated illness continues to damage
health through the ordinary health update.

An adult healthy female may conceive when she and an adult healthy male that is allowed
to breed share an enclosure. Welfare must meet `kBreedingWelfare`; conception risk is
`kConceptionChancePerDay` per day, divided across `kLifeChecksPerGameDay` hourly checks.
The female must be younger than
`kBreedingMaximumAgeFraction` of lifespan and meet the species' birth interval. The
enclosure must have room for `litterMin` young under `groupMax`. Gestation uses the
species' `gestationDays` converted by `kGameDaysPerRealDay`. Births wait while the mother
is crated. Litter size is uniform from `litterMin` to `litterMax`; each young animal has
age zero, random sex, parent ids, and the mother's home. `kStillbirthBaseChance` plus
`kStillbirthPoorWelfareChance` times missing welfare determines whether the whole litter
is lost. A surviving litter increments `Births()` once per young. Pregnancy multiplies
the mother's hunger rise by `kPregnancyFoodNeedMultiplier`.

An enclosure can trigger one fight per game hour when its species exceeds `maxAdultMales`
or `groupMax`. The daily probability is `kFightChancePerDay` times aggression and
`1 + kFightExcessMultiplierPerAnimal` times the amount over the limit, divided across
`kLifeChecksPerGameDay` hourly checks. Sick, sleeping, sheltered,
crated, and juvenile animals are excluded. Damage starts at `kFightHealthDamage` and
scales by the opponent's adult mass divided by the fighter's; each fighter gains
`kFightStress` stress. A fighter reduced to zero health dies through the normal injury
death path.

A dangerous carnivore or omnivore may hunt a different, lighter species in the same
enclosure when hunger exceeds `kPredatorHungerThreshold`, while awake and within
`kPredatorAttackDistanceM`. Its hourly attack chance is `kPredatorAttackChancePerHour`
times hunger. Success is `kPredatorBaseSuccessChance` minus `kPredatorMassSuccessPenalty`
times the prey-to-predator mass ratio, plus `kPredatorHealthSuccessBonus` times missing
prey health and `kPredatorJuvenileSuccessBonus` for juveniles. A successful hunt emits
`AnimalKilled` and `AnimalDied` with
`DeathCause::Killed`, removes the prey, and lowers predator hunger by
`kPredatorHungerDrop`. A failed attack removes `kPredatorFailedWound` health and adds
`kFightStress` stress. The same rule applies to a loose predator standing inside an
enclosure. The hourly life-event pass runs at the game-hour boundary.

## Movement and world edits

Animal positions and speeds use metres and metres per motion second. Direct
walkable routes are preferred; otherwise at most eight A* searches per step use
the animal's swimming profile and a 6000 expansion limit. Non-swimmers
route around all water, including shallow shores. Speed
accelerates gradually and brakes only on the last leg of a route; heading turns
at a capped rate.

Obstacles are rounded, not stopped at. Each tick the planned step is *slid*
along the clearance circle of every blocking prop and of every other animal it
would enter: the inward part of the step is removed and the tangential part
kept; a head-on approach turns to one fixed hand so that two animals meeting
pass each other. Between props it is the width of the body that has to fit
(`kBodyWidthFraction` of the body radius, which is half its length). The same
physical width determines companion clearance. A step still entering a companion
after sliding is stopped and the existing stall/detour recovery takes over;
companions are not ignored on final approach to a feeder. A target prop's solid
radius remains an obstacle even while the animal approaches its use position.
Play runs are limited to a rare, three-second initial burst; ordinary travel
walks, and patrols use at most 1.5 times walking speed. The combined attention
and movement heading change is capped once per tick.

A walk that makes no progress is noticed: the distance left to the waypoint
must shrink by `kStallProgressM` within `kStallTicks`. When it does not, an
animal that is already near enough (a tree stands on the very spot) counts as
arrived; otherwise it takes a sidestep, a waypoint a few metres to one side
that is checked walkable and clear of props, each try further round than the
one before and replacing the previous one. After `kDetourLimit` sidesteps, when
a step would cross a fence that was built meanwhile, or after
`kWalkGiveUpTicks` on one errand, the errand is given up and the animal decides
anew.

Targets are checked clear of blocking prop circles, and candidates near a fence
are rejected when the body radius would overlap the edge. A tile that becomes
impassable relocates the animal to the nearest passable tile in the same region.
Shelters use the passable front tile of an open animal shelter: an animal whose
errand ends there and that has come within `kShelterDoorM` of it goes in; its
position remains on the front tile while its `shelter` id hides it from view.
It stays while it sleeps or while the weather that sent it in lasts (rain above
`kRainLeaveThreshold`, snow, a temperature outside its range by more than
`kShelterLeaveTemperatureC`); pressing hunger or thirst brings it out. The
thresholds for going in are higher than those for coming out, so a shower does
not send the group in and out. Removed, closed or invalid shelters release the
animal on the next step.

Socialising walks to the nearest visible companion of the same species in the
enclosure and stops a body apart from it, not on top of it.

## Saved state and renderer contract

New per-animal JSON fields are `next_need_check`, `answered_call_tick`,
`last_call_tick`, `attention_heading`, `attention_left`, `still_ticks`, and
`following`. They are all hashed and included in equality. Missing fields default
to inactive values, so old saves load. Removing an animal clears follower
references. Favorite locations, leader election and watch rotation are pure
functions of saved state, ids and supplied time; they need no extra cache or RNG.
Viewer counts are external inputs as before. The new routines use fixed-size
candidate arrays and existing scratch buffers, with no new per-tick heap buffer.
Navigation retains its existing route allocations and bounded A* work; group
scans remain local to the enclosure. Very large single enclosures still inherit
the existing pairwise neighbour work and are not a strict constant-cost case.

Enum values 0–14 and their names are unchanged. Appended activities are below.
When `moving`, render locomotion first; once arrived, use the activity pose.
`src/game/AnimalRenderer.cpp` has deliberately not been changed.

| Activity / save name | Renderer action still needed |
|---|---|
| `Browse` / `browse` | Raised-head foliage feeding; giraffe neck extended, not ground grazing. |
| `Ruminate` / `ruminate` | Standing idle body with slow chewing, without lying down. |
| `Bathe` / `bathe` | Shallow-water wallow/splash/roll; deep water already selects swim from position/depth. |
| `DustBathe` / `dust_bathe` | Elephant trunk dust toss; ostrich or other species roll/scrape dry substrate. |
| `Preen` / `preen` | Bird bill grooming feathers with small head turns. |
| `Sentinel` / `sentinel` | Upright meerkat scanning; zebra head-up watch. Use heading as attention direction. |
| `Dig` / `dig` | Meerkat forepaw digging and nose investigation. |
| `FilterFeed` / `filter_feed` | Flamingo lowered head/bill in shallow water with small sweeps. |
| `Patrol` / `patrol` | Existing walk/trot chosen by speed; no stereotypic pacing cue. |
| `Huddle` / `huddle` | Upright flock rest; flamingos on one leg. Use the same standing sleep pose for flamingo `Sleep`. |
| `following == mother` | Juvenile follows its living mother; optional calf/joey following gait or proximity interaction. |
| `following != mother`, non-null | Juvenile playmate; optional paired play gesture while `Play`. |

No `inWater` flag is added. The renderer already computes afloat state from
`World::WaterDepthAt(position)` relative to scaled shoulder height and selects a
swim clip. Water visits now exercise that path. An animal merely assigned `Bathe`
is still walking to water when `moving`; its target is not proof it is in water.
Current rendering falls back to idle for the new stationary activities, so the
behavior is observable spatially before these dedicated clips are added.

## Measurement and validation

`AnimalEthogramTests.cpp` contains a reusable day/week helper. It measures every
tick at 20 Hz: all activity shares (including travel assigned to that activity),
moving-or-foraging share, longest unchanged awake position **and heading**,
mean distance from group centroid, mean nearest-neighbour distance, used tile
fraction, changes per animal per game hour, daylight posted-sentinel coverage,
and welfare. Sleeping and hidden shelter occupants are excluded from the awake
motionless metric. All random sources are seeded.

The controlled fixture uses up to four adult females (limited by groupMax),
23×23 habitat tiles, appropriate terrain, a shallow shore and deeper pool,
a tree, separate stocked feeders/troughs and hourly cleaning/refills. It has mild
constant weather, no visitors, no breeding, and no automatic treatment. Small
flocks are below the catalog's ideal group minimum; the benchmark measures
behavior in this stated fixture rather than asserting ideal habitat welfare.

The comparison uses identical definitions under unknown `legacy_` ids to select
the retained original routine. Both runs include the corrected foraging hunger,
same-species stress, movement safety, independent health rolls and failed-need retry rules. Thus “before”
means **legacy routine under the same corrected foundations**, not a byte-for-byte
execution of the original revision. An initial original-binary trial was rejected
as a benchmark because its fence could not contain elephants/rhinos and its pool
surface/depth were inconsistent. No invalid trial figures appear in the final table.

The week comparison checks mean hunger/stress, critical-need exposure, health,
deaths and escapes. End-of-week hunger is also reported, but a meal's phase makes
a strict single-tick hunger comparison misleading. Mean need/stress tolerances
are 0.05, health tolerance 0.01 and critical exposure tolerance 0.01; these are
regression tolerances, not biological claims. The week test also requires no more
sick animals than the legacy comparison. Independent health rolls make that
comparison meaningful even when routine RNG consumption differs. The exhaustive tables
and week comparison are opt-in tests; focused acceptance checks run normally.

Reproduce with `ASAN_OPTIONS=detect_leaks=0`:

```sh
./build-asan/tests/zoo_core_tests --gtest_filter='AnimalEthogramTests.DISABLED_MeasureAllSpecies:AnimalEthogramTests.DISABLED_MeasureLegacy' --gtest_also_run_disabled_tests
./build-asan/tests/zoo_core_tests --gtest_filter='AnimalEthogramTests.DISABLED_MeasureKeptWeek' --gtest_also_run_disabled_tests
./build-asan/tests/zoo_core_tests --gtest_filter='VisitStart.*Hashes*' --gtest_also_run_disabled_tests | grep ^HASH
```

The VisitStart golden file is outside this task's owned files. New observed
hashes are recorded below for the lead to merge; that golden test remains a
known expected failure until the lead updates `tests/core/VisitStartTests.cpp`.

| Fill ticks | Seed | Hash after fill | Hash after 500 more ticks |
|---:|---:|---:|---:|
| 600 | 1 | 10998775364161072672 | 18387168612943987149 |
| 600 | 2 | 16292211849449288115 | 12618285241167332137 |
| 600 | 3 | 13322387952710669751 | 575973089597162780 |
| 6000 | 1 | 4002593552014394578 | 1664237560221978575 |
| 6000 | 2 | 12651890709020842810 | 18259073505953101266 |
| 6000 | 3 | 1277936308711455298 | 4278679985693691179 |

The explicit hash-recording command ran two tests in 820.240 seconds; both
failed against the old golden values, as expected. These values belong to this
tree before the other agents' work is merged and must be recorded again after
integration.

### Measured game day (seed 831)

Percentages include activity-assigned travel. Resting combines Rest, Sleep,
Ruminate and Huddle. Active is the union of movement and foraging (Forage, Browse,
Dig, FilterFeed); these columns overlap and are not a partition. Rounded to 0.1%.

| Species | Resting % before → after | Active % before → after | Awake still s before → after | Neighbour m before → after |
|---|---:|---:|---:|---:|
| brown_bear | 41.4 → 65.5 | 49.6 → 34.0 | 86.0 → 15.0 | 16.78 → 9.81 |
| camel | 46.8 → 49.6 | 47.3 → 43.7 | 69.0 → 16.0 | 13.47 → 4.11 |
| elephant | 31.3 → 21.5 | 53.7 → 62.0 | 85.0 → 14.0 | 10.41 → 6.39 |
| flamingo | 55.4 → 39.1 | 39.6 → 44.8 | 54.0 → 12.0 | 10.48 → 4.02 |
| giraffe | 40.0 → 41.8 | 49.7 → 50.7 | 89.0 → 16.0 | 11.70 → 4.35 |
| kangaroo | 51.1 → 58.5 | 39.9 → 35.9 | 83.0 → 14.0 | 10.71 → 2.97 |
| lion | 63.2 → 69.8 | 29.8 → 27.3 | 88.0 → 18.0 | 14.53 → 3.73 |
| meerkat | 51.8 → 46.0 | 39.9 → 32.5 | 22.0 → 8.0 | 9.90 → 3.19 |
| ostrich | 53.7 → 35.6 | 37.8 → 52.7 | 48.0 → 10.0 | 13.56 → 3.43 |
| penguin | 54.6 → 49.1 | 45.6 → 39.9 | 69.0 → 10.0 | 16.96 → 2.54 |
| polar_bear | 39.9 → 64.8 | 56.6 → 34.2 | 85.0 → 15.0 | 16.20 → 8.71 |
| rhino | 44.7 → 60.9 | 48.7 → 39.4 | 80.0 → 18.0 | 13.56 → 7.08 |
| tiger | 70.8 → 78.8 | 24.8 → 25.7 | 76.0 → 18.0 | 22.04 → 11.99 |
| wolf | 50.1 → 62.8 | 41.0 → 28.1 | 69.0 → 12.0 | 11.56 → 2.80 |
| zebra | 55.3 → 39.4 | 31.5 → 35.7 | 71.0 → 12.0 | 16.23 → 4.25 |

The meerkat group had a stationary sentinel for **85.5% of daylight**.
Lions spent **69.8%** in Rest/Sleep, elephants **62.0%** moving or foraging,
and flamingos averaged **4.02 m** to the nearest neighbour. All fifteen species
passed their awake motionless bounds. Habitat use decreases with favorite sites
and cohesion; covering more tiles is not intrinsically better for a resting pride
or flock.

| Species | Spread m | Tiles used % | Activity changes / animal / game hour |
|---|---:|---:|---:|
| brown_bear | 7.70 | 23.4 | 0.778 |
| camel | 4.17 | 30.8 | 0.688 |
| elephant | 5.66 | 41.2 | 0.802 |
| flamingo | 4.54 | 19.8 | 0.688 |
| giraffe | 4.35 | 28.0 | 0.854 |
| kangaroo | 3.04 | 24.4 | 0.615 |
| lion | 3.85 | 26.7 | 0.667 |
| meerkat | 3.32 | 20.8 | 0.615 |
| ostrich | 3.69 | 25.3 | 0.781 |
| penguin | 2.76 | 20.4 | 0.438 |
| polar_bear | 6.96 | 26.5 | 0.653 |
| rhino | 7.79 | 31.8 | 0.677 |
| tiger | 6.00 | 17.8 | 0.438 |
| wolf | 3.14 | 31.8 | 0.802 |
| zebra | 4.43 | 28.5 | 0.854 |

Full nonzero activity budgets (%):

| Species | Legacy routine | Species routine |
|---|---|---|
| brown_bear | idle 3.6, wander 23.8, forage 8.4, eat 6.2, drink 6.0, rest 13.6, sleep 27.8, play 0.5, socialize 1.1, vocalize 8.9 | idle 0.2, wander 5.0, forage 11.1, eat 5.7, drink 6.8, rest 24.8, sleep 40.7, play 5.7 |
| camel | idle 0.1, wander 26.1, forage 9.4, eat 4.4, drink 5.9, rest 13.2, sleep 33.6, vocalize 7.4 | wander 9.0, forage 20.4, eat 4.7, drink 7.9, rest 12.4, sleep 27.9, play 4.5, socialize 3.9, ruminate 9.3 |
| elephant | idle 7.2, wander 30.2, forage 11.9, eat 3.5, drink 4.5, rest 20.0, sleep 11.3, play 1.0, socialize 4.1, vocalize 6.2 | idle 0.9, wander 22.0, forage 33.8, eat 4.7, drink 5.4, rest 2.3, sleep 19.2, socialize 7.4, dust_bathe 4.3 |
| flamingo | idle 3.8, wander 17.6, forage 5.1, eat 5.4, drink 5.4, rest 8.2, sleep 47.2, socialize 1.6, vocalize 5.7 | wander 18.5, eat 9.0, drink 10.8, sleep 30.4, socialize 7.2, preen 3.5, filter_feed 12.0, huddle 8.7 |
| giraffe | idle 6.9, wander 27.0, forage 11.1, eat 3.4, drink 6.5, rest 19.6, sleep 20.4, socialize 2.7, vocalize 2.4 | wander 13.0, eat 6.2, drink 7.0, sleep 19.7, play 5.2, socialize 4.1, browse 22.8, ruminate 22.1 |
| kangaroo | idle 4.0, wander 17.5, forage 10.7, eat 5.0, drink 4.3, rest 6.8, sleep 44.3, socialize 3.3, vocalize 4.1 | wander 2.6, forage 16.9, eat 4.4, drink 6.8, rest 16.2, sleep 42.3, socialize 10.8 |
| lion | idle 4.8, wander 14.9, eat 6.0, drink 3.8, rest 15.5, sleep 47.7, play 1.4, socialize 2.2, vocalize 3.7 | idle 3.5, wander 6.9, eat 4.8, drink 5.9, rest 37.3, sleep 32.5, play 2.2, socialize 6.4, vocalize 0.4 |
| meerkat | idle 5.2, wander 12.9, forage 7.0, eat 9.2, drink 7.7, rest 0.3, sleep 51.5, play 0.9, socialize 4.1, vocalize 1.3 | idle 2.9, wander 6.2, forage 2.5, eat 7.3, drink 11.7, rest 0.7, sleep 45.3, socialize 3.4, sentinel 15.1, dig 5.0 |
| ostrich | idle 3.4, wander 17.0, forage 9.2, eat 4.3, drink 3.9, rest 6.5, sleep 47.2, socialize 2.4, vocalize 6.0 | wander 16.4, forage 25.3, eat 4.3, drink 7.8, rest 3.1, sleep 32.5, play 3.1, socialize 3.1, dust_bathe 4.4 |
| penguin | idle 1.1, wander 20.1, eat 5.3, drink 9.7, rest 15.7, sleep 38.9, play 2.7, socialize 1.2, vocalize 5.4 | wander 17.3, eat 7.3, drink 12.5, sleep 48.7, play 3.8, socialize 4.3, preen 5.6, huddle 0.4 |
| polar_bear | idle 6.3, wander 25.7, eat 4.3, drink 8.9, rest 30.2, sleep 9.7, play 0.6, socialize 1.6, vocalize 12.7 | wander 8.2, forage 1.1, eat 7.0, drink 7.0, rest 34.0, sleep 30.8, play 0.5, bathe 11.4 |
| rhino | idle 2.6, wander 18.5, forage 15.6, eat 4.8, drink 6.3, rest 23.9, sleep 20.8, socialize 2.6, vocalize 5.1 | wander 3.2, forage 18.1, eat 6.7, drink 7.0, rest 26.9, sleep 34.0, bathe 4.2 |
| tiger | idle 0.1, wander 19.7, eat 4.1, drink 3.0, rest 15.5, sleep 55.3, vocalize 2.3 | wander 5.6, eat 4.0, drink 5.9, rest 78.8, bathe 5.7 |
| wolf | idle 4.5, wander 26.9, eat 5.4, drink 5.5, rest 14.4, sleep 35.7, socialize 2.6, vocalize 5.0 | idle 6.8, wander 6.4, eat 4.7, drink 7.1, rest 62.8, play 2.5, socialize 4.8, vocalize 0.4, patrol 4.5 |
| zebra | idle 7.2, wander 18.5, forage 5.8, eat 3.2, drink 4.6, rest 5.4, sleep 49.9, socialize 1.5, vocalize 4.1 | idle 3.0, wander 6.9, forage 18.2, eat 5.6, drink 6.1, rest 0.7, sleep 38.7, socialize 4.2, sentinel 16.6 |

### Measured kept week (seed 831)

All fifteen paired week checks passed. No measured animal died or escaped;
all ended at health 1.0, with zero sick animals in either control or new routine.
These results are from the stated fixture and seed, not a guarantee for arbitrary
habitats. Critical need means hunger or thirst above 0.75.

| Species | Mean hunger before → after | Mean stress before → after | Critical exposure % before → after | Final hunger before → after |
|---|---:|---:|---:|---:|
| brown_bear | 0.249 → 0.249 | 0.239 → 0.238 | 1.3 → 0.0 | 0.339 → 0.468 |
| camel | 0.232 → 0.237 | 0.198 → 0.196 | 1.0 → 0.0 | 0.407 → 0.026 |
| elephant | 0.213 → 0.222 | 0.201 → 0.200 | 0.2 → 0.0 | 0.252 → 0.155 |
| flamingo | 0.252 → 0.240 | 0.242 → 0.242 | 0.6 → 0.0 | 0.486 → 0.283 |
| giraffe | 0.228 → 0.232 | 0.202 → 0.201 | 0.3 → 0.0 | 0.339 → 0.073 |
| kangaroo | 0.234 → 0.243 | 0.196 → 0.192 | 3.6 → 0.0 | 0.112 → 0.464 |
| lion | 0.261 → 0.217 | 0.212 → 0.206 | 7.1 → 0.0 | 0.374 → 0.391 |
| meerkat | 0.237 → 0.247 | 0.197 → 0.197 | 1.8 → 0.0 | 0.422 → 0.326 |
| ostrich | 0.247 → 0.231 | 0.195 → 0.194 | 0.6 → 0.0 | 0.335 → 0.032 |
| penguin | 0.249 → 0.254 | 0.235 → 0.234 | 3.3 → 0.0 | 0.308 → 0.416 |
| polar_bear | 0.225 → 0.237 | 0.224 → 0.224 | 0.0 → 0.0 | 0.159 → 0.018 |
| rhino | 0.234 → 0.239 | 0.205 → 0.204 | 0.9 → 0.0 | 0.266 → 0.041 |
| tiger | 0.251 → 0.231 | 0.243 → 0.237 | 5.2 → 0.0 | 0.052 → 0.298 |
| wolf | 0.222 → 0.215 | 0.202 → 0.201 | 0.5 → 0.0 | 0.254 → 0.188 |
| zebra | 0.239 → 0.239 | 0.198 → 0.197 | 0.1 → 0.0 | 0.233 → 0.021 |

The literal stronger condition “no animal is hungrier on the final tick” is
**not established**: brown bears and penguins, for example, finish at a different
point in their feeding cycle. The regression gate uses the week-long need and
stress measures stated above, and explicitly checks health and illness. This is
a sampling-policy deviation from a literal final-tick reading of the brief.

### Integration limitations

#### In the game (lead, 2026-11-13)

What the lead did and measured when the routines were put into the main tree; figures are
from the Release build, visitor zoo, seed 1, after the visit's fill.

- **The watcher test below passes now**: the fix was on the visitors' side and went in before
  this integration. The paragraph is kept as the record of what Codex saw.
- **What the game shows.** `AnimalRenderer` takes the clip of a still animal from a table per
  activity (`Kind::shown`): `dig` and `filter_feed` play `eat`, `bathe` plays `rest`, the rest
  stand in `idle` unless the model's manifest says otherwise (`meta.activity_clips`: flamingo
  `huddle` -> `rest`, its one-leg pose; ostrich `dust_bathe` -> `rest`; camel `ruminate` ->
  `rest`). No model has a clip of its own for preening, dust bathing, digging or sentry duty
  yet: a preening bird stands. The information panel names every new activity.
- **Flamingos are waders.** The routine sent them into the lagoon for every activity but
  sleep, and the lagoon of the present plan is 0.45 to 1.40 m deep everywhere (103 tiles, none
  shallower): they stood in it to the neck. `RoutinePlace` now treats a flamingo as a wader in
  every activity and takes water no deeper than `kWaderStandingFraction` (0.3) of its height,
  0.42 m. Until the plan has shallows (the plan's rework gives the lagoon a 0.3 m shelf)
  no wet place qualifies and the birds keep to the bank; `filter_feed` falls back to `forage`.
  Seen in the game afterwards: most birds on the bank or at its edge, two still swimming out
  on a route across the water. Since 2026-11-14 the lagoon has the shelf (0.3 m, 74 of 103 water
  tiles under 0.45 m): after 6,000 ticks 11 of the 12 birds stand in the shallows, and the game
  shows them wading and feeding there.
- **Animals that walk without getting anywhere: measured, not fixed.** Counting the ticks in
  which an animal is `moving` at a speed under 0.05 m/s, over 6000 ticks: 12,528 of 232,441
  moving ticks (5.4 %); with the profiles switched off (the behaviour before this task) 7,198
  of 238,871 (3.0 %). The worst case is new: a tiger spends 4,461 of 6,000 ticks pinned at
  (245.9, 52.4) between a bamboo clump and a fern of Tiger Forest's thicket, which is planted
  on the 2 m tile grid; the routine prefers shade at midday and the thicket is the shade. The
  navigator works on tiles and knows no props, `AdvanceMovement` stops at them, so a route
  can lead through a gap no body fits, and the five detours and the new choice of place end
  in the same pocket. Others above 300 ticks: a brown bear (1,963; 1,039 before), flamingos
  (993), a meerkat (331), a giraffe (328). On screen a stalled animal stands in its idle or
  rest pose, so it does not look broken, but it is not where it wanted to be and the time
  budgets above count such ticks as walking. Also changed by this task and part of it: an
  animal crowded by a companion stops instead of squeezing past slowly. The remedy belongs in
  navigation (tiles a blocking prop fills are not passable for animals, places inside pockets
  are not chosen), with this count as its regression test. The original diagnostic was
  `.codex-work/probe-stall-ZzProbeTests.cpp`; its active replacement is described below.
  **Measured again on the reworked plan (2026-11-14, `docs/VISITOR_ZOO.md` "The zoo's plan"),
  which keeps the largest resident's width plus 1 m between blocking props:** 3,686 of 273,800
  moving ticks (1.35 %); the tiger's pocket is gone, the only animal above 300 ticks is a wolf
  (820 ticks, longest run 687, at 195.7, 64.0). The navigator itself is unchanged, so a
  player-built habitat can still make such pockets.
  **Waypoints moved off props (2026-11-14, after the keepers):** on the keepers' zoo the
  probe found 5,650 of 268,697 moving ticks (2.10 %), and a giraffe of Acacia Plain stalled
  2,113 of its 4,870: the navigator had planned its route through the centre of a tile a
  sandstone rock stands on, the giraffe gave that waypoint up, found the straight line to
  the next one shut and planned the same route again, over and over. After `FindPath`,
  `KeepWaypointsOffProps` (`AnimalMovement.cpp`) moves every waypoint but the goal that lies
  within a blocking prop's clearance out to its rim (straight out, else to either side of the
  way), or drops it when the way from the one before to the one after is open. The probe
  then: 4,183 of 280,044 (1.49 %), the giraffe 392 ticks.
  **Pocket places rejected (2026-11-19):** `ClearPlace` now requires two body-wide exits from
  a routine destination, checking the whole short exit segment against fences, water and
  blocking props. This keeps a geometrically clear point between props from becoming a
  one-way pocket the tile navigator cannot understand. The same seed-1 visit measured 2,994
  stalled of 270,133 moving ticks (1.11 %), with no continuous stall of 300 ticks. The probe
  is now the active regression `AnimalMovementTests.VisitorZooAnimalsDoNotRemainStalledInPropPockets`
  (`tests/core/AnimalStallTests.cpp`): below 1.3 % and every continuous stall below 300 ticks.
  **Pool-bank planting and prompt recovery (2026-11-19):** keeping land plants off the sloping
  ground dug around every pool changed the prop layout and exposed a 1.34 % result, with two
  giraffes contributing 975 stalled ticks and one continuous stall lasting 289 ticks. An animal
  now tries its next deterministic sidestep after 20 ticks (one second) without progress, rather
  than after 30. The same probe measures 2,688 of 248,322 moving ticks (1.08 %), and the longest
  continuous stall is 110 ticks. The regression limits were not relaxed.
- **Golden hashes** of `tests/core/VisitStartTests.cpp` re-recorded for the routines.
- Pictures: `docs/evidence/animal-routines-2026-11-13.jpg` (lions resting in the shade at
  midday; penguins standing together, one swimming; flamingos at the lagoon after the wader
  rule; the tiger in the bamboo it gets stuck in).


`VisitorTests.WatchersStandAtTheFenceAndTheAnimalsKnow` reproducibly fails at
`VisitorTests.cpp:255`: three watchers have a dot product approximately zero
against the test's expected facing direction. The visitor plan chooses a
specific viewing side, but the assertion finds only the first viewing record
matching the tile; enclosure viewing records can contain multiple sides per
tile. This is a possible explanation, not a confirmed diagnosis. The visitor
owner should inspect the selected side and the arrival-to-watching turn before
changing the assertion or visitor movement. Neither visitor code nor that test
is owned by F4, so both remain unchanged.

The requested few-second whole-suite runtime is not met in this Debug/ASan
configuration. The full CTest invocation took 607.18 seconds, including existing
visitor/map integration cases and a 125.31-second Python animation test. The
exhaustive fifteen-species day/control/week measurements took 374.909 seconds
and remain explicitly opt-in. Fixed candidate arrays bound the new habitat
sampling work, but the inherited enclosure-local neighbour scans and route
allocations mean a strict O(animals × constant), allocation-free step is not
established.

### Commands and observed results

Configuration and builds used the shared cache and the existing `build-asan`
directory, with at most two compile jobs:

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv ASAN_OPTIONS=detect_leaks=0
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DZOO_BUILD_GAME=OFF -DZOO_SANITIZE=address \
  -DZOO_LIBCNA_ROOT=/rv/data/development/github.com/libcna \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build-asan --parallel 2
ctest --test-dir build-asan --output-on-failure -j2 > build-asan/acceptance-ctest.txt
ctest --test-dir build-asan --rerun-failed --output-on-failure -j2 > build-asan/acceptance-rerun.txt
./build-asan/tests/zoo_core_tests --gtest_filter='AnimalEthogramTests.DISABLED_MeasureAllSpecies:AnimalEthogramTests.DISABLED_MeasureLegacy:AnimalEthogramTests.DISABLED_MeasureKeptWeek' --gtest_also_run_disabled_tests > build-asan/acceptance-measurements.txt
set -o pipefail
./build-asan/tests/zoo_core_tests --gtest_filter='VisitStart.*Hashes*' --gtest_also_run_disabled_tests | tee build-asan/hash-transcript.txt | grep '^HASH' > build-asan/recorded-hashes.txt
```

Configuration and the final build succeeded. The exhaustive measurement command
passed all three tests (374.909 seconds). All 47 enabled animal tests passed in
the full CTest run. That initial CTest run reported 128 passes, two assertion
failures and 56 tests unable to launch: a report-label-only test relink overlapped
CTest, temporarily making its executable unavailable. The failed/not-run cases
were subsequently rerun against the completed executable. Fifteen opt-in tests
were disabled in ordinary CTest; the three measurement tests above were run
explicitly.

The rerun completed in 1159.93 seconds: **56 passed, two failed, none unable to
launch**. Across the initial invocation and that rerun, the final result is
**184 passed of 186 enabled tests**, with exactly these unresolved failures:

- `VisitStart.FillHashesShort`: old golden values; replacement pairs are above.
- `VisitorTests.WatchersStandAtTheFenceAndTheAnimalsKnow`: the facing assertion
  described under integration limitations.

Both existing full-visit containment cases, both visiting-time save/load cases,
the starter-zoo kept week and neglect checks passed. The two failing test source
files are outside F4 ownership and require the lead/visitor owner's follow-up.
All changed C++ files were formatted with `clang-format-19 -i`; the final
`clang-format-19 --dry-run --Werror` check and `git diff --check` both exited zero.

Earlier development checks found and resolved activity-expectation, heading-turn,
sentinel-coverage and welfare issues. The broad animal command
`./build-asan/tests/zoo_core_tests --gtest_filter='Animal*Tests.*'` passed 46 tests
before the final shallow-water case was added. The intermediate focused command
`./build-asan/tests/zoo_core_tests --gtest_filter='AnimalEthogramTests.*:AnimalMovementTests.*:AnimalBehaviourTests.*'`
first passed 13 of 15 and then all 15 after fixes. Earlier interrupted CTest runs
are not counted as completed validation.
