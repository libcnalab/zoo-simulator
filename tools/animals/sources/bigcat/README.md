# Prepared big-cat sculpt, version 2

Source: **Free strong lioness - 4k** by **Pedro B. Goulart (Pebegou)**.

Original model: https://sketchfab.com/3d-models/free-strong-lioness-4k-84d56e6cfcf247cb8fe468312f371677

License: **Creative Commons Attribution 4.0 International (CC BY 4.0)**,
https://creativecommons.org/licenses/by/4.0/ . Attribution and license information
were supplied with the task. No endorsement by the original artist is implied.

`sculpt.npz`, `base.png`, `normal.png`, `orm.png`, `occlusion.png`, and the legacy
`detail.png` are derivatives of that work, redistributed under CC BY 4.0.
**The source base colour and tangent-space normal map are now redistributed as
adapted material under CC BY 4.0.** Generated lion, lioness, cub, and tiger meshes
and adapted coat atlases retain this attribution. Generator code is separately MIT.

## Source measurements and material

Blender 4.3.2's glTF importer reports 48 source joints. The authored material has
three distinct embedded images, each **1024 × 1024**, despite the “4k” title:

- `Image_0`: base colour, including the amber iris, pupil, eyelids, whisker pads,
  nose leather, mouth, paw pads, and fur colour variation.
- `Image_1`: packed occlusion (red), roughness (green), metallic (blue); separate
  imported image nodes reference the same image for the material inputs.
- `Image_2`: tangent-space normal map.

No map is upscaled. `orm.png` preserves the packed source map for provenance;
exports use its occlusion and uniform fur roughness 0.84, with metallic zero.
The eye retains the authored amber iris, round pupil and dark rim. It shares the
body's roughness; no separate glossy eye primitive is exported.

## Changes to the original work

- Imported through Blender's glTF importer. Nothing in the download directory
  was executed or parsed independently of the importer.
- Reset walking-pose joint deltas to bind rotations, retaining the first spine
  placement during skin evaluation, then removed the common rigid placement.
- Converted to metres, +X forward / +Y left / +Z up, centred laterally, with the
  lowest foot vertex on Z=0. Removed junk and the transparent eye-glass shell.
- Welded coincident seam vertices while retaining per-corner UVs. Protected the
  source head, jaw and ears during weighted collapse; body and legs absorb the
  reduction. The prepared derivative has 28,942 body triangles, 1,360 eye
  triangles and 1,800 mouth/claw triangles: **32,102 total**. The cub receives a
  second uniform reduction during its build; the prepared allocation already
  strongly favours its head, and repeating that protection starves its torso.
- Closed the jaw by 31.5 degrees through the authored jaw weights. Retained S5's
  shortening of exposed enamel and claw retraction. Retracted soft oral tissue
  2.5 cm into the mouth to avoid protrusion through the closed lips. The same
  weights still move the mouth during vocalization.
- Renamed joints, summed merged neck/tail influences, kept the four strongest
  weights and normalized them. Enforced bilateral joint positions and supplied
  anatomical endpoints for imported terminal joints whose endpoints collapse
  near an unrelated origin. No independent leg-joint straightening was applied.
- Retained the authored UV layout, base colour, tangent-space normal and
  occlusion. The old grey detail map is retained only for compatibility and is
  no longer used to paint the cats. A verified unused upper-right atlas patch
  supplies the opaque hair backing, with a neutral normal there.
- Applied hierarchical per-bone proportion edits from the four species modules:
  broader male skull/jaw, thicker neck and chest; longer tiger trunk, lower head,
  larger rounder skull, shorter rounder ears, heavier forelegs and wider forepaws; larger cub head,
  eyes, ears and paws with shorter lower legs and tail. Shoulder normalization
  remains 1.15 m, 1.05 m, 0.45 m and 1.00 m respectively.
- Relaxed the cub's shoulder and thigh surface locally, shortened the muzzle
  independently of skull size, and relaxed the lower legs with volume-preserving
  smoothing. Softened its coat's high frequencies and attenuated the normal map
  over the limbs and trunk, with gentler facial relief.
- Colour-corrected the lioness's authored coat; darkened the male's; lightened
  the cub's with faint broken rosettes. Remapped the tiger's colour while
  retaining source detail, with spatially located irregular, tapered, forked and
  broken asymmetric flank strokes, leg half-rings, tail rings and symmetric
  facial markings. Preserved the source eyes and mouth detail.
- Replaced lofted outer hair locks with skinned hair cards. The male mane has an
  opaque inner volume in the body's primitive and a separate coarse-strand
  1024 × 512 RGBA material (`MASK`, cutoff 0.5, double-sided). Cheek/forehead
  hair follows the head, neck layers blend head/neck/chest influences, and the
  chest stays with the chest. Card normals point out from the mane volume.
  Tiger cheek ruff and lion tail tufts use the same card path.
- Kept the eight procedural clips: idle, walk, trot, eat, drink, rest, sleep,
  vocalize. Lowered the sleep head target towards the forepaws. Added complete
  surface checks including the hair and opaque backing in rest and sleep.

## Bone mapping

The source joint positions confirm Coluna1 is the pelvis and Clavicula branches
from Coluna4 at the scapula. No anatomical reversal was necessary.

| Source | Generator |
| --- | --- |
| `_rootJoint` | `root` (non-deforming, at origin) |
| `Coluna1..4` | `hips`, `spine1`, `spine2`, `chest` |
| `Pescoco1`, `Pescoco2` | `neck1` (summed weights) |
| `Pescoco3` | `neck2` |
| `Cabeca`, `Maxilar`, `Orelha.L/R` | `head`, `jaw`, `ear.L/R` |
| `Clavicula`, `Braco`, `Antebraco`, `Mao1`, `Mao2` `.L/R` | `scapula`, `upper_arm`, `forearm`, `hand`, `fingers` `.L/R` |
| `Femur`, `Canela`, `Metatarso`, `Pe` `.L/R` | `thigh`, `shin`, `foot`, `toes` `.L/R` |
| `Rabo1..3`, `Rabo4..6`, `Rabo7..9`, `Rabo10` | `tail1`, `tail2`, `tail3`, `tail4` |
| `*_end` | parent deforming bone; no extra bone |

`mapping_source` and `mapping_target` in the NPZ record every imported joint.
There are **33 output bones**, including the non-deforming root. The NPZ contains
numeric and Unicode arrays only and is loaded with `allow_pickle=False`.

## Reproduction and visual review

Normal builds need only this directory, never the original download:

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
blender -b -t 4 --factory-startup --python tools/animals/build_animal.py -- \
  --species lion_female --out assets/animals --preview build-probe/animal-previews
```

Use `lion`, `lion_juvenile`, or `tiger` for the other assets. Regenerate preparation
from an explicitly supplied original, from the repository directory:

```sh
blender -b -t 4 --factory-startup --python tools/animals/prepare_bigcat.py -- \
  --source /path/to/84d56e6cfcf247cb8fe468312f371677.glb \
  --out tools/animals/sources/bigcat
```

Preparation writes `bigcat_head_compare.png`: source on the left, prepared on the
right; rows are front, three-quarter and side, with matched cameras and lighting.
It also writes `bigcat_prepared.png`. Each build's preview includes standing views,
`<species>_eye.png` (1600 × 1000, 4 m distance and 1.6 m camera height),
`<species>_side.png`, `<species>_face.png` (1200 × 1000, 1.5 m head distance),
`<species>_normal_check.png` (opposing sun directions), and requested clip sheets.

The Barye and Pendl lion scans are shape references only. No geometry or texture
from either reference is included in the prepared data or generated models.
