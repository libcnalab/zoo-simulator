# Zebra Anatomy — prepared intact-flank derivative

Title: **Zebra Anatomy**. Author: **Shikaki_Naito**.

Source: https://sketchfab.com/3d-models/zebra-anatomy-c60aa3efb391448faa955426411d485a

Licence: [Creative Commons Attribution 4.0 International](https://creativecommons.org/licenses/by/4.0/).
The mesh and its adapted texture remain CC BY 4.0; the preparation/generator code is MIT.
No endorsement by the original author is implied.

The original is an unrigged anatomical study, with one exposed muscle/bone flank.
Blender 4.3.2 verified one mesh, 90,780 triangles, no armature, and two 1024-square
images. One image is base colour; the other supplies metallic/roughness channels.
There is **no authored normal map**. The derivative exports base colour, matte
roughness 0.9, metallic zero, and tangent attributes; it does not invent a normal map.

Changes made:

- Imported exclusively through Blender's glTF importer; removed source transforms
  and converted to animal axes (+X forward, +Y left, +Z up), metres and ground level.
- Bisected the sagittal region, discarded the anatomical flank, mirrored the intact
  flank, reversed winding and welded the centre seam. The cut is 15 mm inside the
  initially estimated centre to exclude residual exposed anatomy at the boundary.
- Preserved the intact source UV layout in two separate half-width atlas cells.
  The source colour is duplicated into a 2048 by 1024 PNG, with no downloaded
  scripts, external references, or dependency on the original GLB at build time.
- Flattened the duplicated source crest and projecting forelock, and added one
  closed, short central mane ridge. Its UVs project the adjacent neck stripes.
  Surface-edit bounds and ridge dimensions are recorded in `species/zebra.py`.
- Decimated toward 28,500 triangles (28,494 after removing degenerate faces) with extra protection on the head and hooves.
- Fitted a new 33-bone canonical quadruped skeleton from the explicit landmarks in
  `species/zebra.py`. Started with Blender's automatic bone-heat weights, then
  constrained anatomical regions to prevent opposite-leg and belly leakage.
  Smoothed twice and retained at most four normalised influences per vertex.
  The inner-leg threshold varies with height, and limb weights exclude the belly
  between the limbs and the throat ahead of the shoulder.
  Head, jaw, ears, tail and hooves have separate skin groups and part tags.
- Raised the neck by 20 degrees about a measured lower neck pivot, and rotated
  the ears upward (opposed 30-degree rolls and -25-degree pitches), baking the
  weighted surface deformation and descendant joint transforms.
- Split the lip edges along a species-defined plane. The upper/lower edges have
  separate head/jaw weights so vocalization opens a real slit. This adds no
  detailed teeth, tongue, or anatomically modelled oral interior.
- Saved pre-pose vertices, raw bone-heat weights and lip constraints alongside
  the prepared surface, allowing `prepare_scan.py --reweight --out ...` to tune
  the rig from numeric derivative data alone (`--no-preview` skips redundant renders). A manifold proxy is available if
  direct heat fails; the final derivative used the successful direct solution.
- Adult build: scaled to 1.35 m at the measured withers. Mare: 1.296 m with a slimmer
  neck. Foal: 0.90 m with shorter body, neck and muzzle, relatively longer legs and
  larger ears; deterministic coat relief of up to 2 mm and second decimation
  to 19,500 triangles. This is subtle surface fuzz, not individually modelled hair.
- Applied a small, smooth UV-space stripe displacement per flank and variant,
  fading to zero at the centre seam and at UV-chart borders. The original irregular stripe design is kept.
  The foal remaps pigment to brown and cream. Red muzzle/tissue-coloured residue
  is neutralised to charcoal; adult stripe design is otherwise retained.
- Replaced the metallic source appearance with a dry, matte coat; packaged colour
  as baseline JPEG quality 90 through the S6 material path.
- Added our eight named clips: idle, walk, trot, eat, drink, rest, sleep, vocalize.
  Walk uses lateral-sequence contacts; trot uses diagonal pairs and suspension.
  Contact-constrained IK drives the legs. Feeding uses an actual lip contact point; sleep uses the lowest transformed
  lip surface. Lying height is fitted against the deformed skin as well as
  the measured belly, rather than relying on internal bone clearance alone. Rest/sleep fold the front cannon bones
  backwards under the forearms; idle relieves one hind leg.

The source has broad, irregular flank stripes and narrower leg stripes extending
onto the lower legs, but a largely pale belly. The pattern has not been repainted
into an asserted subspecies pattern; this study alone does not establish a precise
plains-zebra subspecies. Mirrored anatomy and the source's stylised crest/head are
limitations, not evidence of photographic realism.

Preparation (run from the repository, not the untrusted download directory):

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv ASAN_OPTIONS=detect_leaks=0 OPENBLAS_NUM_THREADS=1
blender -b -t 3 --factory-startup --python tools/animals/prepare_scan.py -- \
  --species zebra --source /path/to/c60aa3efb391448faa955426411d485a.glb \
  --out tools/animals/sources/zebra
```

Normal builds require only this directory's `scan.npz` and `base.png`:

```sh
blender -b -t 3 --factory-startup --python tools/animals/build_animal.py -- \
  --species zebra --out assets/animals --preview build-probe/animal-previews
```

Repeat with `zebra_female` and `zebra_juvenile`. Landmarks are in animal-space
metres before the neutral neck rotation and final withers rescaling; the archive
contains the baked joint coordinates. The bone overlay is a lateral projection
with orange lines moved in front of the surface solely to make occluded joints
visible. It is not a claim that the bones lie on the skin.
