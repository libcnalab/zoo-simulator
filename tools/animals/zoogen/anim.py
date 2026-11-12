# SPDX-License-Identifier: MIT
"""Bakes procedural clips (rig.Clip) into Blender actions, one NLA track per clip, which
the glTF exporter writes as one animation each. Blender side of rig.py / quadruped.py."""
import numpy as np

import bpy
from mathutils import Matrix

from . import preview
from .rig import Solver, quat_from_matrix

# Animal space (+X forward, +Y left, +Z up) -> Blender armature space, see build.to_blender.
C4 = np.array([[0.0, 1.0, 0.0, 0.0], [-1.0, 0.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0],
               [0.0, 0.0, 0.0, 1.0]])


def rest_matrices(arm_ob):
    """Bone rest matrices in animal space."""
    inv = np.linalg.inv(C4)
    return {b.name: inv @ np.array(b.matrix_local, dtype=np.float64) for b in arm_ob.data.bones}


def make_solver(spec, arm_ob, bones):
    return Solver(bones, rest_matrices(arm_ob), spec.rig)


def _curve(action, path, index, group, frames, values):
    fc = action.fcurves.new(data_path=path, index=index, action_group=group)
    fc.keyframe_points.add(len(frames))
    co = np.empty(len(frames) * 2, dtype=np.float64)
    co[0::2] = frames
    co[1::2] = values
    fc.keyframe_points.foreach_set("co", co)
    for kp in fc.keyframe_points:
        kp.interpolation = 'LINEAR'
    fc.update()


def sample_clip(solver, clip, fps=30):
    """Samples a clip: (frames n+1, {bone: quaternions (n+1, 4)}, {bone: locations (n+1, 3)}).

    A looping clip's last frame repeats its first, so it plays seamlessly."""
    n = max(1, int(round(clip.duration * fps)))
    quats, locs = {}, {}
    solver.reset_stats()
    for i in range(n + 1):
        u = i / n
        if clip.loop and i == n:
            u = 0.0
        basis = solver.basis(solver.solve(clip.pose(u)))
        for name, m in basis.items():
            q = quat_from_matrix(m[:3, :3])
            if name not in quats:
                quats[name] = np.zeros((n + 1, 4))
                locs[name] = np.zeros((n + 1, 3))
            elif np.dot(q, quats[name][i - 1]) < 0.0:
                q = -q                      # the short way round, or interpolation spins the bone
            quats[name][i] = q
            locs[name][i] = m[:3, 3]
    return n, quats, locs


def bake_clips(spec, arm_ob, bones, fps=30):
    """Creates the actions and NLA tracks; returns the manifest entries of the clips."""
    scene = bpy.context.scene
    scene.render.fps = fps
    scene.render.fps_base = 1.0
    solver = make_solver(spec, arm_ob, bones)
    clips = spec.clips(solver)
    for pb in arm_ob.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    ad = arm_ob.animation_data_create()
    out = []
    for clip in clips:
        n, quats, locs = sample_clip(solver, clip, fps)
        frames = np.arange(n + 1, dtype=np.float64)
        action = bpy.data.actions.new(f"{spec.name}_{clip.name}")
        for name in quats:
            path = f'pose.bones["{name}"]'
            for k in range(4):
                _curve(action, path + ".rotation_quaternion", k, name, frames, quats[name][:, k])
            if spec.key_all_locations or np.abs(locs[name]).max() > 1e-5:
                for k in range(3):
                    _curve(action, path + ".location", k, name, frames, locs[name][:, k])
        track = ad.nla_tracks.new()
        track.name = clip.name
        track.strips.new(clip.name, 0, action)
        if solver.ik_error > 1e-4:
            print(f"ZOOGEN warning: {spec.name}/{clip.name}: a foot target is "
                  f"{solver.ik_error * 100:.1f} cm out of reach")
        print(f"ZOOGEN clip {clip.name}: {n + 1} frames, {n / fps:.2f} s, speed {clip.speed:.2f} m/s, "
              f"max leg extension {solver.extension:.3f}")
        out.append({
            "name": clip.name, "duration": round(n / fps, 4), "loop": bool(clip.loop),
            "speed": round(float(clip.speed), 4), "frames": n + 1,
            "events": [{"time": round(float(u) * n / fps, 4), "name": name} for u, name in clip.events],
        })
    ad.action = None
    return out


def clear_pose(arm_ob):
    ad = arm_ob.animation_data
    if ad is not None:
        ad.action = None
        ad.use_nla = False
    for pb in arm_ob.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()


def preview_clip(scene, cam, arm_ob, spec_name, clip, center, radius, prefix, phases=8,
                 res=(480, 360), view="side"):
    """Renders `phases` evenly spaced moments of a clip into one filmstrip PNG."""
    ad = arm_ob.animation_data
    ad.use_nla = False
    ad.action = bpy.data.actions[f"{spec_name}_{clip['name']}"]
    n = clip["frames"] - 1
    span = n if clip["loop"] else n + 1
    tiles = []
    for i in range(phases):
        scene.frame_set(min(n, int(round(i * span / phases))))
        tiles.append(preview.render_tile(scene, cam, center, radius, view, res,
                                         f"{prefix}_{i}.png", distance=5.2))
    ad.action = None
    return preview.save_sheet(tiles, 4, res, prefix + ".png")
