# Animal behaviour

Animal simulation advances once per world tick in ascending animal id. Animals
outside the world in a crate do not advance. The simulation owns its `Rng`; its
state, animal needs, movement route and activity are saved, so a loaded simulation
can continue deterministically.

## Needs and condition

Hunger rises by 0.5 per game day and thirst by 1.0. Suitable, fresh food is
consumed from the active feeder and reduces hunger according to the animal's
scaled daily food requirement. Water is consumed from the active trough. Need
alerts are latched on each animal: starvation and thirst alerts are emitted when
the corresponding need crosses 0.9 and can be emitted again after the need falls
below 0.6.

Fatigue rises while awake and falls during Rest and Sleep. Boredom rises according
to playfulness and habitat enrichment, and falls during Play, Socialize and
Wander. Stress moves toward a target from crowding, poor welfare or being loose.
Juveniles younger than one quarter of maturity age are protected from hunger and
thirst above 0.3 while their living mother is in the same enclosure and has
hunger below 0.9.

Health damage covers critical hunger and thirst, untreated illness and age beyond
lifespan. Healthy animals recover health. An animal whose health reaches zero is
removed and emits `AnimalDied`.

## Feeding and movement

When a need is urgent, the animal searches its current enclosure for an edible
feeder or a water trough. The closest usable prop is selected. Animals move
toward a standing point beside that prop. The first eight path searches in a
tick may use A* with a 6000-node expansion limit; movement follows a route at a
smoothed speed and heading. A direct walkable route is preferred. Failed paths
stop the movement and make the activity available for reconsideration.

The current implementation does not yet choose destinations for general
wandering, resting, sleeping, play or social behaviour. It does not yet implement
prop collision steering, shelter use, open-water drinking, weather decisions,
escape attempts, or the full daily activity schedule. These are required parts
of the intended model and remain outstanding.

## Saved state

Animal JSON includes needs, activity, movement target and route, shelter and
enclosure identity, and alert latches. The `alerts` field stores starvation in
bit 0 and thirst in bit 1. `StateHash` includes these fields and every waypoint
in route order. Viewer counts remain inputs and are not saved.
