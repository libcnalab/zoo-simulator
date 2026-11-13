# Prepared big-cat sculpt

Source: **Free strong lioness - 4k** by **Pedro B. Goulart (Pebegou)**.

Original model: https://sketchfab.com/3d-models/free-strong-lioness-4k-84d56e6cfcf247cb8fe468312f371677

License: **Creative Commons Attribution 4.0 International (CC BY 4.0)**,
https://creativecommons.org/licenses/by/4.0/ . Attribution and license information
were supplied with the task; the source page currently rejects automated access
with HTTP 403. No endorsement by the original artist is implied.

`sculpt.npz`, `detail.png`, and `occlusion.png` are derivatives of that work and
are distributed under CC BY 4.0. Generated lion, lioness, cub, and tiger meshes
and coat atlases retain this attribution. Generator code is separately MIT.
The asset manifests identify CC-BY-4.0 rather than mislabelling the sculpt as MIT.

## Changes to the original work

- Imported through Blender's glTF importer, with no execution or independent
  parsing of files in the untrusted source directory.
- Reset walking-pose joint deltas to bind rotations, retained the first spine
  placement while evaluating skinning, then removed the common placement.
- Converted to metres, +X forward / +Y left / +Z up, centred laterally, and placed
  the lowest foot vertex on Z=0. Removed junk and the transparent eye-glass shell.
- Welded coincident seam vertices while retaining per-corner UVs. Decimated the
  body to approximately 22,500 triangles and the mouth/claws mesh to 1,800;
  retained the 1,360-triangle eye geometry. The cub is further decimated at build;
  duplicate and zero-area collapse faces are removed before painting and export.
- Closed the sculpted open jaw by 29 degrees using its authored skin weights.
  Shortened exposed enamel vertically to fit the closed lips; translated extended
  claws back into their toe sheaths. These edits also apply to the animated mouth.
- Replaced source bone names with our rig names, merged neck and tail influences,
  summed weights, retained the four strongest influences and normalized them.
  Used anatomical endpoints for terminal bones: imported `*_end` joint positions
  collapse near an unrelated origin and do not represent the animal's anatomy.
- Enforced bilateral joint positions to eliminate source floating-point noise.
- Derived a grey high-frequency detail map from source base-colour luminance;
  retained source occlusion. Embedded source maps are 1024×1024, despite the
  original title. No original base-colour or normal-map texture is redistributed.
- Added hierarchical per-bone proportion edits, baked into new mesh and joint
  bind poses, for a heavier male, a short-legged cub with enlarged head/ears/paws,
  and a longer, broad-pawed tiger. The cub also has a smooth, localized orbital
  expansion that enlarges both eyes and their surrounding eyelids. Exact edits are in the species modules and
  copied into generated manifests. Chest-depth edits are compensated at the
  scapulae so they do not lengthen the forelegs or leave the hind paws floating. Model shoulders are normalized to 1.15,
  1.05, 0.45 and 1.00 metres respectively.
- Repainted all coats procedurally at 2048×2048 in the authored UV layout, with
  weak detail/occlusion modulation, new eyes, noses, mouths, lion colours,
  juvenile spots and tiger stripes.
- Added separate lofted mane locks, tail tufts and tiger cheek ruff geometry.
- Added our eight procedural animations: idle, walk, trot, eat, drink, rest,
  sleep and vocalize. The original file contains no animation clips.

## Bone mapping

The actual joint positions confirm Coluna1 is the pelvis and Clavicula branches
from Coluna4 at the scapula. No anatomical reversal was necessary.

| Source | Generator |
| --- | --- |
| `_rootJoint` | `root` (non-deforming, at the origin) |
| `Coluna1..4` | `hips`, `spine1`, `spine2`, `chest` |
| `Pescoco1`, `Pescoco2` | `neck1` (summed weights) |
| `Pescoco3` | `neck2` |
| `Cabeca`, `Maxilar`, `Orelha.L/R` | `head`, `jaw`, `ear.L/R` |
| `Clavicula`, `Braco`, `Antebraco`, `Mao1`, `Mao2` `.L/R` | `scapula`, `upper_arm`, `forearm`, `hand`, `fingers` `.L/R` |
| `Femur`, `Canela`, `Metatarso`, `Pe` `.L/R` | `thigh`, `shin`, `foot`, `toes` `.L/R` |
| `Rabo1..3`, `Rabo4..6`, `Rabo7..9`, `Rabo10` | `tail1`, `tail2`, `tail3`, `tail4` |
| `*_end` | parent deforming bone; no extra bone |

`mapping_source` and `mapping_target` in the NPZ record every imported joint.
There are 33 output bones including the non-deforming root. No limb joint has
been straightened or moved independently of its mesh.

## Reproduction

Normal builds need only this directory, not the original download:

```sh
blender -b --factory-startup --python tools/animals/build_animal.py -- \
  --species lion_female --out assets/animals --preview build-probe/animal-previews
```

Use `lion`, `lion_juvenile`, or `tiger` for the other assets. To regenerate the
prepared derivative from an explicitly supplied original, run from the repository:

```sh
blender -b --factory-startup --python tools/animals/prepare_bigcat.py -- \
  --source /path/to/84d56e6cfcf247cb8fe468312f371677.glb \
  --out tools/animals/sources/bigcat
```

The NPZ contains numeric and Unicode arrays only and is loaded with
`allow_pickle=False`. UVs are per triangle corner; tags and normalized weights
are per vertex. The generator reserves a separate atlas strip for additional hair.


The S5 preview path additionally writes `<species>_eye.png` (1600×1000,
4 m horizontal viewing distance, camera height 1.6 m) and `<species>_side.png`.
`--preview-clips-only --clips sleep` rechecks a sculpt pose revision without
rerendering unchanged standing views. All legacy preview defaults are retained.

The tiger's measured nose-to-tail straight bounding-box length is approximately
2.71 m; the curved tail is not measured as a straight extended tail. The source
sculpt retains some adult muscular definition in the cub. Hair is represented by
opaque geometry and painted strands, not a physically simulated fur groom.
