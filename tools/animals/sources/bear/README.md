# Strong Bear — prepared derivative

Source: **Strong Bear**, Pedro B. Goulart (**Pebegou**).
[Original work](https://sketchfab.com/3d-models/1d1bb459a39a428282ce4cb46bdc3ed5).
Licence: **Creative Commons Attribution 4.0 International (CC BY 4.0)**.
[Licence text](https://creativecommons.org/licenses/by/4.0/).
Original author retains copyright; modifications by Zoo Simulator contributors.
Generator/preparation code is MIT; the derived geometry and source image content are CC BY 4.0.

Blender 4.3.2 importer inspection: 38,040 triangles in the bear, plus six
734-triangle butterfly meshes (42,444 triangles total). Three 1024×1024
textures: base colour, tangent normal, and packed material channels. No
armature, skin weights or animations. The bear stands upright with forelimbs
spread and mouth open. Visually it resembles a brown bear, but has exaggerated
shoulder/arm musculature; an exact species/subspecies is not supplied by the mesh.
Source GLB size: 5,392,664 bytes. Source licence/authorship were supplied by the lead.

Preparation runs from the repository, imports the untrusted GLB **only through
Blender's glTF importer**, discards the butterflies and scene placement, converts
to +X forward/+Y left/+Z up, and grounds the geometry. It welds coincident vertices,
decimates the bear to 27,000 triangles, retains its UV layout and textures, fits
33 canonical bones in the upright pose, computes bone heat with anatomical region
constraints, smooths/truncates weights to at most four normalized influences,
and bakes a four-legged neutral pose. The shoulder pivot is placed inside the
source muscle volume; the short tail is folded down toward the rump. Teeth are assigned as rigid islands to the
upper or lower jaw; the lower jaw is closed by 39 degrees and exposed tooth length is reduced.
Skin folded by the large shoulder/pelvis repose is smoothed. Broad baked colour
shading is removed from the detail map while high-frequency fur detail survives.
The packed source material image is not used in the generated animal material;
a neutral occlusion map avoids multiplying baked muscle shadows into the new coat.

The animal builds use only the prepared numeric archive and maps in this directory.
No pickle, downloaded scripts, or original GLB is needed at build time.
Species modules define the proportion edits. Brown adults get a shoulder hump,
heavier waist/belly, shallow facial dish and brown coat with paler tips. Polar
variants are **re-proportioned and repainted derivatives of this same brown-bear-like
sculpt**, not a separate polar-bear scan: longer neck/body, narrower head, lowered
withers and raised rump, broader paws and warm cream pigment. Cubs have larger
heads/ears/paws and shorter limbs; the brown cub has a pale collar.
External lower-lip skin is included in jaw closure; a small added jaw-weighted
lower-lip volume seals the residual gap of the open-mouth sculpt.
Added opaque tapered fur clumps break the ruff/belly/leg outline. The source normal
map is retained at reduced strength. The eight land clips are newly authored;
all four variants have a paddling swim clip. Rest is sternal recumbency;
sleep rolls the folded body onto its side and settles the skin above the ground. Water placement uses
`water surface - meta.swim.depth_m`; the pronounced brown shoulder hump rides at 0.72 of withers
depth while the polar body retains 0.91. Speed is `meta.swim.speed_mps`.

Reproduction (one Blender process at a time):

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
export OPENBLAS_NUM_THREADS=1 PYTHONDONTWRITEBYTECODE=1 ASAN_OPTIONS=detect_leaks=0
blender -b -t 3 --factory-startup --python tools/animals/prepare_bear.py -- \
  --source /path/to/1d1bb459a39a428282ce4cb46bdc3ed5.glb \
  --out tools/animals/sources/bear
blender -b -t 3 --factory-startup --python tools/animals/build_animal.py -- \
  --species brown_bear --out assets/animals
```

Use the same build command for brown_bear_juvenile, polar_bear and
polar_bear_juvenile. Preparation is needed only to recreate the bundled derivative.
