# SPDX-License-Identifier: MIT
"""African lion (Panthera leo), adult male.

Proportions follow published field measurements for adult males: shoulder height
about 1.2 m, head-and-body length about 1.9-2.0 m, tail about 0.9 m.  Digitigrade
limbs; the scapula is its own bone because a walking cat's shoulder blades visibly
slide under the skin.
"""
import numpy as np

from zoogen import noise as nz
from zoogen.geom import Bone, Part, R, Spec

Y_F = 0.17      # lateral offset of the front limbs
Y_H = 0.16      # lateral offset of the hind limbs
EYE = (1.065, 0.083, 1.338)


def _bones():
    B = Bone
    return [
        B("root", None, (0, 0, 0), (0.3, 0, 0), deform=False),
        B("hips", "root", (-0.62, 0, 1.02), (-0.38, 0, 1.03)),
        B("spine1", "hips", (-0.38, 0, 1.03), (-0.10, 0, 1.03)),
        B("spine2", "spine1", (-0.10, 0, 1.03), (0.18, 0, 1.04)),
        B("chest", "spine2", (0.18, 0, 1.04), (0.46, 0, 1.07)),
        B("neck1", "chest", (0.46, 0, 1.07), (0.66, 0, 1.20)),
        B("neck2", "neck1", (0.66, 0, 1.20), (0.84, 0, 1.32)),
        B("head", "neck2", (0.84, 0, 1.32), (1.22, 0, 1.23)),
        B("jaw", "head", (0.90, 0, 1.225), (1.19, 0, 1.15)),
        B("ear.L", "head", (0.87, 0.10, 1.40), (0.85, 0.135, 1.50)),
        B("tail1", "hips", (-0.80, 0, 1.02), (-1.00, 0, 0.82)),
        B("tail2", "tail1", (-1.00, 0, 0.82), (-1.15, 0, 0.58)),
        B("tail3", "tail2", (-1.15, 0, 0.58), (-1.26, 0, 0.36)),
        B("tail4", "tail3", (-1.26, 0, 0.36), (-1.42, 0, 0.28)),
        B("scapula.L", "chest", (0.36, Y_F - 0.02, 1.10), (0.50, Y_F, 0.84)),
        B("upper_arm.L", "scapula.L", (0.50, Y_F, 0.84), (0.36, Y_F, 0.60)),
        B("forearm.L", "upper_arm.L", (0.36, Y_F, 0.60), (0.42, Y_F - 0.01, 0.30)),
        B("hand.L", "forearm.L", (0.42, Y_F - 0.01, 0.30), (0.45, Y_F - 0.01, 0.075)),
        B("fingers.L", "hand.L", (0.45, Y_F - 0.01, 0.075), (0.58, Y_F - 0.01, 0.035)),
        B("thigh.L", "hips", (-0.58, Y_H - 0.01, 0.98), (-0.42, Y_H + 0.01, 0.66)),
        B("shin.L", "thigh.L", (-0.42, Y_H + 0.01, 0.66), (-0.74, Y_H, 0.36)),
        B("foot.L", "shin.L", (-0.74, Y_H, 0.36), (-0.65, Y_H, 0.075)),
        B("toes.L", "foot.L", (-0.65, Y_H, 0.075), (-0.52, Y_H, 0.035)),
    ]


def _chain(bones, *names):
    by = {b.name: b for b in bones}
    return [(n, by[n].head, by[n].tail) for n in names]


def _ball(name, c, r, bone, tag, mirror=True):
    c = np.asarray(c, dtype=float)
    dx = np.array([0.55 * r, 0, 0])
    return Part(name, [R(c - dx, 0.82 * r, 0.82 * r), R(c, r, r), R(c + dx, 0.82 * r, 0.82 * r)],
                weights={bone: 1.0}, fuse=False, tag=tag, segments=12, subdiv=1, mirror=mirror,
                cap_scale=0.75)


def _parts(bones):
    spine = _chain(bones, "hips", "spine1", "spine2", "chest")
    neck = _chain(bones, "chest", "neck1", "neck2", "head")
    fleg = _chain(bones, "scapula.L", "upper_arm.L", "forearm.L", "hand.L", "fingers.L")
    hleg = _chain(bones, "thigh.L", "shin.L", "foot.L", "toes.L")
    tail = _chain(bones, "hips", "tail1", "tail2", "tail3", "tail4")
    return [
        Part("torso", [
            R((-0.82, 0, 0.98), 0.09, 0.08, 0.11),
            R((-0.62, 0, 0.91), 0.20, 0.20, 0.25),
            R((-0.35, 0, 0.88), 0.215, 0.22, 0.235),
            R((-0.05, 0, 0.85), 0.235, 0.255, 0.275),
            R((0.22, 0, 0.83), 0.245, 0.31, 0.31),
            R((0.42, 0, 0.86), 0.225, 0.305, 0.30),
            R((0.58, 0, 0.92), 0.17, 0.23, 0.25),
        ], chain=spine, segments=28, cap_scale=0.6),
        Part("neck", [
            R((0.48, 0, 0.99), 0.15, 0.20, 0.20),
            R((0.66, 0, 1.15), 0.125, 0.15, 0.16),
            R((0.83, 0, 1.28), 0.115, 0.13, 0.13),
        ], chain=neck, segments=20, cap_start="flat", cap_end="flat"),
        Part("mane", [
            R((0.36, 0, 1.00), 0.20, 0.21, 0.26),
            R((0.56, 0, 1.08), 0.265, 0.27, 0.37),
            R((0.74, 0, 1.22), 0.255, 0.25, 0.31),
            R((0.87, 0, 1.31), 0.205, 0.20, 0.22),
        ], chain=neck, segments=28, tag="mane", rough=(0.035, 13.0), cap_scale=0.5),
        Part("skull", [
            R((0.79, 0, 1.325), 0.115, 0.115, 0.11),
            R((0.90, 0, 1.335), 0.15, 0.135, 0.125),
            R((1.00, 0, 1.31), 0.135, 0.118, 0.105, n=2.3),
            R((1.08, 0, 1.268), 0.105, 0.09, 0.08, n=2.6),
            R((1.16, 0, 1.24), 0.098, 0.078, 0.062, n=2.8),
            R((1.215, 0, 1.228), 0.082, 0.06, 0.05, n=2.8),
        ], weights={"head": 1.0}, segments=22, cap_scale=0.35),
        Part("jaw", [
            R((0.93, 0, 1.205), 0.10, 0.03, 0.05),
            R((1.06, 0, 1.17), 0.085, 0.03, 0.048, n=2.5),
            R((1.18, 0, 1.158), 0.07, 0.027, 0.04, n=2.6),
        ], weights={"jaw": 1.0}, segments=16, cap_scale=0.4, tag="jaw"),
        Part("ear.L", [
            R((0.875, 0.095, 1.395), 0.042, 0.016),
            R((0.865, 0.12, 1.455), 0.046, 0.014),
            R((0.855, 0.135, 1.495), 0.026, 0.012),
        ], weights={"ear.L": 1.0}, segments=12, mirror=True, tag="ear",
            side_ref=(0.25, 1.0, 0.0)),
        _ball("eye.L", EYE, 0.024, "head", "eye"),
        Part("nose", [R((1.215, 0, 1.262), 0.042, 0.024, 0.026), R((1.25, 0, 1.252), 0.036, 0.02, 0.024)],
             weights={"head": 1.0}, segments=12, subdiv=1, fuse=False, tag="nose", cap_scale=0.8),
        Part("front_leg.L", [
            R((0.38, Y_F - 0.03, 1.02), 0.07, 0.13, 0.13),
            R((0.47, Y_F, 0.84), 0.09, 0.12, 0.12),
            R((0.40, Y_F, 0.70), 0.085, 0.10, 0.11),
            R((0.365, Y_F, 0.60), 0.07, 0.078, 0.092),
            R((0.39, Y_F - 0.005, 0.46), 0.062, 0.066, 0.07),
            R((0.42, Y_F - 0.01, 0.30), 0.052, 0.05, 0.052),
            R((0.44, Y_F - 0.01, 0.16), 0.048, 0.046, 0.046),
            R((0.455, Y_F - 0.01, 0.075), 0.054, 0.05, 0.05),
        ], chain=fleg, mirror=True, segments=16, radius_scale=1.3),
        Part("front_paw.L", [
            R((0.44, Y_F - 0.01, 0.052), 0.058, 0.048, 0.046),
            R((0.52, Y_F - 0.01, 0.046), 0.068, 0.042, 0.04),
            R((0.585, Y_F - 0.01, 0.04), 0.06, 0.034, 0.034),
        ], chain=fleg[3:], mirror=True, segments=14, tag="paw", radius_scale=1.22),
        Part("hind_leg.L", [
            R((-0.59, Y_H - 0.03, 0.97), 0.10, 0.17, 0.15),
            R((-0.50, Y_H, 0.81), 0.10, 0.165, 0.14),
            R((-0.43, Y_H + 0.01, 0.67), 0.075, 0.09, 0.10),
            R((-0.57, Y_H + 0.005, 0.52), 0.06, 0.07, 0.075),
            R((-0.73, Y_H, 0.37), 0.046, 0.046, 0.05),
            R((-0.70, Y_H, 0.22), 0.042, 0.04, 0.04),
            R((-0.655, Y_H, 0.08), 0.05, 0.046, 0.046),
        ], chain=hleg, mirror=True, segments=16, radius_scale=1.28),
        Part("hind_paw.L", [
            R((-0.665, Y_H, 0.052), 0.055, 0.046, 0.045),
            R((-0.59, Y_H, 0.046), 0.064, 0.04, 0.04),
            R((-0.525, Y_H, 0.04), 0.057, 0.033, 0.034),
        ], chain=hleg[2:], mirror=True, segments=14, tag="paw", radius_scale=1.2),
        Part("tail", [
            R((-0.76, 0, 1.03), 0.045, 0.045),
            R((-1.00, 0, 0.82), 0.032, 0.032),
            R((-1.15, 0, 0.58), 0.027, 0.027),
            R((-1.26, 0, 0.36), 0.024, 0.024),
            R((-1.33, 0, 0.31), 0.024, 0.024),
        ], chain=tail, segments=10, cap_end="flat"),
        Part("tuft", [
            R((-1.32, 0, 0.315), 0.03, 0.03),
            R((-1.38, 0, 0.29), 0.052, 0.05),
            R((-1.45, 0, 0.265), 0.03, 0.03),
        ], weights={"tail4": 1.0}, segments=10, tag="tuft", rough=(0.012, 30.0)),
    ]


def paint(ctx):
    P, N = ctx.P, ctx.N
    tawny = np.array([0.74, 0.58, 0.36])
    pale = np.array([0.91, 0.83, 0.67])
    dark_mane = np.array([0.30, 0.19, 0.10])
    light_mane = np.array([0.60, 0.42, 0.22])
    # Countershading: paler belly, chest, inner legs and muzzle.
    under = nz.smoothstep(0.15, -0.75, N[..., 2]) * nz.smoothstep(1.05, 0.55, ctx.z)
    inner = nz.smoothstep(0.25, 0.85, -N[..., 1] * np.sign(ctx.y)) * ctx.bones(
        "upper_arm", "forearm", "thigh", "shin")
    coat = nz.mix(tawny, pale, np.clip(under * 0.85 + inner * 0.6, 0, 1))
    grain = nz.fbm(P, 55.0, 3, seed=3)
    blotch = nz.fbm(P, 6.0, 3, seed=5)
    coat = coat * (0.90 + 0.16 * grain)[..., None] * (0.94 + 0.12 * blotch)[..., None]
    # Dorsal line slightly darker.
    dorsal = nz.smoothstep(0.6, 1.0, N[..., 2]) * nz.smoothstep(0.9, 1.05, ctx.z)
    coat = coat * (1.0 - 0.10 * dorsal)[..., None]
    # Face: pale muzzle and chin, dark nose leather.
    muzzle = nz.smoothstep(1.06, 1.14, ctx.x) * ctx.bones("head", "jaw")
    coat = nz.mix(coat, pale * 0.98, muzzle * 0.55)
    coat = nz.mix(coat, pale, ctx.tag("jaw") * 0.6)
    # Mane: dark, streaked along the hair direction, lighter around the face.
    m = np.clip(ctx.tag("mane") * 1.4, 0, 1)
    streak = nz.fbm(P * np.array([3.0, 14.0, 14.0]), 4.0, 3, seed=9)
    face_ring = nz.smoothstep(0.62, 0.92, ctx.x)
    mane_col = nz.mix(dark_mane, light_mane, np.clip(0.25 + 0.9 * face_ring * streak + 0.25 * streak, 0, 1))
    coat = nz.mix(coat, mane_col, m)
    coat = nz.mix(coat, dark_mane * 0.8, np.clip(ctx.tag("tuft") * 1.5, 0, 1))
    coat = nz.mix(coat, np.array([0.33, 0.24, 0.17]), nz.smoothstep(1.47, 1.5, ctx.z) * ctx.tag("ear"))
    coat = nz.mix(coat, np.array([0.10, 0.07, 0.06]), ctx.tag("nose"))
    # Eyes: amber iris, round pupil, looking forward and slightly out.
    eye = ctx.tag("eye")
    c = np.array(EYE)
    d = np.stack([ctx.x - c[0], np.abs(ctx.y) - c[1], ctx.z - c[2]], axis=-1)
    look = np.array([0.80, 0.60, 0.0])
    off = d - look * (d @ look)[..., None]
    r = np.linalg.norm(off, axis=-1) / 0.024
    front = (d @ look) > 0
    iris = nz.mix(np.array([0.72, 0.50, 0.14]), np.array([0.06, 0.05, 0.04]),
                  nz.smoothstep(0.75, 0.9, r))
    iris = nz.mix(np.array([0.02, 0.02, 0.02]), iris, nz.smoothstep(0.28, 0.36, r))
    iris = np.where(front[..., None], iris, np.array([0.08, 0.06, 0.05]))
    coat = nz.mix(coat, iris, np.clip(eye * 3.0, 0, 1))
    return coat


def make_spec():
    bones = _bones()
    return Spec(
        name="lion", bones=bones, parts=_parts(bones), paint=paint,
        texture_size=1024, voxel=0.011, target_tris=18000, smooth_iters=3,
        meta={"common_name": "African lion", "scientific_name": "Panthera leo",
              "shoulder_height_m": 1.2, "sex": "male", "roughness": 0.9},
    )
