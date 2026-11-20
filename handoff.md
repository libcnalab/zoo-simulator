# Handoff: Zoo Simulator (state of 2026-11-20)

For the next agent, who starts without any earlier context. Read this file first, then
`docs/REALISM.md` (progress log and the honest list of what is open), and the document of
whatever you take up (`docs/VISITOR_ZOO.md`, `docs/ANIMAL_BEHAVIOUR.md`, `docs/VISITORS.md`,
`docs/WEB.md`, `docs/PERFORMANCE.md`, `docs/CODEX_TASKS.md` for the history of delegated tasks).

## 0. Start here

- **The game:** Greenhill Zoo, a first-person walk through a zoo (C++23, the simulation core in
  `src/core/zoo/`, the game on CNA / cna-engine in `src/game/`). Zoo management must not come
  back as the primary gameplay.
- **State (end of 2026-11-14):** branch **`develop`** (the owner switched to it), pushed to
  `origin/develop` at the owner's request; `main` = `origin/main` = `0b13f7c` is behind. New on
  develop this session: keepers G5 (`e22d473`), animals' routes kept off props (`00e269f`),
  reliability audit part 1 with a save/load defect fixed (`7010638`), plants G2c (`f8d0adf`),
  ground and water edges G1c (`6985b10`), and the audit's walker corrections, start-up marks and
  `zoo_game_tests` (last commit). The current complete suite has 265 active tests and 21 intended
  disabled measurement/diagnostic tests; all active tests pass, including 13 walker tests.
- **The owner restarted the machine at about 20:00:** game instances of the lead and four
  subagents took 99 % of the GPU. The CPU lock covered builds, tests and benches but **not game
  runs**. New rule: **only one rendering `zoo-simulator` on the machine at a time**, every game
  run under the same lock (`flock .codex-work/cpu.lock`), always with `timeout` and a short
  `--frames`; no long runs under gdb. At most 2 subagents (owner's last word).
- **The owner's complaint: the game takes about 2.5 GB of RAM.** Memory is now measured by kind
  (`docs/RELIABILITY.md`): in this no-GPU sandbox the identical software-GL run fell 3,873 ->
  3,381 -> 2,870 MB after unused original animal textures and level-zero recovery shadows were
  released, then oversized animal atlases were capped at their 1K mip. That is 1,003 MB (25.9%)
  below the original run. Lion ownership output is pixel-identical; four representative 2K/1K
  viewer comparisons show no visible loss at 720p, and focused tests cover both ownership and the
  cap. The visit's shadow map is now 2K instead of 4K: its matched loading phase saves another
  96 MB, four zoo views show no visible loss, and the latest whole-run sample is 2,687 MB. The
  dual plant far forms are about 74-75 MB but buy about 1 ms/frame, so normal visits keep them;
  full-detail diagnostic mode now omits that unused second form. The under-1-GB target is not
  met. Visitor terrain now discards its 100 MB logical CPU edit cache; the ground mark fell
  875 -> 826 MB (49 MB resident), the latest whole-run sample is 2,682 MB, and editor memory is
  unchanged. An in-place save reload used to spike to about 3.1 GB because old software-GL static
  resources remained resident while replacements were built. This is now fixed on the game's
  side: a visit compares every render-relevant terrain, fence and placed-object field, keeps the
  uploaded ground and scenery when they are identical, and keeps animal species models while
  clearing their per-animal animation state. A two-reload run did no static or animal loading in
  either `Adopt`, ended at 3,033 MB instead of the earlier 3,120 MB software-GL sample, and its
  second reload added 9 MB. An editor zoo or any actual geometry edit still rebuilds. Audio now
  catalogues all 54 recordings but decodes a take only on first play: its loading phase fell
  46 -> 0 MB; a 60-frame run decoded 3 recordings,
  played 4 audible voices and had no failure. The noisy whole-run sample was 2,711 MB, so do not
  claim a total-peak cut from it. Do not quote the sandbox's absolute number as a Radeon
  measurement.
  The complete suite after these changes is 267 active tests passed, with 21 long-running
  measurement/diagnostic tests disabled as intended.
- Animal models are now submitted only when a generous body-and-shadow sphere intersects the
  main camera. The count fell from 88 everywhere to entrance 72, lion 7, meerkat 17, penguin 15
  and giraffe 46. All five screenshots were inspected (including edge bodies and the tallest
  shadows), and the 18 game-side tests pass; no frame-time saving is claimed because animals
  were already under 1 ms. A current AddressSanitizer lion-view smoke also passes
  (`docs/PERFORMANCE.md`, 2026-11-19).
- The stopped P1 visitor-cost work was recovered and reviewed. Visible feet are computed once,
  flat people are uploaded together into one retained dynamic buffer, and their shared shadow uses
  another; this removes about thirty temporary CNA buffers a frame without changing a coordinate.
  The 44,571,410-coordinate probe, entrance/lion screenshots, 18 game-side tests and an ASan
  full-crowd smoke pass. Do not claim an fps result: the current offscreen baseline spent 310 ms
  in its software driver (`docs/VISITORS.md`, `docs/PERFORMANCE.md`, 2026-11-19).
- Land plants now keep their whole circle off water and the eight neighbouring tiles whose shared
  corners a pool lowers. Both plan seeds pass the new invariant and the inspected flamingo view has
  clean water with planting retained on dry banks. Faster one-second obstacle recovery offsets the
  changed prop layout: animal stalls were 1.08% (longest 110 ticks), not the regressed 1.34% / 289.
  The plan tests and both fill-hash lengths pass (`docs/VISITOR_ZOO.md`, 2026-11-19).
- Trees now keep the planted strip's depth behind every viewing place, including slender trunks;
  if that would cross the vegetation welfare band the plan restores the former planting (Rhino
  Hollow and Red Dune). Elephant Valley, both fallback habitats and the new invariant were visually
  or automatically checked. The final layout has 2,601 props; stalls remain below the unchanged
  limit at 1.25%, and both fill-hash lengths were recorded. The changed deterministic animal
  history also exposed a brittle keeper comparison: its maxima now identify animal/habitat/tick,
  retain the 0.06 comparison near danger and otherwise remain at or below the 0.70 welfare line
  (`docs/VISITOR_ZOO.md`, `docs/KEEPERS.md`, 2026-11-19).
- Public one-tile walks are down from 48 paved tiles to 3 constrained remnants. Facility aprons
  widen after all sites are fixed (so all 41 buildings remain well distributed; farthest walk
  75.1 m), shade-tree islands keep a two-tile ring, dead raster teeth are removed, and thin runs
  widen only across unobstructed public lawn. Both seeds report the same three coordinates; the
  top-down plan and a food-court view were inspected. The changed deterministic routine history
  improves animal stalls to 0.88%, under the unchanged 1.3% limit; seed 1 had 2,579 props before
  the later reed and tall-grass planting brought the current total to 2,689
  (`docs/VISITOR_ZOO.md`, 2026-11-19).
- Reeds and tall grass now use their previously dormant procedural models and catalog entries.
  Flamingo Lagoon has twelve non-blocking reed stands rooted in its dry lowered margin, outside
  water and bench sight lines; non-blocking tall grass forms a lower layer in the open savanna and
  grassland habitats. Both plan seeds, both fill-hash lengths, all 269 active CTest cases and a
  real offscreen game load pass;
  the inspected evidence is `docs/evidence/reeds-grass-j8-2026-11-20.jpg` (J8, 2026-11-20).
- Every near oak, birch, willow, acacia and baobab now scatters 32 deterministic dry leaves around
  its roots; the far form consumes the same RNG but omits this sub-pixel detail. Seed 1 has 500
  affected trees: +32,000 scenery triangles and +16 draw calls over J8, while the far tally is
  unchanged. A direct model regression covers all five looks and the no-litter pine control; all
  270 active CTest cases pass (21 diagnostics disabled). The normal Tiger Forest view and close
  crop are inspected in `docs/evidence/leaf-litter-j9-2026-11-20.jpg` (J9, 2026-11-20).
- Adult and juvenile grey wolves now have a second-pass double coat and silhouette: a reusable
  height-to-normal stage packs an anisotropic guard-hair normal map and exports tangents; a fused
  neck ruff, lean shoulder transitions and a tapered rough brush replace the smooth chest, ball
  joints and cord tail. Triangle counts stay 18,528 / 15,528; the 3.09 / 2.97 MB production GLBs
  match independent fresh exports. All 120 animal-pipeline tests and all 270 active CTest cases
  pass (21 diagnostics disabled); both viewers plus a real Wolf Wood run after 3,000 ticks were
  inspected in
  `docs/evidence/wolf-coat-j10-2026-11-20.jpg` (J10, 2026-11-20). The anatomy remains stylised.
- Adult and juvenile Bactrian camels now have a second-pass winter coat and species details. An
  anisotropic normal map supplies longitudinal guard-hair relief; a fused rough neck mane and
  forelock, four-stage tail tuft and eight separate dark nails replace the flat coat, cord tail
  and bare pads. Both models have 21,760 triangles, 36 bones and nine clips; the 3.50 / 3.52 MB
  production GLBs match independent fresh exports. All 122 animal-pipeline tests and all 270
  active CTest cases pass (21 diagnostics disabled). The same-camera viewers and a real Red Dune
  run after 3,000 ticks are inspected in
  `docs/evidence/camel-coat-j11-2026-11-20.jpg` (J11, 2026-11-20). The face and base anatomy
  remain stylised.
- Adult and juvenile meerkats now have a second-pass short coat and working feet. Leaner torso
  and limb lofts replace the barrel body; every foot has four separate toes and four
  non-retractile claws, with longer front digging claws; an anisotropic normal map supplies short
  guard-hair relief. Both models have 11,648 triangles, 33 bones and ten clips; the 2.46 / 2.53
  MB production GLBs match independent fresh exports. All 124 animal-pipeline tests and all 270
  active CTest cases pass (21 diagnostics disabled). Sentry and dig were visually rechecked, and
  the same-camera viewers plus a real Meerkat Lookout run after 3,000 ticks are inspected in
  `docs/evidence/meerkat-coat-j12-2026-11-20.jpg` (J12, 2026-11-20). The head and base anatomy
  remain stylised.
- Adult and juvenile ostriches now have second-pass layered plumage. Four detached feathers
  overlap each folded wing and five more separate the tail fan; a feather-only tangent-space
  normal map adds directional barb relief while leaving the bare skin smooth. The adult has
  19,992 triangles and the chick 17,992; both retain 19 bones and nine clips. Their 2.79 / 2.86
  MB production GLBs match independent fresh exports. All 126 animal-pipeline tests and all 270
  active CTest cases pass (21 diagnostics disabled); `dust_bathe` was visually rechecked, and
  the same-camera viewers plus a real Ostrich Rise run after 3,000 ticks are inspected in
  `docs/evidence/ostrich-feathers-j13-2026-11-20.jpg` (J13, 2026-11-20). The egg-body anatomy
  remains stylised.
- Adult and juvenile red kangaroos now have a second-pass short coat and species-specific digits.
  Each hind foot has a dominant fourth digit, smaller fifth and tiny syndactylous inner II/III
  pair; five separate fingers replace each forepaw mitten. An anisotropic tangent-space normal map
  adds dense short-fur relief while the distal digits stay smooth and dark. The adult has 19,884
  triangles and the joey 19,384; both retain 28 bones and eight clips. Their 3.01 / 3.03 MB
  production GLBs match independent `OMP_NUM_THREADS=1` exports byte for byte. All 128
  animal-pipeline tests and all 270 active CTest cases pass (21 diagnostics disabled); hop, crawl
  and close digit views were inspected, along with a real Kangaroo Grove run after 3,000 ticks in
  `docs/evidence/kangaroo-coat-j14-2026-11-20.jpg` (J14, 2026-11-20). The face and lofted body
  remain stylised.
- All four bears have a second anatomy pass. Another 7-degree jaw closure removes the residual
  resting roar while preserving `vocalize`; local mesh relaxation softens the upright sculpt's
  angular reposed shoulders; 105 adult / 120 cub shorter outline clumps replace 90 long sparse
  spikes; and the polar face has quieter nose, mouth and lip pigment. Triangle, bone and clip
  counts are unchanged: brown adult/cub are 30,000/19,500 triangles and 5.58/5.12 MB, polar
  adult/cub are 30,000/19,500 and 6.16/5.78 MB. All four production GLBs and JSONs match
  independent exports byte for byte when both `OMP_NUM_THREADS=1` and Blender `-t 1` are used.
  All 129 animal-pipeline tests and all 270 active CTest cases pass (21 diagnostics disabled);
  `vocalize` and the same-camera viewers were inspected, as were real seed-1/tick-3000 Bear Falls
  and Polar Sound runs in `docs/evidence/bear-anatomy-j15-2026-11-20.jpg` (J15, 2026-11-20).
  Honest remaining limits: the crouched brown, slender converted polar and proportionally edited
  cubs all retain the same upright brown-bear source sculpt and its sharp hip anatomy.
- The tiger cheek ruff no longer consists of 72 wide neutral-grey cards in three rows. J16 uses
  150 narrower locks distributed irregularly over five depths, with a warm cream embedded atlas
  and sparse black/orange groups; only short tapered tips leave the white cheek silhouette. The
  tiger is now 32,702 triangles, 17,183 vertices and 3.18 MB, with its 33 bones and nine clips
  unchanged. Production GLB/JSON match an independent `OMP_NUM_THREADS=1`, Blender `-t 1` export
  byte for byte. All 130 animal-pipeline tests and all 270 active CTest cases pass (21 diagnostics
  disabled). Same-camera and close comparisons, `vocalize`, `bathe`, and a seed-1/tick-3000 Tiger
  Forest run are inspected in `docs/evidence/tiger-ruff-j16-2026-11-20.jpg` (J16, 2026-11-20).
  The alpha-tested fringe is still card hair, the tiger remains a conversion of the lioness
  sculpt, and the lion mane remains the cats' most visible close-up limitation.
- The male lion's J17 mane replaces 1,879 long, broad shingle-like mane cards with
  2,199 directionally varied curved locks over the same opaque undercoat (plus the unchanged
  42-card tail tuft). Mane cards span 1.5–2.3 cm and no more than 12.4 cm long, 288 smaller
  radial locks close the front, and 63 short cards lie back across the forehead. The lion is now
  41,854 triangles, 30,127 vertices and 3.92 MB, under the existing 42,000-triangle budget;
  33 bones and eight clips are unchanged. The production GLB/JSON match an independent
  deterministic export byte for byte. Same-camera,
  side and close comparisons, `vocalize`, `sleep` and an internal seed-1/tick-3000 Lion Rock
  runtime check are inspected in `docs/evidence/lion-mane-j17-2026-11-20.jpg` (J17, 2026-11-20).
  All 131 animal-pipeline tests and all 270 active CTest cases pass (21 diagnostics disabled).
  Cards remain detectable at extreme close range and the head is still a widened lioness sculpt.
- J18 adds the catalog's first `dry_moat` / ha-ha barrier (saved kind 15): an effective 2.4 m,
  strength-5 retaining wall with an unobtrusive 0.29 m coping and full viewing visibility.
  Elephant Valley and Rhino Hollow use their existing two-tile stand-off as a four-metre rocky
  V-ditch, lowered 1.8 m only along vertices surrounded by four reserved tiles. Habitat and public
  path lips therefore keep their exact grades, areas and welfare. Their service aprons and keeper
  gates retain cable barriers. Both plan seeds keep all 999 strip plants, containment, visibility,
  save/load and keeper access; the steepest path remains 2.8/2.7 %. Seed 1 after 3,000 ticks is
  inspected at both habitats in `docs/evidence/dry-moats-j18-2026-11-20.jpg` (J18, 2026-11-20).
  Short and 6,000-step state hashes repeat for seeds 1–3. All 270 active CTest cases pass
  (21 diagnostics disabled).
- The stopped G7c audio audit is recovered and finished. Calls now follow `Vocalize`, social,
  play and documented feeding cues rather than a timer; takes do not repeat immediately, spatial
  pan is capped at 0.60, birds fade with daylight, crowd level follows its people, and footsteps
  are levelled by surface. A public-domain fountain loop plays at the two fountains, the park bed
  no longer masks low animal calls, and the uncertain bear cut is a stronger measured low event
  from the same NPS source. All 27 disk-driver captures have zero clipped samples and zero
  immediate repeats; the dusk wolf/lion chorus peaks at -1.2 dBFS. Nobody listened: conclusions
  are measurements and inspected spectrograms only (`docs/AUDIO.md`, 2026-11-20).
- The stopped A1 activity-clip work is recovered, regenerated and reviewed. Meerkats now stand
  sentry and dig; penguins and flamingos preen; ostriches and elephants dust-bathe. Eight model
  variants carry the new clips and manifest mappings. The 100 pipeline tests and 20 game-side
  tests pass, all viewer and habitat evidence was opened, and the two stale stopped-worktree GLBs
  were replaced by byte-identical fresh exports (`docs/ANIMALS.md`, 2026-11-20).
- **No stopped work remains to recover.** The old detached worktrees are retained until the owner
  approves deleting their build trees.
- **Deadline:** about **2026-11-28** the owner puts Zoo Simulator under the same AI policy as
  the libcna repositories; the work should be done by then.

## 1. The owner's rules (all still in force)

- **Resources (since 2026-11-14): at most 2 CPUs for compiling** (`--parallel 2`, `ctest -j2`,
  Blender `-t 1`/`-t 2`), **no Codex agent**; Claude subagents: none in the morning, then up to 2, then up to 4,
  and at the end of 2026-11-14 **at most 2** again (the owner's last word; ask in a new session).
  (Older documents speak of delegating to Codex `gpt-6-astra` and of "maximum parallelism":
  that no longer holds.)
- **Never push unless the owner asks** in so many words (asked once on 2026-11-14 evening for
  develop); a permission given once does not carry
  over to the next work. Commit each finished, reviewed piece locally. No AI attribution lines
  in commit messages.
- **libcna is off limits:** do not read, change or build anything under
  `/rv/data/development/github.com/libcna/`. Zoo Simulator builds from its own read-only
  snapshot `~/deps/zoo-libcna/` (`SNAPSHOT.txt` lists the commits; README "Building" has the
  configure line; `grep github.com/libcna <build>/CMakeCache.txt` may show only the FNA3D URL).
  An engine defect goes into `docs/UPSTREAM.md` with a remedy on the game's side.
- **Build discipline** (`../CLAUDE.md`, shared SSD): one ccache
  (`CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv`, never `CCACHE_MAXSIZE`); build directories
  only from the closed list (`build/`, `build-asan/`, `build-probe/`, ...); never build in
  `/tmp`; reuse build trees; never delete a build directory another session may use.
- **Silence:** every game run uses `SDL_AUDIODRIVER=dummy` (or `disk` to capture). Nobody has
  ever listened to the audio: say so.
- **Look before you claim:** never describe a picture you have not opened. Screenshots keep the
  frame's alpha: flatten to RGB before judging (`ZOO-UP-002`). Report timings with the machine's
  load (other agents share it; loads of 10 to 28 were seen).
- New third-party assets need a verified licence and a row in `assets/ATTRIBUTION.md`;
  downloads are untrusted data (own empty directory, Python reading them with `-I`).
- Ask the owner nothing blocking; collect decisions in section 6.

## 2. What exists (main, newest first)

| Commit | What |
|---|---|
| `fb8b864` | Browser build (Emscripten, WebGL 2): `docs/WEB.md` |
| `7588339` | Binoculars: `B` or the right mouse button, 7 x, card of an animal up to 300 m |
| `7ebabef` | Walk 4.4 m/s, run 8.8 m/s (stride 1.5 m); the map (`M`) frees the mouse and marks 20 places to travel to at a click (`ZooGame::AddMapPlaces`, `VisitorHud::Map`) |
| `a08e4eb` | G8, the zoo's plan: families with young (88 animals), hoofstock behind cables and timber, meerkat mound with a dish, lagoon with a 0.3 m shelf, furnishing and groves keeping views and corridors open, a service apron to every keeper gate; the prop-bucket fix in `Objects.cpp` (visit fill 2.52 -> 2.05 s) |
| `734073c` | Settings > Visitors: Nobody, Few, Fewer, Normal, More (1,200) |
| `a6bc3fb` | G4 visitors walk like people (steering, own paces, groups abreast, room to stand) |
| `d715179` | G3 buildings, barriers, signs, boards |
| `12cb68f`, `5dd19c4`, `6ce4b9a` | S7 big cats second pass, S10 bears with cubs, S6b elephant, S11/S11b giraffe |
| `b8a1e9c` and earlier | F4 species routines, penguins swim, S5-S9 animal models, E1 habitat fronts, foliage, sky light (`docs/REALISM.md` "Done") |

Controls: `W A S D` walk, mouse look, `Shift` toggles running, `E` reads a board, `B` / right
button binoculars, `M` map (click a marker to travel), `Esc` pause, `F5` save, `F4` diagnostics.

**Tested only by script, never by a person with a real mouse:** map travel, binoculars, the
faster pace. **The web build** ran only in headless Chrome driven over the DevTools protocol
(`build-probe/web/drive.mjs`, `drive2.mjs`); never in a visible browser (fps, pointer lock,
sound unknown); saves there live only in the page's memory.

## 3. Verification recipe for any change

1. Build: `cmake --build build --target zoo-simulator zoo_core_tests --parallel 2`.
2. Focused tests, then the full suite `ctest --test-dir build -j2 --output-on-failure`
   (221 tests, about 5 to 7 minutes; do not rebuild while it runs).
3. **Golden hashes** (`tests/core/VisitStartTests.cpp`) change with any behaviour change of the
   core: print with
   `./build/tests/zoo_core_tests --gtest_filter='VisitStart.*Hashes*' --gtest_also_run_disabled_tests | grep ^HASH`,
   paste them, add a line to the comment above them, record the table in the document of the
   change. A pure caching or speed fix must leave them unchanged (that is its proof).
4. Look in the game (`--stand`, `--view`, `--script`, section 7) and open every screenshot.
5. Docs: the feature's document, `docs/REALISM.md` (Done / Open), an evidence sheet in
   `docs/evidence/` for visible changes, this file.
6. Format: core files follow `.clang-format` (`clang-format-19 -i`); **the game files under
   `src/game/` were never formatted with it** — do not run it on them (it rewrites unrelated
   lines); match the surrounding style by hand.

## 4. Remaining work (priority order)

### 4.1 to 4.3: done on develop (2026-11-14)

Keepers (`docs/KEEPERS.md`; open: no collision with animals and visitors, dangerous habitats
never entered), ground and water edges (`docs/GROUND.md`; open: steep banks at deep pools, props'
collision floor still at the simulation's height in `Walker.cpp`), plants (`docs/PLANTS.md`;
open: far cut-out edges aliased, reeds unused, fill cost of alpha-tested cards). The new first
priority is memory (section 0).

### 4.4 Reliability audit (G6b): merged, see `docs/RELIABILITY.md` top

`.codex-work/wt-audit` (on `f77bc8f`, uncommitted; it has a 4 GB ASan tree), brief
`brief-G6b-audit-finish.md`; it was correcting `docs/RELIABILITY.md` and running save-damage
tests. Take its tests and document if sound; add the known ASan smoke (the game ran clean once:
`--stand habitat:lion --frames 30` in `build-asan`).

The stopped wall probe was rerun directly at the gift shop's west wall. At the body's closest
valid centre, the 0.3 m camera near plane cut away a quarter of the wall at a 75-degree view;
0.1 m keeps the whole wall visible. The before/after contact pictures and a normal lion view
were inspected offscreen (2026-11-19).

Root `build-asan/` was rebuilt on current `develop` (Debug, AddressSanitizer). A targeted 30-test
set covering births/life, the 12,000-step stall regression, every damaged-save test and all
thirteen Walker tests passed in 626.21 s; `--stand habitat:meerkat --ticks 3000 --frames 3`
also completed cleanly with 88 animals and 624 visitors. Leak detection itself cannot run under
this managed session's ptrace wrapper; UBSan and the entire current suite remain unrerun
(2026-11-19).

### 4.5 Audio audit (G7)

Recovered and completed on current `develop`; the implementation, per-capture measurements,
species/source table, open limitations and inspected spectrogram sheet are in `docs/AUDIO.md`.
The 62 audio-tool tests, 383 recording assertions, warning-clean game build and all 20 game-side
tests pass. Nobody listened to the audio; preserve that qualification.

### 4.6 Animals

- First-generation stylised anatomy still: wolf, camel, meerkat, ostrich, kangaroo. All five now
  have second-pass coats or plumage and silhouette details, but are not scan-based. Bears have
  a second-pass resting face, shoulders and outline fur, but remain visibly converted from an
  upright brown-bear sculpt (`docs/ANIMALS.md`; pipeline in `tools/animals/`, its tests
  `python3 -m unittest discover -s tools/animals/tests`, about 10 minutes).
- Every young animal used in the Visitor Zoo has a dedicated variant; the brown/polar bear cub
  models exist but are unused (bears live as pairs).
- `wolf_juvenile` is now a real older-pup variant rather than a scaled adult: 0.46 m shoulder,
  shorter body and muzzle, broader skull, larger eyes, ears and paws, lighter coat, 15,528
  triangles, 33 bones and eight clips with pup-scaled motion. That J1 production pup matched an
  independent fresh export, and its real Wolf Ridge run selected the model for one-year-old
  Rosa. J10 subsequently gave both ages a double-coat normal map and improved ruff, shoulder and
  tail geometry; see the newer note above. The inspected J1 comparison and habitat evidence is
  `docs/evidence/wolf-juvenile-j1-2026-11-20.jpg`.
- `meerkat_juvenile` now replaces the scaled adults used for both 0.15-year-old pups in the
  Visitor Zoo: 0.17 m shoulder, rounder and proportionally broader skull, 72 % face length,
  larger eyes, ears and paws, lighter coat, 8,576 triangles, 33 bones and ten clips. Its
  juvenile-scaled sentry and dig retain grounded hind feet, paws and tail. The adult export
  remained byte-for-byte identical, the production pup matches an independent fresh export,
  and all 109 pipeline tests pass. A real habitat run loaded the variant and selected
  two-month-old Pavel; the inspected evidence is
  `docs/evidence/meerkat-juvenile-j2-2026-11-20.jpg` (J2, 2026-11-20).
- `ostrich_juvenile` now replaces the scaled black-and-white adult males used for the three
  0.3-year-old chicks at Ostrich Rise: 1.13 m head height, shorter neck and face, larger round
  head, eyes and feet, compact wing/tail forms, brown striped down, 14,560 triangles, 19 bones
  and nine clips including juvenile-scaled dust bathing. The adult export remained byte-for-byte
  identical, the production chick matches an independent fresh export, and all 110 pipeline
  tests pass. A real habitat run loaded the variant and selected four-month-old Anika; the
  inspected evidence is `docs/evidence/ostrich-juvenile-j3-2026-11-20.jpg` (J3, 2026-11-20).
- `camel_juvenile` now replaces the scaled adults used for the two young animals at Red Dune:
  1.26 m shoulder, a slimmer barrel, shorter neck and face, larger head, eyes, ears and pads,
  low developing humps and a lighter coat. It has 20,640 triangles, 36 bones and nine clips,
  retaining the adult's dedicated `ruminate` activity with calf-scaled locomotion. The adult
  export remained byte-for-byte identical, the production calf matches an independent final
  export, all 111 pipeline tests and all 267 active CTest cases pass, and a real habitat run loaded
  the variant and selected nine-month-old Ivo. The inspected evidence is
  `docs/evidence/camel-juvenile-j4-2026-11-20.jpg` (J4, 2026-11-20).
- `kangaroo_juvenile` now replaces the scaled adults used for the two joeys below half the age of
  maturity at Kangaroo Grove: 0.89 m head height, a shorter face, larger head, eyes, ears and
  long feet, slimmer body, less muscular tail and thighs, and a separate grey-brown coat. It has
  16,304 triangles, 28 bones and eight clips; the 0.58 m/s crawl remains supported on the hands,
  feet and tail and the 2.2 m/s hop keeps both hind feet synchronized. The adult export remained
  byte-for-byte identical, the production joey matches an independent final export, all 112
  animal-pipeline tests and all 267 active CTest cases pass. A real habitat run loaded the variant
  and selected eight-month-old Nadia; the inspected evidence is
  `docs/evidence/kangaroo-juvenile-j5-2026-11-20.jpg` (J5, 2026-11-20).
- `flamingo_juvenile` now replaces the scaled pink adults used for the 0.8- and 0.9-year-old birds
  at Flamingo Lagoon. It retains the licensed scan-derived anatomy at 86 % height (1.20 m) and has
  separate light grey-pink first-year plumage, grey legs and bill, 12,354 triangles, 29 bones and
  all eleven adult clips. Its 0.60 m/s walk, 1.25 m/s trot, lifted-leg poses, feeding targets and
  activity motions scale with the variant. The adult export remained byte-for-byte identical and
  the production juvenile matches an independent final export; all 113 animal-pipeline tests and
  all 267 active CTest cases pass. A real game run loaded the model and selected ten-month-old
  Aya. The inspected evidence is `docs/evidence/flamingo-juvenile-j6-2026-11-20.jpg`
  (J6, 2026-11-20).
- Zebra rest is no longer held like a statue: all three variants breathe, scan, flick their ears
  and sway their tails while the four folded hooves remain fixed. The elephant family's broken
  rolled-side sleep (belly towards the visitor, four soles spread around it) is replaced by a
  supported standing doze with lowered head, quiet ears, relaxed trunk and one unweighted hind
  foot. Both are real rest strategies within the rigs' limits; the six production GLBs match an
  independent fresh export, and all 115 animal-pipeline tests and all 267 active CTest cases pass.
  The inspected before/after, variants and real 18:00 run selecting sleeping Nora are in
  `docs/evidence/resting-poses-j7-2026-11-20.jpg` (J7, 2026-11-20).
- Dedicated clips now cover meerkat sentry/dig, penguin and flamingo preening, flamingo filter
  feeding, camel and giraffe rumination, ostrich and elephant dust-bathing, and shallow wallowing
  for bears, tigers and rhinoceroses (elephants reuse their scoop/toss as a wet toss). Penguin
  huddling, flamingo huddling, giraffe browsing and zebra watch now also have dedicated clips for
  every model variant. Lion contact and all synchronised multi-animal contact remain nearest
  individual poses. The exact table and honest visual limitations are in `docs/ANIMALS.md`.
  A5's eight production files match independent exports byte for byte; 107 pipeline tests pass.
  The renderer integration's warning-clean build and all 267 enabled CTest cases passed (the
  pipeline was rerun after the last visual refinement). The inspected evidence is
  `docs/evidence/animal-clips-a5-2026-11-20.jpg` (2026-11-20).
- Blocking-prop pockets are now rejected by `ClearPlace`: every routine destination needs two
  body-wide exit segments clear of fences, water and blocking props. Seed 1 fell from 4,183 of
  280,044 stalled moving ticks (1.49 %) to 2,994 of 270,133 (1.11 %), with no continuous stall
  of 300 ticks. The former probe is the active regression
  `AnimalMovementTests.VisitorZooAnimalsDoNotRemainStalledInPropPockets` in
  `tests/core/AnimalStallTests.cpp` (2026-11-19).
- The changed animal positions exposed a visitor leaving a picnic table by an unrelated path
  across 19 ticks of lawn. When a generic nearest-path search would carry somebody more than
  3.5 m from a nearby bench, `Stepper::Route` now uses that bench's recorded path access;
  `VisitorDayTests.ADayInTheStarterZoo` is back to zero astray visitor-ticks (2026-11-19).

### 4.7 Performance

- Loading about 5.5 s, 2.4 s of it painting the ground (in the browser 7 to 18 s, one thread).
- 800 visitors cost about +3 ms a frame since G4, cause not found (`docs/PERFORMANCE.md`;
  rebuild `d715179` for the "before" binary).
- During the visit's fill the visitors' routing and watching are now the largest part (gdb
  stack sampling, `docs/VISIT_START.md`; there is no `perf` on this machine).

### 4.8 Plan details left from G8 (`docs/VISITOR_ZOO.md` "The zoo's plan")

No dry moat or ha-ha in the catalog; three constrained paved tiles remain only 2 m wide; slender
front trees remain where a full set-back would cross the vegetation welfare band (Rhino Hollow
and Red Dune); the hedge fence is still unused by a habitat; area per animal is an estimate, not
EAZA/AZA figures.

`--stand habitat:<species>` now rejects keeper gates and buildings in the sight line. The
meerkat view moved from `(177.0, 263.2)`, looking through mesh into the shelter wall, to
`(169.2, 269.0)`, with an unobstructed animal in the centre; the before/after offscreen images
were inspected and the 18 game-side Walker/texture tests passed (2026-11-19).

The same command also rejects positions overlapping the exact solid radii used by `Walker`,
including its 0.28 m body radius and furniture which simulation visitors may pass through.
All fourteen species kept identical requested/final coordinates to 0.1 m in a zero-tick
offscreen smoke, so the former 42 set-downs inside furniture are resolved (2026-11-19).

## 5. Worktrees in `.codex-work/` (git-ignored)

Still holding work to take: **none**. Briefs `brief-<id>.md`, reports `report-<id>.md`, logs
`log-<id>.txt` beside the retained worktrees are historical records.

Nothing left to take (their build trees can go once the owner agrees; sizes of 2026-11-14):
`wt-plants` (A1), `wt-sound` (G7c), `wt-life` 598 MB (G8), `wt-built` 716 MB (G3), `wt-visitors` 715 MB
(G4), `wt-cats` 251 MB (S7),
`wt-zebra` 272 MB (S10, S8), `wt-pachy` 279 MB (S6b), `wt-giraffe` 338 MB (S11b), `wt-birds`
158 MB, `wt-behave` 349 MB, `wt-audio` 221 MB, `wt-species` 469 MB, `wt-vzoo` 360 MB,
`wt-world` 293 MB. Kept patches: `G8.patch`, `zebra.patch`, `birds.patch`, `giraffe.patch`,
`S6-final.patch`, `behave.patch`, `e1.patch`, `S6-snapshot.patch`.

Worktree build trees point at the snapshot too; `tools/build/use-libcna-snapshot.sh <source>
<build> <log>` re-points another one.

## 6. Decisions for the owner (collect; do not block)

- Licences before any distribution: rhino and elephant sources are CC BY 4.0 as published but
  their authorship is unconfirmed (`assets/ATTRIBUTION.md`); the polar bear's CC BY-SA source
  (`build-probe/model-research/polarbear-cedes-bysa/`) is unused until share-alike is decided;
  the penguin is a Magellanic specimen's shape painted as an African penguin (labelled so).
- A top-level `build-web/` for the browser build (now in `build-probe/web/`, 594 MB, because the
  closed list of build directories has no web entry).
- Deleting the finished worktrees' build trees (section 5, about 5 GB).
- ZOO-UP-001 (cna-engine shadow map), ZOO-UP-002, ZOO-UP-003 (`docs/UPSTREAM.md`): remedied
  on the game's side; for the engine's owner.

## 7. Commands

```bash
cd /rv/data/development/github.com/openeggbert/zoo-simulator
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
cmake --build build --target zoo-simulator zoo_core_tests --parallel 2   # build/ is RelWithDebInfo
ctest --test-dir build -j2 --output-on-failure                           # 221 tests, ~5-7 min

# The game: real GPU, no window, no sound; run from build/ (build/assets is a copy made by the
# build: rebuild after changing assets/). Filter the GLTF warnings.
cd build
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy timeout 180 ./zoo-simulator \
    --stand habitat:lion --ticks 3000 --frames 40 --screenshot ../build-probe/shots/x/lion.png 2>&1 | grep -v WARN
#   --view x,z,yawDeg,pitchDeg   (yaw 180 looks north; to the right the heading falls)
#   --viewer --animal <id> --clip walk --angle 35 --frames 30 --screenshot ...   one model alone
#   --bench --frames 300   fps, frame-time percentiles, draw calls, memory
#   --hour H  --people N  --seed S  --without animals|people|scenery|...  --menu (start at the title)
#   --script FILE --lockstep --frames N   scripted input, 1/60 s a frame (src/game/InputScript.hpp):
#       "30 key M", "40 click 807 350", "31 hold W", "45 shot ../build-probe/shots/x/a.png"
#   Window pixels are 1280 x 720: take click coordinates from a full-size screenshot, not
#   from a shrunken contact sheet.

# The plan from above as a picture, and the plan's figures:
./build/tests/zoo_core_tests --gtest_filter='VisitorZooPlanTests.DISABLED_WritePlanImage' --gtest_also_run_disabled_tests
./build/tests/zoo_core_tests --gtest_filter='VisitorZooTests.DISABLED_PrintPlan' --gtest_also_run_disabled_tests
# Visit-fill timing (interleave before/after binaries, report the load):
./build/tests/zoo_core_tests --gtest_filter='VisitStart.DISABLED_FillTiming' --gtest_also_run_disabled_tests
```

Browser build: `docs/WEB.md` (configure once with `emcmake` into `build-probe/web/`; rebuild
`cmake --build build-probe/web --target zoo-simulator --parallel 2` after `source
~/emsdk/emsdk_env.sh`; about a minute after a game change). To try it headless: serve
`build-probe/web/web-package/` with `python3 -m http.server 8765 --bind 127.0.0.1` and run
`~/emsdk/node/24.19.0_64bit/bin/node build-probe/web/drive2.mjs
'http://127.0.0.1:8765/?arg=--stand&arg=habitat:elephant&arg=--frames&arg=1000000' shots`
from `build-probe/web/` (Chrome flags `--use-angle=gl-egl --ozone-platform=headless`; game
arguments go in the URL as `arg=`). Stop the server and delete `build-probe/web/chrome-profile`
afterwards.

The end of a game run prints `looking at "<name>, <species>" (<activity>)`; automated runs
also print `zoo: map: N places` and `zoo: went to <place>`. The core runs at 20 ticks a second;
a calendar year has 48 days (a week ages an animal 0.146 years). Shell note: each shell command
may start in another directory; begin each with `cd`.
