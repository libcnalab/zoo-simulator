# Keepers

The zoo's staff a visitor sees at work: keepers at the habitats, a gardener at the flower
beds, a cleaner at the bins. The simulation lives in `src/core/zoo/staff/` (no CNA; part of
`zoo_core`), is stepped by `Zoo::Step()` right after the visitors, is saved with the zoo and
enters its state hash. The game draws each member of staff as a CNA avatar in the zoo's
uniform, with the tools of the job (`src/game/KeeperRenderer.cpp`).

There is no management in it: nobody is hired, paid, trained or tired. The crew is fixed
when the Visitor Zoo is built (`BuildVisitorZoo` calls `Staff::Engage`) and goes about its
rounds all day.

## The crew

`Staff::Engage` looks at the zoo as it stands:

- **Keepers** share two habitats when their centres are less than 100 m apart. A more
  remote habitat gets its own keeper. For up to 20 habitats the planner finds the greatest
  number of nearby pairs and then minimises their combined distance. Larger parks pair
  the nearest available neighbours within the same limit. Every habitat with a place to
  work from has a keeper (`Staff::KeeperOf`).
- **A gardener** when there are flower beds that can be stood beside (at most 24 on the
  round, spread evenly along a walk of short hops from the entrance).
- **A cleaner** when there are bins (at most 30 on the round, likewise).

Every member has a `look` (a hash of the id) that picks the avatar playing them.

## A round

A member goes round their stops in order, again and again. At each stop `PlanStop` plans
the jobs from what the stop needs and from the hour; every job (`StaffTask`) has a place
to stand, a way to face, a duration, a visible activity (`StaffActivity`) and the way the
member gets there (walking, carrying a bucket, pushing the wheelbarrow). Its effect on the
zoo happens half way through the job.

- **Feed and water.** Feeders below `kFeederDue` (0.85) and troughs below `kTroughDue` (0.9)
  of their capacity are filled, the emptiest first, at most four a stop. Inside a habitat
  the keeper stands behind the feeder as the public sees it, facing them.
- **Public feedings.** Two a day per habitat, by diet (herbivores 9:30 and 14:00,
  omnivores 10:00 and 14:30, insect eaters 10:30 and 16:00, carnivores 11:00 and 15:00,
  fish eaters 11:30 and 15:30); one keeper's two habitats are kept at least 45 minutes
  apart. A feeding due (from 9 minutes before its hour to 45 minutes after) comes before
  the order of the round: feed is scattered near where the public stands, every feeder
  is topped up and every trough gets fresh water. The information board of the habitat says when
  ("Keeper feeds: 11:00 and 15:00, next at 15:00"; `Staff::FeedingHours`,
  `Staff::NextFeeding`).
- **Routine by the hour.** Mucking out (raking, the wheelbarrow) in the morning, hosing at
  the water's edge, fence checks with a clipboard, a breather at midday, a walk round at
  night. A habitat dirtier than 0.25 is raked whatever the hour. Residents at or above 0.5
  shyness are serviced from the public side instead of repeatedly being displaced from food and
  water by routine entry; Greenhill gives each of its three such northern habitats two separated
  troughs.
- **Dangerous species.** Feeding, watering, fence checks and sweeping remain on the public
  side. When dirt reaches 0.18 (and periodically every third visit), the keeper operates a
  four-second secure control at the service gate. Before a routine recall the keeper checks
  that every resident can go two animal-clock hours without reaching the hunger or thirst issue
  threshold. A group that needs a drink or meal sooner keeps access to its provisions. Every animal in the
  habitat must fit into an open, registered shelter whose door its species can use; a loose resident, missing door or short
  capacity or disconnected route makes the whole operation fail without moving any animal. On
  success the assigned group remains visible while walking purposefully, at up to 1.5 times its
  ordinary walk but never faster than its natural run, to those doors. The keeper waits at
  the outside control until the last dangerous resident is hidden and its separately drawn welded
  holding gate has slid shut, crosses the same registered service gate for one six-second rake
  at its inside tile, returns outside, visibly slides the holding gate open for 1.2 seconds and only
  then releases the group.
  If the way out changes, the animals stay secured and the keeper retries rather than completing
  the release remotely.
- **The gardener** waters a bed every third stop, kneels and tends it with a trowel, and
  rests now and then; **the cleaner** gives priority to the nearest bin with litter within
  30 m, empties it and then sweeps that litter, or the path beside the bin. The overlapping
  catchments include the cleaner's stand beside a bin and no longer leave a narrow unswept ring.
  The sweep target is refreshed before the walk and again on arrival, so a wrapper dropped after
  the stop was first planned does not wait for a complete circuit of the zoo.

## Where they walk and stand

Staff walk on the navigator with `NavProfile::Staff()`: anywhere dry (no tile with water at
its centre), through keeper gates, preferring paths three to one. Since G8 every keeper gate
opens onto a dry, unpaved service apron joined to the walks (`StandOffWithService` in
`VisitorZoo.cpp`, checked by `VisitorZooPlan.EveryKeeperGateHasADryConnectedApproach`), so
the keepers come in the way a zoo's staff do, behind the public's backs; planted stand-off
strips are nobody's ground and nobody walks them.

A place to work is only chosen on ground no deeper in water than a boot (8 cm; the tiles
beside a pool are dry by the map and still slope into it), inside the habitat when the
keeper goes in, on public ground otherwise, and clear of trees and rocks. On the way a
walker follows the rim of a tree or a rock that is in the way and never crosses a fence that is
not a gate. A dangerous habitat is passable only while its saved keeper lockout is active and
every dangerous resident is sheltered; the short direct step is tied to that habitat's registered
keeper gate (`MayStep`). A job whose place
cannot be reached, or whose walk takes more than twice what the route needs plus a minute,
is done as the zoo's automatic care would have done it, and the member goes on.

Since J19 walkers also keep their bodies apart. A moving member slides tangentially around
visitors, visible animals and other staff in the same public region or habitat; if two bodies
already overlap, only a separating step is accepted. Visitors in turn receive all current
staff positions as stationary steering obstacles for that tick. Positions on opposite sides of a
habitat fence do not interact. These obstacles are derived each step, not saved and not hashed;
the paths taken because of them remain ordinary simulation state.

At J19 the seed-1 morning regression started with 249 visitors and followed all nine staff for another
3,600 ticks. It exercised 2,653 staff/visitor and 75 staff/animal close approaches. No walking
member reduced a current separation below the 0.68 m adult clearance (or the animal's scaled
body clearance), and the closest moving visitor remained 0.383 m from the staff position it was
steering around. The inspected game frames show a keeper among the meerkats and the cleaner at a
bin beside the moving public: `evidence/staff-personal-space-j19-2026-11-20.jpg`. A still image is
context rather than collision proof; `Keeper.AtWorkAmongVisitorsAfterTheMorningFill` is the
measured check.

J20 adds the other half for animals that are shy enough to care. Before the animal step, staff
positions inside safe habitats are supplied to `Animals`; a species with `shyness > 0.3` chooses a
clear four-metre retreat when a keeper enters its body-scaled flight distance. An urgent journey
to food or water is not interrupted, and staff outside the habitat cannot trigger it. The same
morning regression now records zero staff/animal body-clearance approaches (75 after J19), while
animal stalls improve to 0.758 % of moving ticks and the staffed week remains no worse than
automatic care. The same-camera game comparison is inspected in
`evidence/shy-animal-keeper-j20-2026-11-20.jpg`: the small meerkat beside the hosing keeper in J19
has left that space in J20. The cast member's face varied between runs then; the measured movement
test, not the still, proves the response. J22 later makes that cast repeatable.

J30 closes two layout-dependent gaps in the walking guard. A member now applies body avoidance to
the final arrival step as well as intermediate route steps, then validates the combined prop/body
slide against every visitor, animal and colleague; a later tangent can no longer undo an earlier
one. If a shy animal has just entered its clearance plus an 8 cm reaction margin, staff wait for
its keeper-retreat route instead of walking away through the overlap. The changed wide-walk plan's
morning run exercises 3,289 close staff/visitor steps, records zero staff/animal approaches, and
keeps the nearest moving visitor 0.253 m away; the zero-contact assertions were not relaxed.

The 121-habitat expansion exposed two further edges in that guard. A tolerant flamingo could step
into a member's motion margin between ticks; staff now wait for every visible animal in that margin,
not only shy species. The measured visitor morning again records zero close animal steps while staff
remain visibly at work for 63% of their time and negotiate 7,373 close visitor steps. The same larger
zoo also showed that repeated entry displaced moderately shy Arctic foxes from provisions. The
outside-service threshold is now 0.5 shyness. At visitor pace the staffed comparison falls from 13
animal-hours in need (five consecutive, worst hunger 0.787) to zero, with worst hunger 0.669 versus
automatic care's 0.685. Test diagnostics now name the species and habitat behind every extreme.

J35 closes the dangerous-habitat care gap. `Animals::SecureForStaff` first solves the whole
group-to-shelter capacity assignment and changes nothing unless every animal has a valid slot.
The resulting shelter hold is transient input rebuilt from the keeper's saved `lockout` field:
needs, disease and health continue to advance, but escape, new behaviour and leaving the house do
not. All seven dangerous Visitor Zoo habitats exercise a real inside job; no measured lockout lasts
600 ticks. Failed capacity is atomic, a live lockout survives save/load, staff save version 1 still
loads, and the safety actions are version 2. The week records 7 animal-hours in need against 9 with
automatic care alone, a two-hour longest spell against three, and worst hunger 0.712 against 0.786;
the 3,600-tick public morning still records zero staff/animal body-clearance approaches. All 285
active CTest cases pass, including the 366.73-second animation test; 21 diagnostics remain disabled.

J38 locks down the cleaner's existing daytime effect rather than merely its animation. A public
visit produces ground litter, the regression waits for the exact midpoint of a visible sweep and
observes the count fall before the day changes; automatic midnight cleaning remains only the net
behind anything the round did not reach. The complete suite passes all 289 active cases, including
the 362.86-second animation test; 21 diagnostics remain disabled.

J42 makes that daytime effect independent of when the public happens to drop its wrappers. A
cleaner now chooses the nearest dirty bin catchment before continuing the ordinary round, retains
the bin on its sweep task, and refreshes the exact litter target before setting out and on arrival.
The J38 public-visit regression had regressed after an unrelated crowd correction: its closest
scheduled sweep was 39.69 m from any litter although one piece lay 0.23 m from a bin. It now
removes real litter before midnight again; all 13 focused keeper cases pass, including equal-run
determinism, a mid-round save/load and the staffed week. The combined J41/J42 suite passes all
292 active cases, including the 366.68-second animation pipeline; 21 diagnostics remain disabled.
Both short and 6,000-tick state hashes repeat twice for seeds 1–3 (J42, 2026-11-20).

J50 replaces J35's instantaneous shelter transfer with a visible, fail-closed recall. The same
atomic capacity and fallback-route check assigns each resident a registered door, but preserves
its position and uses the ordinary animal gait and collision system to reach it. A shelter route
which cannot fit the exact prop-aware tile-centre graph may fall back to the older grid route;
waypoint correction and every physical step still avoid the real trees and rocks. A later large-zoo
regression gives this zero-dwell safety errand a purposeful 1.5x walk (capped by natural run speed)
and shortens the single inside rake from twelve to six seconds; every four-day lockout remains below
the unchanged 1,500-tick bound. The keeper holds
the outside control until `DangerSecuredForStaff` sees every dangerous resident hidden, so the gate
remains impassable during the walk. Every dangerous Visitor Zoo habitat shows a recall and later
admits its keeper; the longest complete lockout is 1,116 ticks (55.8 seconds) against a 1,500-tick
regression bound.
Both a recall and the subsequent inside lockout survive save/load bit-identically. The current
staffed week records 10 animal-hours in need against 16 with automatic care alone, and a two-hour
longest spell against four. The inspected Polar Sound frame shows all three bears approaching the
house in `evidence/animated-lockout-j50-2026-11-20.png`. Changed short and 6,000-step hashes
repeat for seeds 1–3. The complete suite passes all 298 active cases including the 466.17-second
animation pipeline, with 21 diagnostics disabled (2026-11-20).

J58 completes the visible lockout at the animal-house end. Each registered house assigned an
animal receives a welded-mesh sliding leaf with a steel frame, fixed overhead rail, diagonal brace
and yellow operating plate. Empty houses remain open; an assigned leaf closes only after its last
resident has crossed the threshold. On the return journey the keeper no longer skips the release
task as soon as they reach public ground: they operate it from that safe side, the leaf slides open
over 1.2 seconds, and the saved lockout releases at the task's midpoint. If the exact outside control is
unreachable, that safe-side operation remains the fallback, so presentation cannot strand the
animals or relax the entry guard. `Keeper.DangerousHabitatsHaveOutsideServiceAndLockoutWorkInside`
now requires every dangerous habitat to show this outside release operation. A lockstep Polar
Sound run was opened at the open, closed, sliding and released frames; the three bears emerge
only after the doorway is clear. The staffed week remains better than automatic care at 11 against
16 animal-hours in need, with longest spells of one against four hours. Changed short and
6,000-step hashes repeat twice for seeds 1–3. The complete suite passes all 304 active cases,
including the 479.96-second animation pipeline, with 21 diagnostics disabled. Under ASan the
two gate/lockout tests, the focused group-route test, the 12,000-step stall regression and all 30
game-side tests pass; a real three-frame Polar Sound visit after 3,000 fill steps also completes
cleanly (leak detection off for the documented `ptrace` limitation; J58, 2026-11-20).

J21 spreads outside work around habitats which have a planted stand-off strip. `BarrierStands`
used to stop as soon as it found the service gate, even if that left Elephant Valley and Rhino
Hollow with one place for every feeding, fence check and sweep. It now keeps both the direct gate
stands and every reachable public viewing bay. A 12,000-tick regression sees at least two distinct
safe work places at every dangerous habitat; before the change Lion Rock had two, Elephant Valley
and Rhino Hollow only one. The inspected game frame shows keeper 7 checking Elephant Valley from
its public dry-moat gallery, with the elephants beyond the separate containment barrier:
`evidence/safe-barrier-work-j21-2026-11-20.jpg`. All 271 active functional tests pass; the short
and 6,000-step state hashes repeat for seeds 1–3.

J22 gives the four-person cast a repeatable appearance. It still alternates women and men and uses
CNA's current avatar catalog, but supplies a fixed per-slot random stream rather than calling the
process-random `AvatarDescription::CreateRandom`. `KeeperAppearance.CastIsValidDistinctAndRepeatable`
checks the public 1,021-byte descriptions, body types, validity and distinctness. Two separate game
processes at the same seed and tick show keeper 7 with the same face, hair, build, height and uniform
in `evidence/repeatable-staff-cast-j22-2026-11-20.jpg`; the gait differs slightly because the images
were taken after 30 real-time render frames whose loading times differ. All 22 game-side tests and
all 272 active functional CTest cases pass.

J23 grounds the tools as well as their users. Buckets, barrows, rakes, brooms, hoses, clipboards,
sacks, trowels and watering cans now lay small soft patches on the drawn ground, projected away from
the sun by the height of the object. Long tools use three overlapping patches along the shaft rather
than a disconnected round blob. They share the crew's existing transparent pass, so the change adds
no draw call; the busiest barrow-and-rake pose adds 210 simple triangles. The inspected morning frame
shows the wheel, tray and rake joined to the yard in
`evidence/staff-tool-shadows-j23-2026-11-20.jpg`. The warning-clean game build, targeted off-screen
run and all 272 active functional CTest cases pass (21 diagnostics remain disabled).

## Automatic care stays behind them

The game's time is compressed (a game hour is thirty seconds of walking, `docs/VISITORS.md`
"Time"): a full feeder is eaten in about 2,400 ticks, while a keeper's visible round of two
habitats takes far longer. Keepers alone left animals hungry (hunger 1.0, 72 to 77 hours in
need, dirt 0.93 in the earlier run's measurement), so `autoCare` stays on as the net behind
them: what the keepers do is what a visitor sees, and nobody goes hungry when they cannot
keep up. The tests compare a staffed zoo with an unstaffed one under automatic care. Aggregate
hours in need and their longest run may be only narrowly worse; the worst hunger and thirst stay
within 0.07 of automatic care whenever it is near the welfare issue line, and otherwise must still
remain at or below that 0.70 line. The diagnostic names the particular animal, habitat and tick
behind each maximum so a layout-dependent change can be investigated rather than dismissed as noise.
With J50's animated dangerous recalls, the staffed week records 10 animal-hours in need against
16 under automatic care, with worst hunger 0.739 against 0.770 and thirst 0.762 against 0.730.
After the northern expansion reached 16 habitats, the former 14-habitat exact-search limit made
the greedy fallback leave Lynx Ridge and Red Panda Canopy as its final pair: one keeper had a
204 m cross-zoo round. J80's bit-mask solver finds the global minimum without enumerating every
pairing. No two-stop round is now 100 m or longer. The staffed week records 18 animal-hours in
need against 20 under automatic care (worst hunger 0.702 against 0.745); at visitor pace the
comparison is 10 against 13 (0.628 against 0.681). J81's second northern stage raises the zoo to
17 habitats and nine keepers; Snow Leopard Heights is the odd habitat with its own keeper. Two
water points in each very shy northern habitat keep feeding and watering outside; inspections are
also outside, with one brief inside rake per round so manure cannot accumulate. The new layout's
staffed week records 13 animal-hours in need against automatic care's 26, and the visitor-paced
morning 21 against 30. A lockout is owned by the keeper operating it: other staff cannot use its
open gate as a shortcut. That layout had nine keepers, a gardener and a cleaner.
Adding Capybara Marsh made the old even-habitat rule pair the red panda and snow leopard
galleries, 113.7 m apart. With the distance limit in force the 18-habitat zoo has ten
keepers, a gardener and a cleaner. Eight keepers have two stops. Snow Leopard Heights and
Red Dune each have a dedicated keeper. Penguin Cove shares its keeper with Capybara Marsh.
The seed-1 staffed week recorded one animal-hour in need, compared with ten under automatic
care alone. Both morning visits recorded zero.
The comparison still keeps its separate
aggregate, duration and peak checks rather than allowing one good average to hide a stranded animal.
The 121-habitat week exposed that exact individual failure in a slow tortoise. Its far feeder and
trough could make urgent hunger and thirst reverse the journey just before arrival. Essential food,
water and shelter journeys are no longer abandoned merely for exceeding the old forty-metre time
budget; their existing progress/stall guard still rejects blocked routes. A species walking below
0.3 m/s may switch from food to urgent water while far away, but commits to the final five metres of
its food journey so it satisfies one need instead of oscillating. Direct tests cover both sides of
that boundary. The final staffed week records 55 animal-hours in need against automatic care's 82,
a three-hour longest spell against five, worst hunger 0.780 against 0.826 and worst thirst 0.768
against 0.839. The 12,000-tick whole-zoo stall probe remains green at 5,941 of 1,809,748 moving ticks
(0.328%), with every individual run below the unchanged 120-tick bound.

## Determinism and saves

`Staff::Step` steps members in ascending id; there is no random stream (variety comes from
the stateless hashes of ids, tiles and the count of stops done); everything is plain data
compared by `operator==` and hashed by `StateHash` (FNV-1a, floats by bit pattern). The save
key `"staff"` holds the whole crew; a save without it opens as a zoo without staff, a damaged
one is refused (`StaffFromJson`). Staff format 2 adds each task's lockout action and the keeper's
active habitat; format 1 still loads with both absent. A zoo without staff keeps the state hash it had.

## Drawing them

`KeeperRenderer` makes a repeatable cast of four avatars once (women and men in turn), all in the
uniform: a green polo shirt, khaki cargo trousers, boots and a khaki bucket hat. The uniform is
written into CNA's avatar description (item and colour bytes
as CNA's `docs/avatars.md` lays them out, and its CRC-32); if CNA refuses the result, the
colours alone are tried, then the person's own clothes, and one log line says so.

Every member is posed from the simulation: the gait of `AvatarPose` while walking, and a pose
of its own for every job (a lean, a squat, the hands placed and the arms solved), blended in
and out so that a rake does not turn into a bucket in the hands. The tools are coloured
triangles built every frame for those in view (bucket, wheelbarrow, rake, broom, hose with its
jet, clipboard, sack, trowel, watering can) and drawn in two calls for the whole crew; they
cast soft projected contact shadows, as does each member. All eleven members of Greenhill's crew
fit the 12-nearest full-avatar budget; nobody beyond 150 m is drawn. The bounded headroom admits
a somewhat larger built-in zoo without letting a damaged or imported save request hundreds of
CNA avatars in one frame. A warning-clean game build, a real 626-visitor offscreen run at a
working keeper and all 32 active keeper/Walker integration cases pass (J46, 2026-11-20); the
captured frame was opened and checked.

`--stand keeper:<id>` stands the camera on the nearest path a few metres from that member
(`keeper:0`: the first member at a job; `keeper:0:rake`: the first raking) and prints who it is
and what they do.

## Cost

The entrance bench (`--bench --frames 300`, 800 visitors), RelWithDebInfo binaries of develop
without and with the keepers, interleaved, three runs each, 2026-11-14, load average 3.6 to 4.9:

| | before | after |
|---|---|---|
| frame ms mean | 15.05, 14.22, 14.53 | 16.49, 14.18, 13.98 |
| frame ms median | 14.63, 14.26, 14.45 | 15.74, 14.18, 14.10 |
| people pass ms (visitors and keepers) | 3.41, 3.25, 3.16 | 4.85, 4.41, 4.29 |
| simulation ms | 0.30, 0.27, 0.27 | 0.48, 0.33, 0.34 |
| draw calls | 478, 479, 479 | 471, 471, 471 |
| resident memory | | 2233 to 2236 MB |

The keepers' avatars and tools add about 1.1 ms to the people pass and 0.05 to 0.2 ms to the
simulation; the frame as a whole shows no cost beyond the spread of the runs (the first "after"
run is the slowest of all six, the other two the fastest). Engine draw calls fall by 7 and
triangles by 0.19 million because the crowd at the entrance differs (the state differs from
the first tick on), not because of the keepers.

## Open

- Nobody is drawn beyond 150 m or beyond the 12 nearest.
