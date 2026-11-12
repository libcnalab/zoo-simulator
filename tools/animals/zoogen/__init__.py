# SPDX-License-Identifier: MIT
"""zoogen: procedural, reproducible animal asset generator for Zoo Simulator.

Runs inside Blender's Python (blender -b --python tools/animals/build_animal.py).
An animal is described in "animal space": +X forward (nose), +Y left, +Z up, metres,
origin on the ground below the body.  The exporter writes glTF 2.0 (+Y up, the animal
facing +Z), one skinned mesh, one material with a base-colour texture, and one glTF
animation per clip.
"""
