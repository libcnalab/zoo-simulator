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
  measurement. J47's bear cubs now make all 33 currently needed animal variants legitimate.
  J53 releases each importer's bone timelines after copying them into the clips the actors use:
  the current matched software-GL sample fell 2,848 -> 2,799 MB resident (heap 2,499 -> 2,457),
  and the isolated animal loading mark fell 2,601 -> 2,550 MB. A 3,000-tick Lion Rock run and its
  frame were clean, as was the equivalent three-frame ASan/UBSan game smoke. J54 also drops the
  smaller CPU mip staging levels after their GPU upload on desktop Linux only: the same profile
  falls 2,799 -> 2,760 MB (heap 2,457 -> 2,405), and the animal mark 2,550 -> 2,486 MB. Other
  platforms retain context recovery and readback. J55 applies the same completed-GPU-chain
  operation to the read-only desktop terrain: the immediate sample falls 2,760 -> 2,665 MB
  (heap 2,405 -> 2,301). The editor and other platforms retain their staging.
  J56 applies it to immutable procedural surfaces and the completed far-plant palette, and also
  discards the palette's source image and miniatures. Exact known CPU allocations removed are
  10.33 MiB; contradictory short-run RSS samples are not claimed as a saving. Water animation,
  editor mutation and same-zoo reloads remain intact.
  J57 identifies the next large floor: CNA retains CPU shadows for write-only static vertex and
  index buffers. The new `--bench` line measures 1,023 owned meshes, 112.2 MiB vertex and 20.7 MiB
  index data (132.9 MiB total, excluding imported borrowed buffers). Zoo cannot safely release
  these without a CNA API; ZOO-UP-004 specifies one.
  The complete suite after these changes is 305 active tests passed, with 21 long-running
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
  the later reed and tall-grass planting brought the then-current total to 2,689
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
- J19 gives staff physical space in the simulation. Visitors steer around the current position of
  every keeper, gardener and cleaner in their walking region; moving staff slide around visitors,
  visible animals and one another, and may only separate from an overlap. A seed-1 morning over
  3,600 measured ticks exercises 2,653 close visitor and 75 close animal steps; moving visitors
  remain at least 0.383 m from the staff position they saw. Short and 6,000-step hashes repeat for
  seeds 1–3, 269 active CTest cases pass, and the previously run `zoogen_animation` also passes
  (21 diagnostics disabled). The two inspected runtime contexts are in
  `docs/evidence/staff-personal-space-j19-2026-11-20.jpg` (J19, 2026-11-20). Shy animals still do
  not avoid staff, and dangerous habitats still cannot be entered.
- J20 resolves J19's remaining shy-animal limit. Staff positions inside a habitat are a transient
  input to `Animals`; species above 0.3 shyness choose a clear four-metre retreat when a keeper
  enters their body-scaled flight distance, without interrupting urgent food or water. A direct
  zebra/flamingo regression proves the threshold, the seed-1 morning's staff/animal approaches
  inside body clearance fall 75 -> 0, stalls improve to 0.758 %, and the staffed week remains no
  worse than automatic care. The same-camera runtime comparison is inspected in
  `docs/evidence/shy-animal-keeper-j20-2026-11-20.jpg` (J20, 2026-11-20). Short and 6,000-step
  hashes repeat for seeds 1–3; all 270 active functional CTest cases pass, as did today's separate
  six-minute `zoogen_animation` run (21 diagnostics disabled). Dangerous habitats still cannot be
  entered and bold species at or below 0.3 do not retreat.
- J21 fixes the single-position outside round at strip-ringed dangerous habitats. Barrier work now
  draws from both the direct service-gate stands and every reachable public viewing bay instead of
  returning after the first category. In the new 12,000-tick regression Elephant Valley and Rhino
  Hollow rise from one distinct work place to at least two (Lion Rock already had two), while all
  positions remain on public ground. Keeper 7 checking Elephant Valley from its dry-moat gallery is
  inspected in `docs/evidence/safe-barrier-work-j21-2026-11-20.jpg` (J21, 2026-11-20). Dangerous
  habitats still cannot be entered without an animal lock-in model. Short and 6,000-step hashes
  repeat twice for seeds 1–3; all 271 active functional CTest cases pass, as did today's earlier
  separate `zoogen_animation` run (21 diagnostics disabled).
- J22 makes the four-person staff cast repeatable across processes. `StaffAppearance` feeds CNA's
  current avatar generator a fixed per-slot stream, keeping the alternating female/male bodies but
  removing `CreateRandom`'s process entropy. A direct test proves all four public 1,021-byte
  descriptions are valid, distinct, correctly typed and exactly repeatable; all 22 game-side tests
  pass. The two separately rendered keeper-7 crops in
  `docs/evidence/repeatable-staff-cast-j22-2026-11-20.jpg` have the same face, hair, build, height
  and uniform; only the gait phase differs with real loading time. All 272 active functional CTest
  cases pass, as did today's earlier separate `zoogen_animation` run (21 diagnostics disabled; J22,
  2026-11-20).
- J23 gives every staff tool a soft contact shadow projected onto the ground from the sun direction
  and the tool's height. A long rake or broom uses three joined patches; a barrow-and-rake pose adds
  at most 210 simple transparent triangles and no draw call because it shares the existing crew
  shade pass. The inspected morning frame is
  `docs/evidence/staff-tool-shadows-j23-2026-11-20.jpg` (J23, 2026-11-20). The separate shadow-map
  pass is deliberately not used for these small CPU-built tools. A targeted off-screen run finds
  keeper 6 raking at tick 12,000 and all 272 active functional CTest cases pass (21 diagnostics
  disabled; today's separate `zoogen_animation` run already passed).
- J24 removes two unused animal-model allocations from the ordinary visit. Female and juvenile GLBs
  are now imported independently on the first current animal which needs them, rather than both on
  first sight of the species; later births still take the same path. Greenhill has no bear cubs, so
  `brown_bear_juvenile` and `polar_bear_juvenile` are no longer among its imports: 33 -> 31 models,
  model import/mipmap time 3.027 -> 2.748 s in the matched one-frame logs. The immediate
  `tools/bench/memory.sh 2 --bench --lockstep` software-GL run fell 2,796 -> 2,736 MB resident and
  2,461 -> 2,394 MB heap. Treat that 60 MB as a matched sandbox saving, not a Radeon absolute
  figure. The warning-clean game build and all 272 active functional CTest cases pass (21
  diagnostics disabled; J24, 2026-11-20).
- J25 closes the stale entrance-board decision from the reliability audit. A board farther than
  eight metres from every lived-in habitat already used general visitor guidance; its panel now
  also gives the simulation's real 9:00–19:00 opening hours, 16:30 last admission and live animal
  and habitat counts. Both entrance boards were opened in separate runs, and board 2
  retained its full Meerkat Lookout species, resident and feeding content. The inspected panel is
  `docs/evidence/entrance-welcome-board-j25-2026-11-20.jpg`; all 272 active functional CTest cases
  pass (21 diagnostics disabled; J25, 2026-11-20).
- J26 makes breeding respect the habitat the young will actually occupy. Conception now reserves
  the minimum litter against group size, shelter and space, including other pregnancies in a mixed
  habitat; birth caps a larger random litter to the remaining capacity and waits if even the
  minimum was consumed after conception. A small-shelter regression limits a 1–20 chick ostrich
  clutch to one and then prevents another pregnancy. The formerly permissive Visitor Zoo week
  check is strict after births: seed 1 ends day 7 with 91 animals and three young, all habitats
  still fully sheltered and at space 1. The disabled long-visit diagnostic also passes after
  172,800 steps with 102 animals, 14 births, no death or escape and no capacity failure. Short and
  6,000-step hashes for seeds 1–3 remain unchanged; all 274 active CTest cases pass, including the
  separate animation pipeline (21 diagnostics disabled; J26, 2026-11-20).
- J27 implements the reliability audit's filled-zoo cache. A normal visit stores its deterministic
  built-and-filled state under the save directory's private `.cache/`; keys separate seed, crowd,
  hour, ticks, available models, save/game versions and all construction/species definition bytes.
  Invalid entries rebuild atomically, only eight variants remain, and automation opts in with
  `--saves`. Matched software-GL runs loaded the identical state hash `2609457543749151601` in
  0.04 s instead of 2.58 s for build/fill (64.5x), cutting startup through visitors/tools
  8.15 -> 5.65 s. A different crowd made a separate cache, and a deliberately truncated one was
  rejected and rebuilt to its earlier hash. All 273 active C++/game CTest cases pass; together with
  the unchanged animation pipeline, 274 active tests pass and 21 diagnostics are disabled
  (J27, 2026-11-20).
- J28 removes full-save parsing from the ordinary load-menu path. Every successful `SaveZoo`
  writes a small derived `<save>.summary` after the authoritative file is committed; its exact
  size and file timestamp reject stale data. Missing, damaged or stale sidecars fall back to the
  full validated save, then repair themselves; oversized sidecars are bounded before parsing, and
  a sidecar write failure never fails the save. On the 2,374 KiB Visitor Zoo diagnostic, forced
  cold reads took 24.1--24.4 ms and cached reads 34--50 microseconds (about 500--700x faster).
  New tests cover creation, loss, staleness,
  corruption, repair and interaction with truncated saves (J28, 2026-11-20).
  The full suite passes: 275 active tests including the six-minute animation pipeline, with 21
  diagnostics disabled as intended.
- J29 replaces Meerkat Lookout's exposed service-side wire mesh with a 1.8 m clipped wildlife
  hedge containing a mostly hidden welded mesh and buried edge. New stable fence kind 16 has
  strength 2; the five meerkats retain containment 1.0, while the glass public front, 252 m² area,
  28 viewing tiles and furnishings remain. Save format 3 prevents an older reader from accepting
  the new kind; formats 1 and 2 still load. Both plan seeds and the version regression pass, short
  hashes are unchanged, and rebaselined 6,000-step hashes repeat twice for seeds 1–3. Both opened
  runtime views are in `docs/evidence/wildlife-hedge-j29-2026-11-20.jpg`. The full suite passes:
  277 active tests including the six-minute animation pipeline, with 21 diagnostics disabled as
  intended (J29, 2026-11-20).
- J30 removes the last three ordinary 2 m path remnants. The food-court ellipse extends one tile,
  the public rail recesses around one outer planting tile to make the shared Elephant Valley /
  Kangaroo Grove approach at least 4 m wide without moving either habitat, and an accidental
  diagonal path spur is closed through Ostrich Rise's planted stand-off. Both seeds now report
  zero narrow ordinary tiles (down from the original 48); Kangaroo Grove retains 1,136 m², 64
  viewing tiles and seven residents, all 14 habitat areas remain unchanged, and the farthest
  facility stays at 75.1 m. Walker traverses all 5,013 path tiles / 10,024 neighbour hops. The
  changed crowd exposed two staff-avoidance gaps: final arrival steps now avoid bodies, composed
  slides receive a last all-body check, and staff wait inside a shy animal's 8 cm reaction margin.
  The morning regression again has zero animal contact without relaxing assertions. Short and
  6,000-step hashes repeat twice for seeds 1–3; a separate plan regression preserves the recessed
  lane, its two planted buffers and two rails. The full suite passes: 279 active tests including
  the 364.99-second animation pipeline, with 21 diagnostics disabled as intended. Opened views are
  recorded in `docs/evidence/wide-walks-j30-2026-11-20.jpg` (J30, 2026-11-20).
- J31 removes the final tree-view exceptions at Rhino Hollow and Red Dune. Every trunk at every
  habitat now keeps five metres plus the planted strip's depth from every viewing tile. Where that
  set-back approaches the welfare band, deterministic low-layer planting fills free beds first:
  grass and savanna scrub retain Rhino Hollow's 0.140 cover, and new steppe grass retains Red
  Dune's 0.125 cover while its palms stay behind the crown line (guarded minimum 0.120). Seed 1
  has 2,752 props (+47 low plants); all areas, water, residents and views remain unchanged. The
  stall probe remains under its 1.3% limit at 2,988 / 293,364 moving ticks (1.019%). Both short
  and 6,000-step hashes repeat twice for seeds 1–3. All 280 active functional CTest cases pass in
  106.99 seconds; together with the unchanged animation pipeline passed immediately before this
  work, 281 active tests pass and 21 diagnostics are disabled. Opened real-game views are in
  `docs/evidence/open-habitat-fronts-j31-2026-11-20.jpg` (J31, 2026-11-20).
- J32 makes the scripted habitat camera choose a public viewing front rather than the nearest
  service edge. `--stand habitat:<species>` rejects keeper gates, strongly prefers glass and moat
  over mesh or hedge, prefers an interior span over an end panel, and still lets actual distance,
  vegetation and visibility override that preference when the formally better panel hides the
  animal. Real offscreen Lion Rock and Bear Falls views both keep their selected resting animal
  visible; the opened pair is `docs/evidence/public-front-stand-j32-2026-11-20.jpg`. All fifteen
  species aliases across the fourteen habitats resolve in a zero-tick offscreen smoke. The 22
  game-side tests and the complete suite pass: 281 active tests including the 365.72-second
  animation pipeline, with 21 diagnostics disabled as intended (J32, 2026-11-20).
- J33 refines the shrubs seen at arm's length along the planted fronts. Near shrub and scrub
  models use leaf sprays about one fifth smaller and seven visible stems instead of four; their
  far forms retain the old geometry. A first +19.9% scene setting was rejected. The retained
  version raises the same Lion Rock scene only 2,258,980 -> 2,364,188 triangles (+4.66%) with
  draw calls unchanged at 472; one savanna shrub is 500 -> 552 triangles and stays 128 far.
  All 23 game-side tests pass, including the new direct near/far budget regression. Together with
  the core and 365.72-second animation passes immediately before this game-only change, the suite
  has 282 active passes and 21 disabled diagnostics. The opened crop is
  `docs/evidence/fine-shrubs-j33-2026-11-20.jpg` (J33, 2026-11-20).
- J34 puts every player collision shape on the ground that is actually drawn. Fences, buildings
  and props now use `DrawnGroundHeight`; deep-water blocks extend above the water surface rather
  than a fixed distance above their bed; and `Walker` watches `World::NavRevision`, so a later
  terrain or water edit rebuilds collision on the next update. The expanded regression failed
  before the fix with 227/240 steps in a legal 3 m pool and a closest approach of 6.7 cm through
  a visible rock whose required body clearance is 0.86 m. It passes after the correction, as do
  all 23 game-side tests / 13 Walker cases (J34, 2026-11-20).
- J35 gives dangerous habitats a real fail-closed keeper lockout. The whole group is atomically
  assigned to registered shelter capacity (a loose animal, bad door or short capacity changes
  nothing), remains hidden while needs and health continue, and is released only after the keeper
  has crossed back out through the same registered service gate. Routine outside care stays
  outside; a due lockout contains one twelve-second rake at the gate, and all seven dangerous
  habitats exercise it without any measured hold reaching 600 ticks. Active phase and task actions
  are saved in staff format 2; format 1 still loads, and a mid-lockout zoo resumes bit-identically.
  Earlier 0.85/0.90 service thresholds plus fresh water at both public feedings improve the staffed
  week to 7 animal-hours in need versus 9 under automatic care (worst hunger 0.712 vs 0.786).
  All 12 focused keeper tests pass, including the week and public-crowd run; the final complete
  suite passes all 285 active cases, including the 366.73-second animation test, with 21 diagnostics
  disabled (J35, 2026-11-20).
- J36 closes the visitors' unchecked last step from paving to a seat. Site refresh now advertises
  a bench or picnic table only when one reachable path access has a straight ground-profile line
  to every real seat, so the line cannot cross a fence, water or the property boundary. Arrival
  and departure routing both reuse that access rather than independently selecting a nearer tile
  across a barrier. Two direct regressions cover an unusable fenced bench and the safe access at
  both ends of a route; all visitor/day/walk and short-hash tests pass without changing the Visitor
  Zoo hashes. The complete suite passes all 287 active cases, including the 364.69-second animation
  test, with 21 diagnostics disabled (J36, 2026-11-20).
- J37 persists the one-game-hour loose-animal alarm cooldown. The optional top-level
  `last_scare_note` keeps format-3 saves backward compatible, rejects impossible future or
  pre-sentinel ticks, and is part of saved-state equality while remaining outside the
  simulation hash like the notifications it controls. A regression scares one crowd, saves and
  loads, then produces a real scare in a second crowd inside the cooldown and proves neither zoo
  repeats the alarm. All 30 focused visitor/save/hash cases pass; the complete suite passes all
  288 active cases, including the 365.89-second animation test, with 21 diagnostics disabled
  (J37, 2026-11-20).
- J38 corrects the stale claim that litter waits for midnight even though the staffed zoo already
  has a cleaner. A deterministic public-visit regression creates ground litter, watches the exact
  midpoint of the cleaner's visible sweep remove it, and proves the calendar has not crossed into
  the automatic night shift. The focused test passes in 4.10 seconds; the complete suite passes all
  289 active cases, including the 362.86-second animation test, with 21 diagnostics disabled
  (J38, 2026-11-20).
- J39 replaces the visitor leader's single rejected service with a bounded visit-long history.
  Queue patience, an unaffordable order and a full indoor service all add to the same list, and
  every later service choice avoids every remembered building. Saves keep the complete history
  while retaining `skip_building` for old readers; old saves promote that lone value. Focused
  visitor, save round-trip and corruption tests pass; the complete suite passes all 290 active
  cases, including the 367.51-second animation test, with 21 diagnostics disabled
  (J39, 2026-11-20).
- J40 keeps an active food or drink queue out of unrelated people who are already standing.
  `MakeRoom` now reserves the continuous line through all 16 occupiable places rather than eight
  isolated points, and a shuffling queue waits as a whole before entering an eating, watching,
  reading or idle visitor. A 6,000-tick every-frame regression covers the former gap while the
  two-day visitor simulation retains normal purchases and sub-three-second longest standstill
  (J40, 2026-11-20). The complete suite passes all 291 active cases, including the 363.81-second
  animation test, with 21 diagnostics disabled.
- J41 keeps a late-arriving watcher out of somebody already standing at a habitat front. Viewing
  slots now reject every unrelated current or reserved position within 0.5 m; if somebody settles
  there after the plan was made, the watching group yields and chooses another view. The new
  6,000-tick every-frame regression first caught a watcher 0.257 m from a visitor who had just
  finished eating and now keeps all inspected pairs at least 0.5 m apart. All 29 focused
  visitor/day/walk cases pass without disturbing purchases, path discipline or standstill bounds
  (J41, 2026-11-20).
- J42 makes the visible cleaner react to fresh litter instead of carrying out a stale sweep plan.
  The cleaner now goes next to the nearest bin whose 25 m catchment contains litter, retains that
  bin on the sweep task and refreshes the exact target before leaving and on arrival. This was a
  real gap exposed by J41's changed crowd history: the existing public regression saw a closest
  sweep of 39.69 m while litter lay 0.23 m from a bin. It now removes a piece before midnight
  again; all 13 keeper cases pass, including deterministic twins, mid-round save/load and the
  staffed week. The combined J41/J42 suite passes all 292 active cases, including the 366.68-second
  animation pipeline, with 21 diagnostics disabled. Changed short and 6,000-tick hashes repeat
  twice for seeds 1–3 (J42, 2026-11-20).
- J43 gives members of one walking group the same 0.55 m shoulder corridor as unrelated walkers;
  it replaces the old every-tenth-tick allowance with an every-tick regression. Same-group pairs
  below 0.3 m fall from 1,422 / 98,545 (1.44%) to 562 / 97,096 (0.58%). An attempted hard
  collision constraint was rejected because crossing routes could deadlock for hundreds of
  seconds. The retained tuning keeps the two-day visit healthy: everybody leaves, purchases are
  195/198, nobody strays off a legal access and longest walking standstills are 2.5/1.9 seconds;
  all 29 focused visitor/day/walk cases pass. Changed short and 6,000-tick hashes repeat twice for
  seeds 1–3; the complete suite passes all 292 active cases including the 367.09-second animation
  pipeline, with 21 diagnostics disabled (J43, 2026-11-20).
- J44 closes T1's empty-habitat reporting follow-up. `ScenarioReport` now contains every named
  enclosure's actual opening species/count inventory, retaining an empty entry when stocking was
  omitted or failed. The Visitor Zoo regression compares all 14 report entries with its promised
  15 species and explicitly sees an empty Penguin Cove in the omit-penguin variant; both seeds and
  all four starter-scenario cases pass without changing simulation state. Short and 6,000-tick
  hashes for seeds 1–3 remain unchanged; the complete suite passes all 292 active cases including
  its 365.28-second animation pipeline, with 21 diagnostics disabled (J44, 2026-11-20).
- J45 prompts the next deterministic animal sidestep after 15 rather than 20 ticks for ordinary
  activities, while food and water errands retain the proven one-second route. Seed 1's stalled
  moving ticks fall from 2,110 / 262,045 (0.805%) to 1,805 / 271,509 (0.665%); the regression now
  requires below 0.75% and no continuous stall of 120 ticks. Applying the shorter limit to every
  activity was rejected because it failed either morning or week-long staffed care. The scoped
  version passes all 24 focused movement, keeper, day and week-capacity cases; changed short and
  6,000-tick hashes for seeds 1–3 repeat twice. The complete suite passes all 292 active cases
  including its 368.55-second animation pipeline, with 21 diagnostics disabled (J45, 2026-11-20).
- J46 raises the keeper renderer's bounded full-avatar budget from 8 to 12, enough for all nine
  members of Greenhill's crew instead of dropping the farthest one when all are visible. The
  150 m distance/frustum cull and the bound for damaged or imported saves remain; the scenario
  regression now requires the planned seven keepers, gardener and cleaner. A warning-clean game
  build, real 626-visitor offscreen smoke and all 32 active keeper/Walker integration cases pass;
  its captured frame was opened and checked (J46, 2026-11-20).
- J47 brings the two already-authored bear-cub bodies into Greenhill. Bear Falls and Polar Sound
  now each hold a mother, a 0.6/0.7-year female cub and a male cub; the sire lives elsewhere, as
  in a real mother group. The population is 90 and both three-animal groups remain inside their
  catalog capacity and welfare bands. Both family seeds, opening/day/week capacity, identity and
  feeding checks pass; live habitat runs load the dedicated juvenile models, and both cub viewers
  were opened and checked in `docs/evidence/bear-families-j47-2026-11-20.jpg`
  (J47, 2026-11-20). The 600-step smoothed family means are 0.936/0.936 while the direct habitat
  scores remain 0.969/0.963; the changed seed-1 history has 1,603 / 303,845 stalled moving ticks
  (0.528%). Short and 6,000-step hashes repeat for seeds 1–3. The complete suite passes all 292
  active cases including the 365.05-second animation pipeline, with 21 diagnostics disabled.
- J48 keeps scripted public viewpoints clear of the visible tree, not just its collision trunk.
  A shared sight-blocking radius derives a tree's opaque crown from its existing shade footprint;
  non-tree props keep their former solid radius. At Bear Falls seed 1, the old sight line missed
  a spruce trunk by 1.15 m but crossed its 2.15 m crown, filling the `(164.8,57.0)` view with
  needles. It now chooses the clear water-edge `(154.8,45.0)` position. Lion, meerkat, polar-bear
  and giraffe control views retain useful public fronts; the opened comparison is
  `docs/evidence/clear-view-crowns-j48-2026-11-20.jpg`. Two focused radius regressions and all 25
  game-side tests pass. The complete suite passes all 294 active cases including the 365.53-second
  animation pipeline, with 21 diagnostics disabled as intended (J48, 2026-11-20).
- J49 makes animal route searches include blocking scenery in a six-metre start/target corridor.
  A generic per-route circle overlay is checked on A* edges and again while simplifying straight
  segments; visitors, staff and cached world components keep their old profiles, while a body
  loaded inside old scenery may still walk out. Two navigator regressions and a lion-around-an-oak
  integration pass; Greenhill's seed-1 stall probe improves from 1,603 / 303,845 (0.528%) to 819 / 287,191
  (0.285%) without relaxing its 0.75% or 120-tick bounds. The changed visitor history exposed and
  fixed three general late conflicts: arrivals no longer snap into a newly occupied destination,
  walkers have a sliding 0.36 m floor around the player, and the cleaner's overlapping 30 m bin
  catchments close its narrow unswept ring. The affected queue, standing, player, cleaner and
  short/full hash regressions pass; both hash sets repeat for seeds 1–3. The complete suite passes
  all 297 active cases including the 374.83-second animation pipeline, with 21 diagnostics disabled
  as intended (J49, 2026-11-20).
- J50 replaces the dangerous-habitat lockout's teleport with a visible shelter recall. Capacity
  and usable doors are still assigned atomically, then every resident walks there with ordinary
  gait and collision while the keeper waits at the outside control; `MayStep` remains fail-closed
  until the last dangerous animal is hidden. All seven dangerous habitats show the transfer and
  later admit the keeper; the longest whole lockout is 1,116 ticks (55.8 seconds) under a
  1,500-tick bound. Saves during both the walk and inside work resume bit-identically. The current
  staffed week records 10 animal-hours in need against 16 without visible staff, with longest
  spells of two against four hours. The inspected Polar Sound frame is
  `docs/evidence/animated-lockout-j50-2026-11-20.png`. The changed short and 6,000-step hashes
  repeat for seeds 1–3. Both manually enabled full-visibility and visiting-day capacity seeds pass;
  the complete suite passes all 298 active cases including the 466.17-second animation pipeline,
  with 21 diagnostics disabled (J50, 2026-11-20).
- J51 removes the square ground-wear seam at fence ends, turns and gate transitions. Ground paint
  now measures the distance to finite nearby fence segments rather than the infinite side of its
  own tile: free ends are round, turns join and the 1.5 m gate apron blends into the narrower panel
  wear. The tile paint signature covers the same twelve possible segments, so an editor fence
  change also invalidates every neighbouring texture it can affect. Three game regressions compare
  648 boundary samples around a free end, L turn, straight join and panel/gate transition; colour
  agrees within 1e-6 and detail kinds exactly. All 28 game-side C++ tests pass. A 1280 x 720
  offscreen Tiger Forest smoke completed with ground paint at 1.77 seconds, within the previous
  measured range. The complete suite passes all 301 active cases including the 562.16-second
  animation pipeline, with 21 diagnostics disabled (J51, 2026-11-20).
- J52 broadens the drawn bank at every deep pool from 0.9 to 1.55 m and reduces the wandering
  start from 0.9 to 0.35 m. The 1.9 m total still reaches the exact bed inside its first two-metre
  water tile, preserving the cross-tile seam proof and keeping dry land dry. Across all nine mesh
  columns of a constructed 1.4 m pool, the worst 25 cm step falls from 3.19 to below 1.6 m/m and
  the narrowest visible 20-to-50 cm band grows from 11 to over 20 cm. The new hard regression and
  all 29 game-side tests pass; all four ground tests pass under AddressSanitizer. Matched offscreen
  Tiger Forest frames were inspected without cracks, flooding or renderer errors. The complete
  C++ suite passes all 301 active cases in 281.86 seconds, with 21 diagnostics disabled; the
  unchanged animation pipeline passed in the immediately preceding full run (J52, 2026-11-20).
- J53 removes the second resident copy of every animal animation timeline. The importer-owned XNA
  tracks are released only after `AnimalRenderer` has deep-copied them into its engine-neutral
  skeleton clips; model buffers, materials, textures and the converted clips remain alive. On the
  current 90-animal zoo, which now legitimately loads all 33 variants because J47 added bear cubs,
  the immediate matched software-GL profile falls 2,848 -> 2,799 MB resident and 2,499 -> 2,457 MB
  heap; the animal load mark falls 2,601 -> 2,550 MB. All five alternate memory modes corroborate
  the direction. All 29 game-side tests pass, a 3,000-tick/60-frame Lion Rock run is visually
  intact, and the same warm-up plus three frames completes without ASan or UBSan findings (J53,
  2026-11-20). The complete C++ suite passes all 301 active cases in 311.90 seconds, with 21
  diagnostics disabled; the unchanged animation pipeline passed in the preceding full run.
- J54 removes the CPU staging vectors for animal mip levels after their complete GPU upload, but
  only in the desktop Linux visit where context recovery is already deliberately disabled. The
  viewer and Windows, Android and web paths retain readback/recovery. The immediate same-machine
  software-GL profile falls 2,799 -> 2,760 MB resident and 2,457 -> 2,405 MB heap; the animal mark
  falls 2,550 -> 2,486 MB. All five alternate modes move in the same direction, but only the
  direct 39 MB resident result is claimed. A regression proves the thin wrapper retains its GPU
  renderer and complete mip count without level-zero CPU pixels; all six texture tests pass under
  ASan and all 30 game-side tests pass. A matched Lion Rock pair differs only on 0.14% moving
  distant pixels, and a real ASan/UBSan visit loads all 33 variants and draws 65 of 90 animals
  cleanly. The complete C++ suite passes all 302 active cases in 366.28 seconds, with 21
  diagnostics disabled (J54, 2026-11-20).
- J55 releases the smaller CPU mip staging levels of the 100 terrain chunks, three ground-detail
  textures and the apron after a desktop Linux visit has uploaded them. A shared helper preserves
  the texture wrapper's address, renderer and complete mip count, so existing materials keep
  valid pointers; the editor and context-recovering platforms remain unchanged. The immediate
  software-GL profile falls 2,760 -> 2,665 MB resident and 2,405 -> 2,301 MB heap. All four
  alternate visit modes also move down, while the unchanged editor is noisy. The direct 95 MB
  observation exceeds the removed allocations, so only their 37.7 MiB logical size is attributed.
  All 30 game-side tests and six ASan texture tests pass. A real
  ASan/UBSan visit completes 6,000 steps cleanly, and the inspected Lion Rock image keeps the
  terrain intact. The animation pipeline passes all 131 cases, and the complete C++ suite passes
  all 302 active cases in 253.16 seconds with 21 diagnostics disabled (J55, 2026-11-20).
- J56 releases another exact 10.33 MiB of known CPU allocations after a read-only desktop visit
  has uploaded its common procedural textures: 4.81 MiB of smaller surface mips and 5.52 MiB for
  the far-plant palette source image, three miniatures and mip staging. GPU wrappers and materials
  retain their addresses, including the 48 water-normal frames selected by animation. The editor
  keeps all editable resources; context-recovering platforms keep texture staging. No RSS saving
  is inferred from the noisy matrix (the direct sample rose while all four alternates fell).
  All 30 game-side tests pass; a matched Lion Rock image changes only on 0.15% moving pixels, a
  real ASan/UBSan visit completes cleanly, and both an editor smoke and the two-reload end-to-end
  script pass. The complete suite passes all 303 active tests in 420.97 seconds, including the
  animation pipeline, with 21 diagnostics disabled (J56, 2026-11-20).
- J57 adds exact owned mesh-buffer accounting to `--bench` and records ZOO-UP-004. The normal
  Visitor Zoo owns 1,023 `GpuMesh` instances with 112.2 MiB of vertex and 20.7 MiB of index data.
  CNA retains matching CPU shadows even though the buffers are write-only and static; 132.9 MiB
  is therefore the measured lower bound, before imported animal buffers which the counter does
  not own. No memory reduction is claimed: CNA exposes neither a shadow-release method nor a
  cached-renderer reconstruction factory for buffers. The proposed upstream extension retains
  renderer identity and restricts release to callers which opt out of recovery/readback (J57,
  2026-11-20). The warning-clean build, real 45-frame benchmark and all 30 game-side tests pass.
- J58 gives every occupied dangerous-animal house a separately operated welded-mesh sliding
  gate. It closes behind the last assigned resident, remains shut through the keeper's inside
  rake, and slides open for 1.2 seconds before the lockout releases. The keeper now visibly
  performs the release task from safe public ground instead of skipping it on the gate crossing; an unreachable
  exact control still falls back to that safe-side operation. The seven-habitat regression now
  requires a visible outside release at every one. A lockstep Polar Sound sequence was opened at
  the open, closed, sliding and released frames: the gate fits the cooled-den portal and all three
  bears emerge only after it clears. The staffed week remains better than automatic care at 11
  against 16 animal-hours in need, with longest spells of one against four hours. Changed short
  and 6,000-step hashes repeat twice for seeds 1–3. The same history exposed a follower restarting
  its route to a moving leader once per second; comparing against the leader's destination fixes
  the 179-tick penguin stall and lowers the seed-1 probe from 0.575% to 0.329%, with a focused
  regression for the route. The complete suite passes all 304 active cases, including the
  479.96-second animation pipeline, with 21 diagnostics disabled. Under ASan the two gate/lockout
  tests, focused group-route test and 12,000-step stall regression pass in 532.88 seconds; all 30
  game-side tests pass in 240.69 seconds. A real Polar Sound visit loads the full 90-animal zoo,
  runs 3,000 fill steps and draws three frames cleanly (leak detection off for the documented
  `ptrace` limitation; J58, 2026-11-20).
- J59 makes water respond to animals instead of leaving waders and swimmers visually detached
  from its animated surface. Every visible animal moving through more than 4 cm of water leaves
  three soft concentric rings at the drawn surface; a floating body keeps a quiet disturbance at
  rest, and the footprint scales with its body and
  stretches along its heading and speed into a wake. It fades when the animal stops or leaves
  the water, so no dry selection-like mark remains. One 64 x 64 texture and one quad are uploaded
  once, then reused for the extra visible-wet-animal draw. Penguin Cove and Flamingo Lagoon were
  inspected close up; both show subtle rings without a rectangular texture edge
  (`docs/evidence/animal-water-j59-2026-11-20.jpg`). An early unlit material exposed its whole
  white quad and was rejected using a no-animal control before the final PBR alpha material.
  The image generator has a focused coverage/falloff test; all 31 game-side tests pass under
  ASan in 205.39 seconds, and a real ASan Flamingo Lagoon visit loads all 90 animals, runs 3,000
  fill steps and draws three frames cleanly. The complete suite passes all 305 active cases in
  392.12 seconds, including the 392.11-second animation pipeline, with 21 diagnostics disabled
  (leak detection off for the documented `ptrace` limitation; J59, 2026-11-20). The effect is
  surface displacement, not a claim of discrete particle splashes.
- J60 gives both brown-bear bodies the 1.5-second alternating-paw swim already authored for the
  shared polar-bear rig. The adult moves at 0.78 m/s and the cub at 0.29 m/s; neither clip has
  ground-contact events, so deep water no longer plays their land walk. A first preview copied
  the polar placement at 0.91 withers and visibly split the brown silhouette into head and rump;
  it was rejected. The brown-specific 0.72 placement leaves the shoulder hump and back as one
  continuous surface silhouette (`docs/evidence/brown-bear-swim-j60-2026-11-20.jpg`). Both GLBs
  retain their mesh, coat and 33-bone skeleton and grow by exactly 35,348 bytes. Independent
  one-thread exports of both GLBs and manifests are byte-identical. All nine bear tests and all
  131 animation-pipeline cases pass (10.58 and 353.28 seconds). Release viewers inspected adult
  and cub at two phases; ASan viewers load
  and play both 330-track clips, and a real ASan Bear Falls visit loads all 90 animals, runs
  3,000 fill steps and draws three frames cleanly (leak detection off for the documented
  `ptrace` limitation; J60, 2026-11-20). Polar assets and simulation history are unchanged.
- J61 gives the tiger its own 1.2-second, 0.90 m/s deep-water paddle instead of playing the land
  walk below the pool. Alternating forepaws each travel 36 cm, the hind paws trail clear of the
  bed, a small wave travels down the four-bone tail and the clip declares no contacts. A direct
  17-phase regression requires both front strokes to exceed 30 cm and every toe to remain at
  least 8 cm above its rest level. The 0.70-withers waterline was opened in a calibrated render:
  head and back remain one continuous surface silhouette, and all eight held phases are shown in
  `docs/evidence/tiger-swim-j61-2026-11-21.jpg`. Mesh, coat, two materials, 32,702 triangles and
  33-bone skeleton are unchanged; the GLB grows by 35,204 bytes and a separate one-thread GLB
  and manifest export is byte-identical. All 132 animation-pipeline tests pass in 351.67 seconds
  (the 49-case shared clip suite before the new focused case passed in 167.51 seconds). An ASan
  viewer plays more than a complete cycle over 80 lockstep frames, and a real ASan Tiger Forest
  visit loads all 90 animals, runs 3,000 fill steps and draws three frames cleanly. Release
  Tiger Forest checks at 3,000 and 6,000 ticks exercised ordinary land rendering; both tigers
  happened to be resting, so they are not claimed as swim evidence (leak detection off for the
  documented `ptrace` limitation; J61, 2026-11-21). Lion assets and simulation history are
  unchanged.
- J62 closes the remaining deep-water animation gap: adult and first-year flamingos now use a
  1.4-second surface swim instead of their land walk in the deep heart of Flamingo Lagoon. The
  level trunk and upright neck ride at a 0.72 m adult waterline while the long legs scull
  alternately beneath and behind it; the 0.45 m/s speed, depth and complete pose scale to the
  86-percent juvenile, and neither clip declares ground contacts. Both GLBs retain 12,354
  triangles and 29 bones, grow from 11 to 12 clips, and match independent one-thread GLB and JSON
  exports byte for byte. The calibrated adult/juvenile water views and six phases are in
  `docs/evidence/flamingo-swim-j62-2026-11-21.jpg`. A new catalog-level regression derives every
  `needsDeepWater` model and requires `swim` on its base and every available female/juvenile
  variant; this now passes for all such species. All 134 animation-pipeline tests pass in 322.16
  seconds. ASan viewers played more than one cycle for both flamingo bodies, and a real ASan
  Flamingo Lagoon visit loaded all 90 animals, ran 3,000 fill steps and drew three frames cleanly
  (leak detection off for the documented `ptrace` limitation; J62, 2026-11-21).
- J63 gives the male lion, lioness and cub a dedicated seven-second `nuzzle` for `socialize`
  instead of the ordinary idle pose. All four paws stay planted while the chest and neck lower,
  the muzzle moves through a broad side-to-side rub, the ears settle back and the tail keeps a
  quiet wave. This is deliberately honest individual contact motion: the simulation places a
  social lion near a group mate but does not time-align or fit two actors. The opposite contact
  directions for all three bodies are in `docs/evidence/lion-nuzzle-j63-2026-11-21.jpg`. Each GLB
  grows by about 104 kB, from eight to nine 32-bone clips, and all six production GLB/JSON files
  match independent one-thread exports byte for byte. All 135 animation-pipeline tests pass in
  334.57 seconds; all 305 active CTest cases also pass (the pipeline's second run took 379.09
  seconds). ASan viewers played 440 lockstep frames for each body, and a real ASan Lion Rock visit
  loaded all 90 animals, ran 3,000 fill steps and drew three frames cleanly (leak detection off for
  the documented `ptrace` limitation; J63, 2026-11-21).
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

Keepers (`docs/KEEPERS.md`; J58 closes the holding-gate/release-animation item), ground and water edges
(`docs/GROUND.md`; open: 25 cm detail outlines and engineered one-tile pool banks), plants (`docs/PLANTS.md`;
open: far cut-out edges aliased and fill cost of alpha-tested cards). The new first
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
- Every young animal used in the Visitor Zoo has a dedicated variant. J47 puts the brown and polar
  juvenile bodies into their three-animal mother-and-cub groups; six other young species still use
  dedicated variants with adult proportions because no separate source body exists.
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
  every model variant. Lion contact now has a dedicated individual nuzzle; all synchronised
  multi-animal contact remains unimplemented. The exact table and honest visual limitations are
  in `docs/ANIMALS.md`.
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

Area per animal is an estimate, not EAZA/AZA figures. J18 closed the dry-moat item, J29 puts a
containment-rated hedge around Meerkat Lookout's service sides, J30 widens or removes all three
remaining ordinary 2 m path remnants, and J31 replaces the last slender front-tree exceptions
with low cover inside the welfare band.

`--stand habitat:<species>` now rejects keeper gates and buildings in the sight line. It also
scores the public barrier material and its span: glass and moat strongly outrank service mesh or
hedge, while actual visibility can still override a scenery-obscured preferred panel. All fifteen
species aliases resolve; real Lion Rock and Bear Falls views are recorded in
`docs/evidence/public-front-stand-j32-2026-11-20.jpg` (J32). The earlier meerkat correction moved
the view from `(177.0, 263.2)`, looking through mesh into the shelter wall, to `(169.2, 269.0)`,
with an unobstructed animal in the centre; the before/after offscreen images were inspected and
the 18 game-side Walker/texture tests passed (2026-11-19).

The same command also rejects positions overlapping the exact solid radii used by `Walker`,
including its 0.28 m body radius and furniture which simulation visitors may pass through.
All fourteen habitats kept identical requested/final coordinates to 0.1 m in a zero-tick
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
