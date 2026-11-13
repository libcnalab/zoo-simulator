# Elephant CYCLE

By exegetpro.

https://sketchfab.com/3d-models/elephant-cycle-50e17ba5c2dc4e3d9ed0effc7d2f9fd5

License: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

Changes:

- Removed camera, lights, helper shapes and unused animation controls.
- Converted the neutral bind mesh and joints to metres and animal axes; grounded feet.
- Renamed and merged joints, summed weights, retained at most four normalized influences.
- Resampled the authored walk to 81 samples; repeated the first endpoint for exact looping.
- Extracted source images through Blender; no executable download content is used.
- Tagged eyes, horns/tusks, ears, trunk and mouth where separable; nail tags use foot position.
- Measured backward stance velocities in the hip-relative frame, using the lowest 35% of foot heights.
- Repacked five material UV layouts into one 2048 atlas with eight-pixel gutters.
- Subdivided five trunk segments into ten and reduced each ear to two joints.
- The source has NO occlusion map and NO eye normal map. AO is baked from the neutral source geometry with Cycles (1024 pixels, 16 samples, two threads). The eye normal tile is flat. Neither is claimed as authored source detail.
- The prepared source is Asian; African conversion belongs to the species build.

Prepared: 15820 triangles, 46 bones. Original local-rotation endpoint maximum difference: 0.079077.

Authored stance speeds at prepared size (minimum / median / maximum, m/s):

- FL: 0.1402 / 0.3019 / 0.3313
- FR: 0.1146 / 0.3131 / 0.3449
- HL: 0.1798 / 0.5727 / 0.5863
- HR: 0.0005 / 0.5038 / 0.5256

The game clips use contact-constrained procedural replacement motion: these source velocities vary beyond the existing 2% foot-sliding tolerance. Original resampled rotations and positions remain in the NPZ for future animation work.

Species builds mirror the intact left ear over the perforated right ear, reshape/enlarge the ears, depress the back, round the crown, raise the head, adjust body proportions, remove pink coloration and reduce pigment contrast on the face/ears. New curved tusks sample the source ivory tile and follow the head rigidly; two new trunk-tip fingers sample the source skin. Cow/calf proportions differ; the calf has no tusks and reduced normal strength.

Builds export baseline JPEG colour (quality 90), JPEG normal (97), PNG occlusion, standard glTF tangent attributes and roughness 0.9. The neutral source PNGs remain unaltered by the variant builder.

Bone mapping:

- `Armature_rootJoint` → `root`
- `Root_Armature` → `root`
- `Body_Armature` → `chest`
- `Spine1_Armature` → `spine2`
- `Spine2_Armature` → `spine1`
- `Spine3_Armature` → `hips`
- `Hvost1_Armature` → `tail1`
- `Hvost2_Armature` → `tail2`
- `Hvost3_Armature` → `tail3`
- `Hvost4_Armature` → `tail4`
- `Hvost1.001_Armature` → `hips`
- `Hvost2.001_Armature` → `hips`
- `Hvost3.001_Armature` → `hips`
- `Hvost4.001_Armature` → `hips`
- `Taz_Armature` → `hips`
- `Bedro.R_Armature` → `thigh.R`
- `Golen.R_Armature` → `shin.R`
- `Noga.R_Armature` → `foot.R`
- `Bedro.L_Armature` → `thigh.L`
- `Golen.L_Armature` → `shin.L`
- `Noga.L_Armature` → `foot.L`
- `Bedro.IK.R_Armature` → `hips`
- `Golen.IK.R_Armature` → `hips`
- `Bedro.IK.L_Armature` → `hips`
- `Golen.IK.L_Armature` → `hips`
- `Zhivot_Armature` → `spine2`
- `Sheya_Armature` → `neck1`
- `Golova_Armature` → `head`
- `Hobot1_Armature` → `trunk1`
- `Hobot2_Armature` → `trunk3`
- `Hobot3_Armature` → `trunk5`
- `Hobot4_Armature` → `trunk7`
- `Hobot5_Armature` → `trunk9`
- `Chelust_Armature` → `jaw`
- `Chelust.001_Armature` → `lower_lip`
- `Uho1.R_Armature` → `ear.R`
- `Uho2.R_Armature` → `ear_tip.R`
- `Uho22.R_Armature` → `ear_tip.R`
- `Uho11.R_Armature` → `ear.R`
- `Uho11.R.002_Armature` → `ear.R`
- `Uho1.L_Armature` → `ear.L`
- `Uho2.L_Armature` → `ear_tip.L`
- `Uho22.L_Armature` → `ear_tip.L`
- `Uho11.L_Armature` → `ear.L`
- `Hobot1.001_Armature` → `head`
- `Hobot2.001_Armature` → `head`
- `Hobot3.001_Armature` → `head`
- `Hobot4.001_Armature` → `head`
- `Hobot5.001_Armature` → `head`
- `Chelust.002_Armature` → `head`
- `Chelust.003_Armature` → `head`
- `Uho1.R.001_Armature` → `head`
- `Uho2.R.001_Armature` → `head`
- `Uho22.R.001_Armature` → `head`
- `Uho11.R.001_Armature` → `head`
- `Uho1.L.001_Armature` → `head`
- `Uho2.L.001_Armature` → `head`
- `Uho22.L.001_Armature` → `head`
- `Uho11.L.001_Armature` → `head`
- `Lopatka.R_Armature` → `scapula.R`
- `Plecho.R_Armature` → `upper_arm.R`
- `Lokot.R_Armature` → `forearm.R`
- `Ruka.R_Armature` → `hand.R`
- `Lopatka.L_Armature` → `scapula.L`
- `Plecho.L_Armature` → `upper_arm.L`
- `Lokot.L_Armature` → `forearm.L`
- `Ruka.L_Armature` → `hand.L`
- `Lopatka.IK.R_Armature` → `chest`
- `Plecho.IK.R_Armature` → `chest`
- `Lokot.IK.R_Armature` → `chest`
- `Lopatka.IK.L_Armature` → `chest`
- `Plecho.IK.L_Armature` → `chest`
- `Lokot.IK.L_Armature` → `chest`
- `Spine1.002_Armature` → `chest`
- `Spine1.001_Armature` → `chest`
- `Spine1.003_Armature` → `chest`
- `Spine2.001_Armature` → `chest`
- `Spine2.002_Armature` → `chest`
- `Spine3.001_Armature` → `chest`
- `Sheya.001_Armature` → `chest`
- `Golova.001_Armature` → `chest`
- `Spine3.002_Armature` → `root`
- `Golen.IK.L.001_Armature` → `root`
- `Noga.IK.L_Armature` → `root`
- `CTR.Lokot.IK.L_Armature` → `root`
- `Ruka.IK.L_Armature` → `root`
- `CTR.Lokot.IK.R_Armature` → `root`
- `Ruka.IK.R_Armature` → `root`
- `Golen.IK.R.001_Armature` → `root`
- `Noga.IK.R_Armature` → `root`
