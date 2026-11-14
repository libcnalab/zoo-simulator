# Giraffe head and neck derivative

Source: **Giraffe (textured)**, a taxidermy head-and-neck photogrammetry scan.
Author: **aellis43** (https://sketchfab.com/aellis43).
URL: https://sketchfab.com/3d-models/giraffe-textured-673b69aaafc14d94940a862225794f01
Title, author, source URL and licence were verified in the GLB asset extras through
Blender's glTF importer; ordinary imported objects had no such custom properties.
Licence: **CC BY 4.0**, https://creativecommons.org/licenses/by/4.0/ .
The original download is not needed to build the three giraffe models.

The body is modelled by us. Our generator and animation code are MIT licensed;
the scanned head and neck and their derivative maps retain the attribution above.

## Preparation and changes

- Import only through Blender 4.3's glTF importer, from the read-only local file.
- Combine the 26 mesh chunks; weld coincident boundaries before decimation.
- Reduce the original 2,668,311 triangles to 13,000, giving the head priority.
- Correct the imported coordinate frame to metres, +X forward, +Y left, +Z up.
- Remove triangles at the cut neck end below source Z = -2 m; the portable
  derivative has 12,518 triangles (6,579 above source Z = -0.55 m).
- Preserve the photo UVs and photo colour; the supplied 1024-square image is
  resampled to 2048. This does not create new photographic detail.
- Bake a tangent-space normal map once from the full scan before proportion edits.
- For the adult, incline the neck 22 degrees, scale the scan to 88%, and position
  its highest point at 5.2 m. The retained neck already supplies adult neck length;
  the newly modelled shoulder fills the cropped base.
- Add our body, limbs, hooves, tail and mane continuation; assign our skeleton and
  normalized weights, and animate with our clips rather than any source animation.
- Build a slimmer 4.5 m female and a 2 m calf with relatively long legs and shorter
  neck; use one combined base-colour atlas and one combined normal atlas per model.

## S11b coat and anatomy revision

The source mount's subspecies remains unverified. Its photographed irregular,
sometimes jagged patches, broad boundaries, aged olive colour and green background
contamination are unsuitable for the game's reticulated giraffe. The S11b generator
retains the scan's shape and baked normal detail, but **does not use `photo.png` in
any exported material**. That file remains as a historical source reference.

The whole animal is repainted in animal-space coordinates with a reticulated
polygon network: chestnut patches (nominal sRGB 165/95/50, with bounded centre and
cell variation), cream lines (235/222/195), smaller facial markings, a pale underside
and markings fading below the carpi and hocks. The calf's patches are paler. Muzzle,
nostrils, eye surrounds, inner ears and bald bull ossicone tips are painted separately.
The species label now describes our repainted animal, not an identification of the
original mount.

The overlapping neck ends are cut back and their open boundary rings are stitched
into a continuous skinned transition. The combined mesh is unwrapped into one
2048-square atlas with equalised island scale. The baked scan normals are transferred from the old UV tangent frames into
the new ones, fading at the neck graft. Short-fur detail is sampled in metres across
both surfaces. Base colour and normal maps are exported losslessly as PNG, avoiding
colour contamination at cream/chestnut boundaries. Source files remain unchanged.

Additional modelling changes in S11b:

- Replace the black box feet with tapered cloven hoof capsules, level soles,
  approximately 0.15 m tall and 0.25 m long in the bull; scale them for the variants.
- Shape the fetlocks and short pasterns, and broaden the muscular lower neck.
- Extend a short brown mane along the dorsal neck to the poll; refine the long
  black tail tassel with tapered locks.
- Enlarge the calf's eyes and ears and incline its tufted ossicones backwards;
  retain its 2.0 m height and shorter-neck proportions.
- Preserve the 36-bone skeleton and eight clip names. Rest remains chest-down with
  folded legs and upright neck; only sleep curls the neck back.

Four duplicate scan faces were removed in S11; generated meshes are validated
before export. The original scan remains a decimated taxidermy surface, with some
small facial irregularities still visible in close views. The body is modelled by
us and should not be described as a photorealistic scanned whole animal.
