# Animated Rhinoceros Walking Animal Loop

By LasquetiSpice.

https://sketchfab.com/3d-models/animated-rhinoceros-walking-animal-loop-32abc12d4fe44bd0b116b294194d9615

License: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

Changes:

- Removed camera, lights, helper shapes and unused animation controls.
- Converted the neutral bind mesh and joints to metres and animal axes; grounded feet.
- Renamed and merged joints, summed weights, retained at most four normalized influences.
- Resampled the authored walk to 81 samples; repeated the first endpoint for exact looping.
- Extracted source images through Blender; no executable download content is used.
- Tagged eyes, horns/tusks, ears, trunk and mouth where separable; nail tags use foot position.
- Measured backward stance velocities in the hip-relative frame, using the lowest 35% of foot heights.

Prepared: 32418 triangles, 37 bones. Original local-rotation endpoint maximum difference: 0.1957.

Authored stance speeds at prepared size (minimum / median / maximum, m/s):

- FL: 0.8648 / 1.1471 / 1.2182
- FR: 1.1110 / 1.1784 / 1.1941
- HL: 0.6822 / 1.1368 / 1.1652
- HR: 0.5226 / 1.1703 / 1.5868

The game clips use contact-constrained procedural replacement motion: these source velocities vary beyond the existing 2% foot-sliding tolerance. Original resampled rotations and positions remain in the NPZ for future animation work.

Species builds lower the tail, moderately desaturate the skin, and create a calf with shorter body/head, larger ears, reduced hump and normal strength. The calf replaces the two adult horn components with closed skin patches and a front stub; patch UVs continue the surrounding forehead layout. Baked adult horn-cap pixels in the calf colour, normal and AO maps are resampled from adjacent photographic skin with a feathered border. It is decimated to 21,000 triangles.

Builds export baseline JPEG colour (quality 90), JPEG normal (97), PNG occlusion, standard glTF tangent attributes and roughness 0.9. The neutral source PNGs remain unaltered by the variant builder.

Bone mapping:

- `GLTF_created_0_rootJoint` → `root`
- `Root.4_4` → `hips`
- `Spine.5_5` → `spine1`
- `Spine1.6_6` → `spine2`
- `Spine2.7_7` → `chest`
- `Spine3.8_8` → `neck1`
- `Neck.9_9` → `neck2`
- `Head.10_10` → `head`
- `Jaw.11_11` → `jaw`
- `T_Lip.12_12` → `jaw`
- `L_T_Lip1.13_13` → `jaw`
- `L_T_Lip2.14_14` → `jaw`
- `L_C_Lip.15_15` → `jaw`
- `L_B_Lip1.16_16` → `jaw`
- `L_B_Lip2.17_17` → `jaw`
- `B_Lip.18_18` → `lower_lip`
- `R_T_Lip1.19_19` → `jaw`
- `R_T_Lip2.20_20` → `jaw`
- `R_C_Lip.21_21` → `jaw`
- `R_B_Lip1.22_22` → `jaw`
- `R_B_Lip2.23_23` → `jaw`
- `Tongue1.24_24` → `jaw`
- `Tongue2.25_25` → `jaw`
- `Tongue3.26_26` → `jaw`
- `Tongue4.27_27` → `jaw`
- `Tongue5.28_28` → `jaw`
- `L_Cheek2.29_29` → `head`
- `L_Ear1.30_30` → `ear.L`
- `L_Ear2.31_31` → `ear_tip.L`
- `R_Ear1.32_32` → `ear.R`
- `R_Ear2.33_33` → `ear_tip.R`
- `R_Cheek2.34_34` → `head`
- `L_Eye.35_35` → `head`
- `L_TopLid.36_36` → `head`
- `L_BottomLid.37_37` → `head`
- `R_Eye.38_38` → `head`
- `R_TopLid.39_39` → `head`
- `R_BottomLid.40_40` → `head`
- `L_Nose.41_41` → `head`
- `R_Nose.42_42` → `head`
- `L_Clavicle.43_43` → `scapula.L`
- `L_Shoulder.44_44` → `upper_arm.L`
- `L_Elbow.45_45` → `forearm.L`
- `L_Wrist.46_46` → `hand.L`
- `L_Hand.47_47` → `fingers.L`
- `L_Finger1.48_48` → `fingers.L`
- `L_Finger2.49_49` → `fingers.L`
- `L_Finger3.50_50` → `fingers.L`
- `L_SkinFoldBck.51_51` → `forearm.L`
- `L_FrontSkinFold.52_52` → `neck1`
- `R_FrontSkinFold.53_53` → `neck1`
- `R_Clavicle.54_54` → `scapula.R`
- `R_Shoulder.55_55` → `upper_arm.R`
- `R_Elbow.56_56` → `forearm.R`
- `R_Wrist.57_57` → `hand.R`
- `R_Hand.58_58` → `fingers.R`
- `R_Finger1.59_59` → `fingers.R`
- `R_Finger2.60_60` → `fingers.R`
- `R_Finger3.61_61` → `fingers.R`
- `R_SkinFoldBck.62_62` → `forearm.R`
- `L_Hip.63_63` → `thigh.L`
- `L_Knee.64_64` → `shin.L`
- `L_Ankle.65_65` → `foot.L`
- `L_Ball.66_66` → `toes.L`
- `L_Toe1.67_67` → `toes.L`
- `L_Toe2.68_68` → `toes.L`
- `L_Toe3.69_69` → `toes.L`
- `R_Hip.70_70` → `thigh.R`
- `R_Knee.71_71` → `shin.R`
- `R_Ankle.72_72` → `foot.R`
- `R_Ball.73_73` → `toes.R`
- `R_Toe1.74_74` → `toes.R`
- `R_Toe2.75_75` → `toes.R`
- `R_Toe3.76_76` → `toes.R`
- `Tail.77_77` → `tail1`
- `Tail1.78_78` → `tail2`
- `Tail2.79_79` → `tail3`
- `Tail3.80_80` → `tail4`
- `Tail4.81_81` → `tail5`
