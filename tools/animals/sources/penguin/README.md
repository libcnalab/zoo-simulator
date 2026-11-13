# Penguin derivative

Source: **Juvenile Magellanic Penguin**, RISD Nature Lab, **CC BY 4.0**.
https://sketchfab.com/3d-models/juvenile-magellanic-penguin-bf69c563457a4bb184fb8af2bf2d2811
Licence: https://creativecommons.org/licenses/by/4.0/

The specimen is *Spheniscus magellanicus*, not *Spheniscus demersus*.
**The adult African penguin plumage is painted by us on a Magellanic specimen's shape.**
Do not describe this asset as a scan of an African penguin. The juvenile retains
this Magellanic juvenile's grey-brown photo plumage as a same-genus shape proxy.
The little blue penguin scan is not incorporated.

Changes made:

- Imported the GLB with Blender 4.3.2's glTF importer, joined its 21 mesh chunks,
  baked node transforms, welded boundaries at 0.05% of specimen height, removed
  loose vertices, triangulated, and decimated from 449,799 source triangles to
  12,489 triangles in the portable derivative. The retained scan includes the bill, flippers and webbed feet.
- Converted to +X forward, +Y left, +Z up in metres; standing head height is 0.65 m.
  Aligned the trunk by 18 degrees and counter-rotated the mounted head a further
  18 degrees with a smooth neck blend;
  raised the lower bill to close the mounted specimen's gape; slightly thickened
  and lowered the dried feet to the ground.
- Added our landmark-fitted bird skeleton, a jaw, three joints per flipper, and
  deterministic anatomical-region skin weights (at most four, normalized).
  The source is static; none of the animations comes from RISD.
- Kept the original UV layout and 1024 photo texture. `detail.png` is a high-pass
  luminance derivative of that photograph. `occlusion.png` is neutral white,
  **not** a baked occlusion map. No scan normal map is supplied or claimed.
- Added small living dark eyes with painted highlights. Repainted the adult with
  one black breast band, white facial stripe/front, pink supraorbital patch,
  pale bill band, pink foot blotches and one deterministic breast-spot layout.
- Generated a smaller juvenile (0.82 relative height), broadened its trunk 7.5%
  locally, retained the photo plumage, and applied a tighter triangle budget.
- Added our idle, waddle walk, fast waddle, eat, drink, belly rest, sleep, bray,
  underwater flipper-flight swim and surface float animations.

Foot skinning retains whole-foot side ownership where webbing crosses the midline.
Both leading and trailing flipper edges follow the paddle chain; the outer paddle
is checked for stretch when raised and during underwater flight.

`mesh.npz` contains only numeric and Unicode arrays, read with `allow_pickle=False`.
Normal asset builds read only this directory, never the original downloaded GLB.
Preparation is reproducible using `tools/animals/prepare_bird.py --source ... --kind penguin --out ...`.

Water integration: read `meta.swim` and `meta.float`, each with `clip`, `depth_m`
and `speed`. Place the model origin at `water_surface_z - depth_m`. Values scale
with the model; the adult swim matches 1.5 m/s and two flipper beats per second,
and float matches 0.12 m/s. Water clips deliberately have no stance-foot events.

Individual spot variation is not implemented by the current renderer. The adult
has layout `adult_01`; the juvenile is unspotted. Per-individual adult spots need
a stable individual seed selecting alternate albedo atlases/materials (or a
renderer-supported spot-mask overlay). Do not randomly regenerate a shared atlas
at runtime or call the juvenile pattern an alternate adult spot layout.

Known visual limitations: the scan retains coarse belly/foot relief and some mounted-skin
asymmetry. The gape was reduced, not fully reconstructed. Painted bands are cleaner
than the scanned feather boundary. Juvenile fluff is a broader silhouette, not fur geometry.
