# SPDX-License-Identifier: MIT
"""Generate one animal asset.

    blender -b --factory-startup --python tools/animals/build_animal.py -- \
        --species lion --out assets/animals --preview build-probe/animal-previews

Writes <out>/<species>.glb and <out>/<species>.json (manifest); with --preview also
contact-sheet PNGs of the rest pose and of every animation clip.
"""
import argparse
import importlib
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
import numpy as np  # noqa: E402

from zoogen import build, preview  # noqa: E402


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--species", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--preview")
    ap.add_argument("--no-anim", action="store_true")
    ap.add_argument("--preview-clips-only", action="store_true",
                    help="for sculpt pose revisions, render only the requested clip sheets")
    ap.add_argument("--clips", help="comma separated clip names to preview (default: all)")
    args = ap.parse_args(argv)

    t0 = time.time()
    mod = importlib.import_module("species." + args.species)
    spec = mod.make_spec()
    scene = build.reset_scene()
    mesh_ob, info = build.build_mesh(spec)
    arm_ob = build.build_armature(spec, info["bones"])
    build.bind(mesh_ob, arm_ob)
    clips = []
    anim = None
    if spec.rig is not None and not args.no_anim:
        from zoogen import anim as anim_mod
        anim = anim_mod
        clips = anim_mod.bake_clips(spec, arm_ob, info["bones"])
    os.makedirs(args.out, exist_ok=True)
    glb = os.path.join(args.out, spec.name + ".glb")
    if spec.sculpt is not None and spec.sculpt.get("rigged", False):
        build.export_glb(glb, mesh_ob, arm_ob, tangents=True, all_bones=True)
    else:
        build.export_glb(glb, mesh_ob, arm_ob)
    build.write_manifest(os.path.join(args.out, spec.name + ".json"), spec, info, clips)
    print(f"ZOOGEN {spec.name}: {info['triangles']} tris, {info['vertices']} verts, "
          f"{len(info['bones'])} bones, {len(clips)} clips, {os.path.getsize(glb)} bytes, "
          f"{time.time() - t0:.1f}s")

    if args.preview and spec.sculpt is not None and spec.sculpt.get("kind") == "scan":
        from zoogen.scan import model_previews
        model_previews(scene, arm_ob, spec, clips, args)
        return

    if args.preview and spec.sculpt is not None and spec.sculpt.get("birdscan", False):
        from zoogen.birdscan import model_previews
        model_previews(scene, arm_ob, spec, clips, args)
        return

    if args.preview and spec.sculpt is not None and spec.sculpt.get("rigged", False):
        from zoogen.rigged import model_previews
        model_previews(scene, arm_ob, spec, clips, args)
        return

    if args.preview:
        os.makedirs(args.preview, exist_ok=True)
        bones = info["bones"]
        lo = np.min([np.minimum(b.head, b.tail) for b in bones], axis=0)
        hi = np.max([np.maximum(b.head, b.tail) for b in bones], axis=0)
        center = build.to_blender((lo + hi) * 0.5)
        radius = float(np.linalg.norm(hi - lo)) * 0.5
        cam, _ = preview.setup_stage(scene, radius)
        if spec.sculpt is not None:
            preview.sculpt_lighting(scene)
        if anim is not None:
            anim.clear_pose(arm_ob)
        if not (spec.sculpt is not None and args.preview_clips_only):
            preview.render_views(scene, cam, center, radius, os.path.join(args.preview, spec.name))
            if spec.sculpt is not None:
                preview.render_eye_level(scene, cam, spec.meta["shoulder_height_m"],
                                         os.path.join(args.preview, spec.name))
        if anim is not None:
            want = args.clips.split(",") if args.clips else [c["name"] for c in clips]
            for c in clips:
                if c["name"] in want:
                    anim.preview_clip(scene, cam, arm_ob, spec.name, c, center, radius,
                                      os.path.join(args.preview, f"{spec.name}_{c['name']}"))


main()
