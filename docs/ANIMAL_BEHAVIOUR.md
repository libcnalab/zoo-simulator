# Animal behaviour model

The animal subsystem advances in fixed simulation ticks. Needs use game calendar time (`1 / 14,400` game day per tick); movement durations use motion time (`1 / 20` second per tick). Random values are intended to come from the `Animals` instance's saved `Rng` stream.

## Implemented rates and rules

Hunger rises by 0.5 per game day and thirst by 1.0, both clamped to `[0, 1]`. Body scale uses a smoothstep from 0.35 at birth to 1 at maturity; daily food and water needs scale with its square. An active Eat or Drink activity consumes feeder or trough contents and reduces its corresponding need. Food is edible through the configured shelf life. Critical needs emit one alert on crossing 0.9; the alert can recur after the need falls below 0.6.

Health loses 0.5 per day at critical hunger, 1.0 at critical thirst, 0.08 while sick and untreated, and 0.2 beyond the individual lifespan. It recovers by 0.1 per day when hunger and thirst are below 0.7 and the animal is not sick. The strongest active health loss selects the reported death cause. Dirt increases from living animal mass and can be reduced with `Clean`.

Habitat scores implement the space, terrain, water, deep water, vegetation, shelter, social group, predator, and enrichment formulas in the contract. Welfare adds nutrition, health, calm, and cleanliness, and smooths the overall value on the hourly stagger. `Containment`, age, adult status, body scale, value, crate, uncrate, recapture, food selection, refill, treatment, and viewer bookkeeping are also implemented.

## Simplifications and unimplemented behavior

The current simulation pass does not implement natural daily activity selection, route planning, collision avoidance, shelter use, sleep and fatigue, weather comfort, juvenile suckling, breeding, sickness onset and recovery, fights, predation, fence breakouts, or loose animal roaming. Feeding and drinking currently require an activity already assigned; the subsystem does not yet move animals to those sources. The class header has no per-animal fields for alert latches, so the implementation stores them in a parallel saved vector.

Food freshness uses the catalog shelf life; open water is not yet used as a drink source. Habitat and group evaluation currently scans animals directly and is not optimized for the stated 300-animal performance target. Shelter and comfort welfare aspects are placeholders pending those behaviors.

## Persistence

The JSON state stores animals by species string id, activity and sex by their stable names, route points, enclosure dirt, alert latches, counters, id allocation, and both 64-bit RNG words. Viewer counts are inputs and are not saved. Loading rejects unknown species or enum names, malformed fields, unsorted ids, and ids at or above the next id.
