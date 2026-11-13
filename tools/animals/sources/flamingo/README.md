# Flamingo derivative

Source: flamingo sculpt by **muppe5**, **CC BY 4.0**.
https://sketchfab.com/3d-models/b363f8f4f7394ddb9d5b3a337f2f7fc7
Licence: https://creativecommons.org/licenses/by/4.0/

“Flamingo” is a descriptive label: Blender's imported object/material metadata
contains no title. The two meshes are named Object_4 and Object_6, with materials
Material.002 and Material.001. Contrary to the research brief, the file imports
**upright with an S-neck and one lifted foot**, not in a head-down feeding pose.
It has two 1024 colour images, including painted feather and eye detail.

Changes made:

- Imported only through Blender 4.3.2's glTF importer; joined meshes, baked node
  transforms, welded coincident boundaries, triangulated and decimated the
  51,606-triangle sculpt to 12,354 triangles before rigging.
- Converted to +X forward, +Y left, +Z up in metres, with head top at 1.4 m.
  Retained the sculpt's body, S-neck centreline, head, bent bill, eye geometry,
  draped scapular silhouette and webbed foot shape.
- Replaced the lifted leg with a reflection of the planted leg and gave both
  shanks a mild backward hock curve. Removed leftover raised-foot webbing.
- Added our landmark bird rig, eleven neck segments, head/jaw, wings, tail and
  two short femoral pivots and two four-bone leg chains. Assigned anatomical-region
  weights with normalized influences. The static source supplies no authored rig or animation.
- Packed both source UV/material tiles into one colour atlas. Extracted photo/
  paint luminance detail, repainted plumage pale pink-white with deeper pink
  coverts and dark flight-feather margins, pink legs, pale/black bent bill and
  yellow eyes with dark pupils. The retained sculpt supplies feather layering.
- Added our wading walk/fast gait, idle preening, inverted sweeping feeding with
  alternating foot stamps, drink, one-legged rest/sleep and neck-up flagging display.
- `occlusion.png` is neutral white, not a baked map. No normal map is supplied.

Foot skinning retains whole-foot side ownership where webbing crosses the midline.

The portable NPZ uses numeric/Unicode arrays only, loaded without pickle. Builds
need only these derivatives; they do not read the downloaded GLB. Regenerate with
`tools/animals/prepare_bird.py --source ... --kind flamingo --out ...`.
The optional juvenile flamingo is not included.

Known visual limitations: the sculpt has coarse feather ridges, and the neck base
shows some angular deformation during deep bends. The folded resting leg and tucked
head are less convincing in close side views than the neutral visitor-distance pose.
