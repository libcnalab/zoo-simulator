# The pivot: from building a zoo to visiting one

On 2026-11-12 the project changed direction. It had been growing into a management game (build
habitats, buy animals, balance the books, seen from above). The owner's correction: the player is
a **visitor**. The game starts in a finished, populated zoo, at eye level, and is walked through.

This document records what that changed, what was kept, what was verified and how, and what is
still not good enough. Figures are from runs of the real executable on this machine; where
something was only checked by its numbers and not seen or heard, it says so.

## What the game is now

`./zoo-simulator` opens a title in front of the entrance of **Greenhill Zoo**. *Visit Zoo* puts
the player on the entrance path at 10:30 on a summer morning, with the gates open since nine and
the morning's crowd already spread over the park.

| | |
|---|---|
| Title | Visit Zoo, Continue Visit, Load Visit, Settings, Quit |
| Walking | `W A S D` or the arrows; the mouse looks around |
| Pace | `Shift` **switches** between walking (1.6 m/s) and running (4.4 m/s); it is not held |
| Looking at an animal | a card fades in: its name, species, sex and age, what it is doing |
| `E` | read the information board one stands at (range, diet, facts, who lives here) |
| `M` | the visitor map: every habitat by name, buildings, "You are here" |
| `Esc` | the pause menu (Resume, Save Visit, Load Visit, Settings, Back to the Title); the mouse is released and the world stands |
| `F5` | save the visit; the visit is also kept every two minutes for *Continue Visit* |
| `F4` | diagnostics: visitors simulated / in range / drawn, avatars ready, sound voices |
| Settings | mouse sensitivity, invert Y, field of view, head bob, volume, diagnostics |

The walker follows the terrain, is stopped by fences, gates, buildings and the larger props, and
cannot enter a habitat. There is no money, no build menu and no management HUD in a visit; the
screen shows the world, a small hint line, and the cards and boards above.

The management game is not deleted. `./zoo-simulator --editor` starts it as it was (overview
camera, build palettes, finances, staff contract), and all of its simulation code is what runs
the visit: the animals' needs and routines, the visitors' groups and errands, the keepers'
automatic care. It is not being developed further unless the owner asks.

## Why no people were seen before

The owner had run the earlier build and seen an empty zoo. Three separate causes, all real:

1. **Nothing drew the visitors.** The visitor simulation existed in `zoo_core` and was stepped,
   but the game had no renderer for it until commit `30606d9`. People were numbers in a window.
2. **The scenario opened at 08:00 and the gates at 09:00**, with nobody inside. A player who
   started and looked around was in the zoo an hour before the first group could arrive.
3. **The camera was an overview camera.** Even a drawn crowd would have been dots.

What was done: visitors are drawn as **CNA Avatars** (`VisitorRenderer`: a cast of 28 random
avatars from CNA's `AvatarDescription` and `AvatarRenderer`, each posed by the game with a walking
gait and sitting, eating, watching and reading poses, drawn in their own pass). The visit starts
at 10:30; before the first frame the zoo is simulated from the opening (the loading screen shows
this step), and `Zoo::SpreadVisitors()` sets the morning's groups down all over the paths instead
of in a column at the gate. A visit pace (`VisitorPace`: linger, stay, arrivals) keeps people at the habitats and
on the benches long enough to be met.

## What was reused and what was added

Reused as it was: the world grid and terrain, fences, paths, the construction catalog, the builder
(the Visitor Zoo is built by a script through the same `Builder` operations a player would use),
enclosure analysis, navigation, the animal simulation (needs, routines, shelters, feeders, life
events), the visitor simulation, saves, the terrain/scenery/animal renderers, the interface
library, the animal generator `tools/animals`.

Added or changed for the pivot (commits, oldest first):

| Commit | What |
|---|---|
| `30606d9` | visitors drawn as CNA avatars |
| `939911d` | the calendar can run slower than bodies move, so a visit is not over in ten minutes |
| `16ab8ab` | the visit: title, walker with collisions, mouse look, pause, settings, save/continue/load; the management game moves behind `--editor` |
| `ba25422` | the sound of the zoo (`Soundscape`): positional sources through CNA Engine's audio |
| `6765b1f` | the Visitor Zoo: Greenhill Zoo, fourteen habitats on a round walk, plaza, two food courts, planting |
| `7b554e3` | the scenario builds in under half the time |
| `c783101` | the visitor map |
| `53de51f` | visit pace and the spread of the morning's crowd |
| `fbdcf41` | 48 recordings with per-file sources and licences |
| `731148a` | the loading screen |
| `ec9aa4a` | animals do not sleep in water |
| `610c33f` | a 4.5 m glass wall, so tigers and bears are seen through glass and not through bars |
| `fca0d9c` | `--stand habitat:N` for repeatable pictures from the glass |
| `39210fc` | models of elephant, giraffe, camel, meerkat and rhino |
| `418205e`, `4359dd6` | `--stand` looks squarely through one fence, finds a habitat by its species, lists who is outdoors |
| `f4a8ba4` | models of penguin, ostrich, flamingo and kangaroo: every species has one |
| `f85e97c` | polar bears stay outdoors on a summer day (they sat in the cooled den from the opening) |
| `ddf8a35`, `230d57c` | the flamingo lagoon is seen through glass like the other habitats, not through mesh, and under acacias: its willows hung over the view |

The avatars come from CNA itself (its implementation of XNA's avatar API); CNA Extended is not
used. No change to CNA or CNA Engine was needed for the pivot itself; the game relies on
`EngineGame::SetShadowArea` (cna-engine `04ab7b3`, added the same day) to keep the sun's shadow map
round the walker.

## The zoo

320 × 320 m, the plan and the table of habitats are in [`VISITOR_ZOO.md`](VISITOR_ZOO.md).
Fourteen habitats: Meerkat Lookout, Flamingo Lagoon, Acacia Plain (giraffes and zebras together),
Ostrich Rise, Lion Rock, Tiger Forest, Wolf Wood, Bear Falls, Polar Sound, Penguin Cove, Elephant
Valley, Rhino Hollow, Red Dune (camels), Kangaroo Grove. An entrance plaza with gift shop,
education centre, refreshments, restrooms, fountain and statue; a central and a northern food
court; kiosks, benches, lamps, bins, flower beds; woods between the habitats.

## Animals

Models are generated, not downloaded: `tools/animals` builds a rigged glTF with eight clips
(idle, walk, trot, eat, drink, rest, sleep, vocalize) per species from a species definition. The
game drives the clips from what each animal is doing in the simulation and matches the gait to
its ground speed.

All fifteen species have a model: lion, tiger, wolf, brown bear, polar bear, zebra, giraffe,
elephant, rhino, camel, meerkat on the quadruped generator; penguin, ostrich, flamingo and
kangaroo on the biped one (the penguin waddles, the flamingo rests on one leg and feeds with its
bill upside down, the kangaroo hops with both feet and crawls on feet, hands and tail). A seed-1
visit has **74 animals** in all fourteen habitats; a run at 10:30 counted every one of them
outdoors.

**These are stylised procedural models, not photorealistic ones.** Each is recognisable at a
glance by silhouette and coat; none would be mistaken for a photograph or for a scanned or
sculpted production asset. Specific weaknesses: the meerkat is convincing in its upright sentinel
pose and weak when walking; the rhino and camel read as toy-like up close; zebra stripes alias at
a distance; fur and feathers are paint, not geometry; the ostrich's plumes are simple fans, the
flamingo's wings broad patches of colour, the penguin's face markings graphic.

## Visitors

`Visitors::kMaxVisitors` is 800, and a visit reaches it. Measured at 10:30 in a seed-1 visit:
800 simulated; about 730 within range of the player at the entrance; walking 415, watching
animals 177, eating 153, queueing 15, sitting 10, reading boards 8. The renderer draws the nearest
ones (60 in the automated runs below; the game's own default is higher) from a cast of 28 avatars,
all 28 built without failure. Groups walk together, stand at the glass, sit on benches and eat at
the courts.

## Sound

`Soundscape` plays through CNA Engine's 3D audio: the park's air, water at ponds and fountains, a
murmur where people gather (louder with more of them), animal calls from where the calling animal
stands, the player's footsteps by surface (paved, gravel, grass). 48 recordings of twelve species
and the park, each with its source, author, licence and edits in `assets/audio/SOURCES.md`
(public domain, CC0, CC BY, and CC BY-SA for the meerkat and camel calls). The library has gaps:
some species have one or two calls only, nine call names have no recording at all (among them
the elephant's trumpet and the tiger's chuff), and a few clips are substitutions, each marked in
the sources table.

**Sound was verified by its figures only** (recordings loaded, voices playing, calls triggered):
the automated runs use a dummy audio device, and nobody has yet listened to the mix on speakers.

## How it was verified

- Unit tests: `ctest --test-dir build` — 158 tests, all passing: 157 of `zoo_core` and the animal
  pipeline's suite (4 long diagnostic tests are disabled by design). The Visitor Zoo has its own tests: every habitat contains its
  species (fence height and strength), every path and door is reachable.
- Animal pipeline: `python3 -m unittest discover -s tools/animals/tests` — 28 tests (no foot
  sliding, no over-extended legs, seamless loops, a supporting foot at every moment of a bird's
  walk, the kangaroo's hop in step) for fifteen species.
- AddressSanitizer: the whole core suite in `build-asan/` (49 minutes) reported no memory error.
  One test failed there, on a timing budget of the optimised build (a millisecond a tick for the
  whole zoo; the instrumented build takes four); that assertion is now compiled out under a
  sanitizer; the suites changed afterwards (25 tests) were run under it again. The game
  executable itself has not been run under a sanitizer.
- The real executable, run headless (`xvfb-run`, software rendering) with a script that takes
  screenshots, each of which was opened and looked at: the title, the loading screen, the
  entrance with visitors, every one of the fourteen habitats from its fence, a food court, the
  visitor map, an information board, the pause menu.

The pictures are repeatable; from `build/`:

```bash
echo "40 shot ../build-probe/shots/lion.png" > /path/to/script.txt
SDL_AUDIODRIVER=dummy xvfb-run -a -s "-screen 0 1280x720x24" ./zoo-simulator \
    --frames 44 --lockstep --people 60 --script /path/to/script.txt --stand habitat:lion --hour 15
```

`--stand habitat:<species>` stands at the fence of that species' habitat where an animal is best
seen (squarely through one fence, some six metres off, nobody in front) and lists how many
animals of each habitat are outdoors; `--stand habitat:N[:species]` takes the N-th habitat with
an animal to be seen; `--stand board:N`
at an information board; `--view x,z,yaw,pitch` anywhere; `--hour` starts the visit at another
time of day. Each run prints `zoo: visit for ...` lines with the figures quoted in this document.
At 10:30 the lions sleep far from the glass; the picture of a lion at the glass was taken at 15:00.

Not verified: the frame rate on a real GPU (software rendering manages about 2 frames a second,
which says nothing); the sound by ear; a long play session by a person.

## Known limits

- Animals are stylised (above). The lion remains the best one.
- At 10:30 most predators sleep or rest, as they would; the zoo is livelier for the herbivores.
- No species has a swimming clip: a penguin or a polar bear in its pool is drawn with its
  walking and standing clips.
- Ostrich, kangaroo and polar bear have no call recorded; they are silent.
- One model per species except the lion (male, female, cub); other young animals are scaled adults.
- The resting poses of the zebra and the elephant (forequarters down, head up) look stiff.
- The ground texture is soft when one looks straight down; distant trees are simple.
- No third-person view (the brief called it optional).
- Keepers are not seen: the staff simulation exists as a contract only, care is automatic.
- The loading takes ten to fifteen seconds, most of it simulating the morning.
