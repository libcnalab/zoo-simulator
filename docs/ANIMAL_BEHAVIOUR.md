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
Sleep. Sleeping animals wake for urgent hunger or thirst above 0.8; other
activities use the lower eating and drinking thresholds. Boredom rises with
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

When no urgent need or shelter decision applies, a weighted choice uses fatigue,
boredom, playfulness and sociability to select Wander, Idle, Rest, Forage, Play,
Socialize or Vocalize. Carnivores and piscivores do not forage. Herbivores and
omnivores lose a small amount of hunger while foraging. Playing, socialising and
wander reduce boredom. High stress can start a pacing bout along the enclosure
edge. A low containment score is checked once per game hour, staggered by id; a
successful attempt runs to the weakest edge and crosses it, then emits
`AnimalEscaped`. `Recapture` places the loose animal back in its home enclosure or
crates it if the home no longer exists.

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
In the kept starter zoo that is about one illness per animal in half a year
(`ZooSeasonTests`, disabled because it is slow: ninety days, 13 illnesses among 12 to
27 animals, 15 births, no deaths).
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
the animal's wading and swimming profile and a 6000 expansion limit. Speed
accelerates gradually and brakes only on the last leg of a route; heading turns
at a capped rate.

Obstacles are rounded, not stopped at. Each tick the planned step is *slid*
along the clearance circle of every blocking prop and of every other animal it
would enter: the inward part of the step is removed and the tangential part
kept; a head-on approach turns to one fixed hand so that two animals meeting
pass each other. Between props it is the width of the body that has to fit
(`kBodyWidthFraction` of the body radius, which is half its length). Two animals
that still press into each other squeeze past at a fraction of their speed
(`kSqueezePastFactor`) instead of waiting for one another, which is what used to
deadlock a group at a trough. The prop an animal is heading for (its feeder,
trough or toy) is never an obstacle to it, and neighbours are ignored on the
last metre to that prop.

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

## Saved state and current simplifications

JSON stores every animal field, including `alerts`, activity state, target prop,
route and route index, the stall and sidestep bookkeeping, plus the simulation RNG and enclosure dirt. Equal saved
state and equal inputs produce equal hashes and event sequences. Viewer counts
remain external inputs and are not saved.

The current routine uses hand-tuned activity weights and a coarse circadian
curve. Wander destinations prefer terrain the species tolerates and nearby
group mates; young animals stay within a few metres of their mother. Pacing
walks a direct 6–10 metre loop along the boundary within one pacing activity.
In heat, shallow water or a spot beside a tree is used when no cooled shelter
has room. Habitat evaluation is cached by enclosure, species, group composition,
revisions and game hour.
The disabled performance scene measures 300 animals in 20 enclosures; its
sanitizer timing is not a release performance result.
