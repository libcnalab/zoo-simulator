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
