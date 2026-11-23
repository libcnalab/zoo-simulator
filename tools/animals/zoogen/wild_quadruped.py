# SPDX-License-Identifier: MIT
"""Shared construction helpers for the additional wild quadruped species."""
import numpy as np

from . import noise as nz
from . import quadruped as quad
from .geom import Bone, Part, R, Spec
from .rig import Leg, Rig


def _part(name, rings, bone, tag="body", **kwargs):
    return Part(name, rings, weights={bone: 1.0}, tag=tag, **kwargs)


def make_wild_quadruped(config):
    """Build a named quadruped from body measurements and anatomical proportions."""
    name = config["name"]
    height = config["height"]
    length = config["length"]
    kind = config["kind"]
    bear = kind in ("brown_bear", "polar_bear")
    zebra = kind == "zebra"
    tiger = kind == "tiger"
    wolf = kind == "wolf"
    coat_variant = config.get("coat_variant", "grey_wolf")
    skull_scale = config.get("skull_scale", 1.0)
    muzzle_width_scale = config.get("muzzle_width_scale", 1.0)
    muzzle_length_scale = config.get("muzzle_length_scale", 1.0)
    ear_scale = config.get("ear_scale", 1.0)
    eye_scale = config.get("eye_scale", 1.0)
    paw_scale = config.get("paw_scale", 1.0)
    body_z = height * (0.70 if bear else 0.72 if zebra else 0.70)
    front_x = length * 0.20
    hip_x = -length * 0.27
    chest_x = length * 0.13
    head_z = height * (0.91 if zebra else 0.94 if bear else 0.91)
    nose_x = length * 0.50
    brown_face_drop = (0.30 * height if kind == "polar_bear" else
                       0.24 * height if kind == "brown_bear" else
                       0.30 * height if zebra else 0.0)
    rear_x = -length * 0.50
    leg_y = height * (0.145 if bear else 0.105 if zebra else 0.115)
    foot_z = 0.055 if bear else 0.035
    shoulder_top = height
    shoulder_x = front_x
    rump_z = height * (0.88 if kind == "brown_bear" else 0.97 if kind == "polar_bear" else 0.90)

    bones = [
        Bone("root", None, (0, 0, 0), (0.25, 0, 0), deform=False),
        Bone("hips", "root", (hip_x, 0, body_z), (hip_x + 0.22, 0, body_z)),
        Bone("spine1", "hips", (hip_x + 0.22, 0, body_z), (0, 0, body_z)),
        Bone("spine2", "spine1", (0, 0, body_z), (chest_x, 0, body_z + 0.015 * height)),
        Bone("chest", "spine2", (chest_x, 0, body_z + 0.015 * height),
             (front_x, 0, body_z + 0.04 * height)),
        Bone("neck1", "chest", (front_x, 0, body_z + 0.04 * height),
             (front_x + 0.10 * length, 0, head_z - 0.08 * height)),
        Bone("neck2", "neck1", (front_x + 0.10 * length, 0, head_z - 0.08 * height),
             (nose_x - 0.20 * length, 0, head_z)),
        Bone("head", "neck2", (nose_x - 0.20 * length, 0, head_z),
             (nose_x - 0.035 * length, 0, head_z - 0.04 * height)),
        Bone("jaw", "head", (nose_x - 0.18 * length, 0, head_z - 0.07 * height),
             (nose_x - 0.01 * length, 0, head_z - (0.25 * height + brown_face_drop))),
        Bone("ear.L", "head", (nose_x - 0.19 * length, 0.10 * height, head_z + 0.02 * height),
             (nose_x - 0.20 * length, 0.12 * height, head_z + 0.12 * height)),
        Bone("tail1", "hips", (rear_x + 0.08 * length, 0, body_z),
             (rear_x - 0.05 * length, 0, body_z - 0.08 * height)),
        Bone("tail2", "tail1", (rear_x - 0.05 * length, 0, body_z - 0.08 * height),
             (rear_x - 0.12 * length, 0, body_z - 0.18 * height)),
        Bone("tail3", "tail2", (rear_x - 0.12 * length, 0, body_z - 0.18 * height),
             (rear_x - 0.15 * length, 0, body_z - 0.28 * height)),
        Bone("tail4", "tail3", (rear_x - 0.15 * length, 0, body_z - 0.28 * height),
             (rear_x - 0.17 * length, 0, body_z - 0.36 * height)),
        Bone("scapula.L", "chest", (front_x, leg_y - 0.02, shoulder_top - 0.04 * height),
             (front_x, leg_y, shoulder_top - 0.06 * height)),
        Bone("upper_arm.L", "scapula.L", (front_x, leg_y, shoulder_top - 0.06 * height),
             (front_x - 0.06 * length, leg_y, shoulder_top - 0.48 * height)),
        Bone("forearm.L", "upper_arm.L", (front_x - 0.06 * length, leg_y, shoulder_top - 0.48 * height),
             (front_x + 0.01 * length, leg_y, 0.08 * height)),
        Bone("hand.L", "forearm.L", (front_x + 0.01 * length, leg_y, 0.08 * height),
             (front_x + 0.03 * length, leg_y, foot_z)),
        Bone("fingers.L", "hand.L", (front_x + 0.03 * length, leg_y, foot_z),
             (front_x + (0.10 if bear else 0.075) * length, leg_y, 0.025)),
        Bone("thigh.L", "hips", (hip_x, leg_y, rump_z - 0.02 * height),
             (hip_x + 0.09 * length, leg_y, 0.45 * height)),
        Bone("shin.L", "thigh.L", (hip_x + 0.09 * length, leg_y, 0.45 * height),
             (hip_x - 0.025 * length, leg_y, 0.10 * height)),
        Bone("foot.L", "shin.L", (hip_x - 0.025 * length, leg_y, 0.10 * height),
             (hip_x + (0.015 if bear else 0.025) * length, leg_y, foot_z)),
        Bone("toes.L", "foot.L", (hip_x + (0.015 if bear else 0.025) * length, leg_y, foot_z),
             (hip_x + (0.13 if bear else 0.10) * length, leg_y, 0.025)),
    ]

    def chain(*names):
        return [(b.name, b.head, b.tail) for b in bones if b.name in names]
    torso_chain = chain("hips", "spine1", "spine2", "chest")
    neck_chain = chain("chest", "neck1", "neck2", "head")
    front_chain = chain("scapula.L", "upper_arm.L", "forearm.L", "hand.L", "fingers.L")
    hind_chain = chain("thigh.L", "shin.L", "foot.L", "toes.L")
    tail_chain = chain("hips", "tail1", "tail2", "tail3", "tail4")

    trunk_width = height * (0.31 if bear else 0.23 if zebra else 0.22 if wolf else 0.28)
    trunk_depth = height * (0.28 if bear else 0.25 if zebra else 0.23 if wolf else 0.28)
    parts = [Part("torso", [
        R((rear_x, 0, rump_z - 0.06 * height), 0.10 * height, 0.12 * height),
        R((hip_x, 0, body_z - 0.015 * height), 0.78 * trunk_width, 0.77 * trunk_depth),
        R((hip_x + 0.23 * length, 0, body_z - 0.03 * height), trunk_width, trunk_depth),
        R((0, 0, body_z - 0.04 * height), 1.04 * trunk_width, trunk_depth),
        R((chest_x, 0, body_z), 1.04 * trunk_width, 1.04 * trunk_depth),
        R((front_x + 0.04 * length, 0, body_z + 0.01 * height), 0.84 * trunk_width,
          0.89 * trunk_depth),
    ], chain=torso_chain, segments=28, cap_scale=0.55)]

    neck_width = height * (0.14 if zebra else 0.13 if kind == "polar_bear" else 0.15)
    parts.append(Part("neck", [
        R((front_x - 0.05 * length, 0, body_z), neck_width, 0.17 * height),
        R((front_x + 0.08 * length, 0, body_z + 0.10 * height), 0.78 * neck_width,
          0.15 * height),
        R((nose_x - 0.22 * length, 0, head_z - 0.03 * height), 0.66 * neck_width,
          0.13 * height),
    ], chain=neck_chain, segments=22, cap_start="flat", cap_end="flat"))

    if wolf:
        # A wolf's double coat breaks the neck-to-chest outline instead of following the
        # underlying cylinder. This overlapping mass is fused into the body so there is no seam.
        parts.append(Part("neck_ruff", [
            R((front_x - 0.08 * length, 0, body_z + 0.02 * height), 0.22 * height,
              0.25 * height, 0.28 * height),
            R((front_x + 0.015 * length, 0, body_z + 0.10 * height), 0.20 * height,
              0.22 * height, 0.27 * height),
            R((front_x + 0.09 * length, 0, body_z + 0.16 * height), 0.15 * height,
              0.17 * height, 0.20 * height),
        ], chain=neck_chain, tag="ruff", segments=24, rough=(0.008 * height, 42.0 / height),
           cap_start="flat", cap_end="flat"))

    if kind == "brown_bear":
        parts.append(Part("shoulder_hump", [
            R((front_x - 0.09 * length, 0, body_z + 0.05 * height), 0.17 * height,
              0.22 * height),
            R((front_x + 0.015 * length, 0, body_z + 0.12 * height), 0.22 * height,
              0.18 * height),
            R((front_x + 0.13 * length, 0, body_z + 0.07 * height), 0.17 * height,
              0.21 * height),
        ], chain=neck_chain, tag="hump", segments=22, rough=(0.012, 16.0)))
    if zebra:
        parts.append(Part("upright_mane", [
            R((nose_x - 0.20 * length, 0, head_z + 0.13 * height), 0.080 * height,
              0.070 * height),
            R((front_x + 0.035 * length, 0, body_z + 0.38 * height), 0.085 * height,
              0.080 * height),
            R((front_x - 0.08 * length, 0, body_z + 0.28 * height), 0.065 * height,
              0.060 * height),
        ], chain=neck_chain, tag="mane", segments=18, rough=(0.008, 24.0), cap_start="flat",
           cap_end="flat", fuse=False))

    if tiger:
        parts.append(Part("cheek_ruff.L", [
            R((nose_x - 0.22 * length, 0.085 * height, head_z - 0.015 * height),
              0.075 * height, 0.070 * height),
            R((nose_x - 0.15 * length, 0.090 * height, head_z - 0.045 * height),
              0.080 * height, 0.075 * height),
            R((nose_x - 0.09 * length, 0.075 * height, head_z - 0.060 * height),
              0.045 * height, 0.050 * height),
        ], weights={"head": 1.0}, tag="ruff", segments=14, mirror=True,
           rough=(0.006, 18.0), fuse=False))

    skull_width = height * (0.12 if kind == "polar_bear" else 0.17 if kind == "brown_bear"
                            else 0.105 if wolf else 0.12 if zebra else 0.15 if tiger else 0.13)
    skull_width *= skull_scale
    muzzle_x = nose_x - (0.20 if wolf else 0.18 if kind == "polar_bear"
                         else 0.08 if zebra else 0.10 if bear else 0.075) * length * muzzle_length_scale
    muzzle_width = height * (0.08 if wolf else 0.095 if zebra else 0.09 if bear
                             else 0.11 if tiger else 0.10)
    muzzle_width *= muzzle_width_scale
    parts.append(Part("skull", [
        R((nose_x - 0.22 * length, 0, head_z), 0.70 * skull_width, 0.11 * height),
        R((nose_x - 0.145 * length, 0, head_z - 0.005 * height), skull_width,
          0.12 * height, n=2.3),
        R((muzzle_x, 0, head_z - 0.045 * height - brown_face_drop), 0.78 * muzzle_width,
          0.075 * height, n=2.5),
        R((nose_x - 0.018 * length, 0, head_z - 0.06 * height - brown_face_drop),
          0.58 * muzzle_width,
          0.05 * height, n=2.6),
    ], weights={"head": 1.0}, tag="head", segments=20, cap_scale=0.45))
    parts.append(Part("jaw", [
        R((nose_x - 0.15 * length * muzzle_length_scale, 0,
          head_z - 0.08 * height - brown_face_drop),
          0.70 * muzzle_width,
          0.035 * height),
        R((nose_x - 0.04 * length, 0, head_z - 0.115 * height - brown_face_drop),
          0.60 * muzzle_width,
          0.03 * height),
    ], weights={"jaw": 1.0}, tag="jaw", segments=16, cap_scale=0.4))
    ear_height = (0.20 * height if wolf else 0.15 * height if zebra else
                  0.075 * height if bear else 0.10 * height if tiger else 0.11 * height) * ear_scale
    ear_x = nose_x - 0.20 * length
    parts.append(Part("ear.L", [
        R((ear_x, 0.09 * height, head_z + 0.03 * height),
          (0.058 if wolf or zebra else 0.045) * height * ear_scale, 0.04 * height * ear_scale),
        R((ear_x - 0.01 * length, 0.11 * height, head_z + 0.65 * ear_height),
          (0.052 if wolf or zebra else 0.04) * height * ear_scale, 0.045 * height * ear_scale),
        R((ear_x - 0.02 * length, 0.12 * height, head_z + ear_height),
          (0.004 * height if wolf or zebra else 0.018 * height) * ear_scale,
          (0.006 * height if wolf or zebra else 0.012 * height) * ear_scale),
    ], weights={"ear.L": 1.0}, tag="ear", segments=12, mirror=True, side_ref=(0.25, 1, 0),
       cap_scale=0.3))
    eye = np.array([nose_x - 0.155 * length, 0.075 * height, head_z + 0.015 * height])
    parts.append(Part("eye.L", [R(eye, 0.018 * height * eye_scale, 0.018 * height * eye_scale)],
                      weights={"head": 1.0},
                      tag="eye", segments=12, subdiv=1, fuse=False, mirror=True))
    parts.append(Part("nose", [
        R((nose_x - 0.015 * length, 0, head_z - 0.035 * height - brown_face_drop),
          0.040 * height,
          0.028 * height),
        R((nose_x + 0.01 * length, 0, head_z - 0.045 * height - brown_face_drop),
          0.034 * height,
          0.023 * height),
    ], weights={"head": 1.0}, tag="nose", segments=12, subdiv=1, fuse=False))

    f_rings = [
        R((front_x, leg_y, shoulder_top - 0.06 * height), 0.105 * height, 0.13 * height),
        R((front_x, leg_y, shoulder_top - 0.06 * height), 0.09 * height, 0.10 * height),
        R((front_x - 0.06 * length, leg_y, shoulder_top - 0.48 * height),
          0.067 * height, 0.075 * height),
        R((front_x + 0.01 * length, leg_y, 0.08 * height), 0.052 * height, 0.060 * height),
        R((front_x + 0.03 * length, leg_y, foot_z), 0.055 * height, 0.045 * height),
    ]
    h_rings = [
        R((hip_x, leg_y, rump_z - 0.04 * height), 0.13 * height, 0.15 * height),
        R((hip_x + 0.09 * length, leg_y, 0.45 * height), 0.10 * height, 0.12 * height),
        R((hip_x - 0.025 * length, leg_y, 0.10 * height), 0.060 * height, 0.07 * height),
        R((hip_x + 0.015 * length, leg_y, foot_z), 0.055 * height, 0.045 * height),
    ]
    if wolf:
        # The generic limb starts with two rings at the same shoulder point to make a bear's
        # heavy upper arm. On a wolf that reads as a ball, so keep one lean transition instead.
        f_rings = [f_rings[0], *f_rings[2:]]
        f_rings[0].ry = 0.085 * height
        f_rings[0].up = f_rings[0].dn = 0.11 * height
        h_rings[0].ry = 0.105 * height
        h_rings[0].up = h_rings[0].dn = 0.13 * height
    parts.extend([
        Part("front_leg.L", f_rings, chain=front_chain, tag="leg", segments=16, mirror=True,
             radius_scale=1.15 if bear else 1.0),
        Part("hind_leg.L", h_rings, chain=hind_chain, tag="leg", segments=16, mirror=True,
             radius_scale=1.2 if bear else 1.0),
    ])

    paw_x = front_x + (0.18 if bear else 0.09) * height
    paw_back_x = front_x - 0.12 * height if bear else front_x + 0.015 * length
    hind_paw_back = hip_x - 0.12 * height if bear else hip_x + 0.015 * length
    paw_rx = (0.13 if bear else 0.075 if zebra else 0.085) * height * paw_scale
    paw_ry = (0.10 if bear else 0.07 if zebra else 0.067) * height * paw_scale
    hoof_tag = "hoof" if zebra else "paw"
    paw_seg = 8 if zebra else 16
    parts.extend([
        Part("front_paw.L", [R((paw_back_x, leg_y, 0.050), paw_ry, paw_ry * 0.65),
                              R((paw_x, leg_y, 0.042), paw_rx, paw_ry)],
             chain=front_chain[3:], tag=hoof_tag, segments=paw_seg, mirror=True,
             cap_scale=0.45),
        Part("hind_paw.L", [R((hind_paw_back, leg_y, 0.050), paw_ry, paw_ry * 0.65),
                             R((hip_x + (0.18 if bear else 0.10) * height, leg_y, 0.042),
                               paw_rx, paw_ry)],
             chain=hind_chain[2:], tag=hoof_tag, segments=paw_seg, mirror=True,
             cap_scale=0.45),
    ])
    if bear:
        for front, x in ((True, paw_x), (False, hip_x + 0.13 * length)):
            for digit in range(4):
                y = leg_y + (digit - 1.5) * 0.035 * height
                claw_x = x + 0.105 * height
                parts.append(_part("claw", [R((claw_x, y, 0.047), 0.017 * height, 0.018 * height),
                                              R((claw_x + 0.045 * height, y, 0.025),
                                                0.004 * height, 0.005 * height)],
                                   "fingers.L" if front else "toes.L", tag="claw", segments=8,
                                   subdiv=2, fuse=False, mirror=True, cap_start="flat", cap_end="point"))

    tail_len = 0.23 * length if tiger else 0.18 * length if zebra else 0.30 * length if wolf else 0.10 * length
    tail_r = 0.022 * height if tiger else 0.070 * height if wolf else 0.018 * height
    parts.append(Part("tail", [
        R((rear_x + 0.08 * length, 0, body_z), 0.045 * height, 0.045 * height),
        R((rear_x - 0.35 * tail_len, 0, body_z - 0.08 * height), tail_r, tail_r),
        R((rear_x - 0.70 * tail_len, 0, body_z - 0.18 * height), tail_r, tail_r),
        R((rear_x - tail_len, 0, body_z - 0.30 * height), tail_r, tail_r),
    ], chain=tail_chain, tag="tail", segments=16 if wolf else 12, cap_end="flat",
       rough=(0.007 * height, 48.0 / height) if wolf else None))
    if wolf or zebra:
        if wolf:
            tuft_rings = [
                R((rear_x - tail_len, 0, body_z - 0.30 * height), 1.45 * tail_r, 1.45 * tail_r),
                R((rear_x - 1.06 * tail_len, 0, body_z - 0.33 * height), 1.85 * tail_r,
                  1.70 * tail_r),
                R((rear_x - 1.14 * tail_len, 0, body_z - 0.37 * height), 1.25 * tail_r,
                  1.15 * tail_r),
                R((rear_x - 1.21 * tail_len, 0, body_z - 0.40 * height), 0.25 * tail_r,
                  0.25 * tail_r),
            ]
        else:
            tuft_rings = [
                R((rear_x - tail_len, 0, body_z - 0.30 * height), 1.5 * tail_r, 1.5 * tail_r),
                R((rear_x - 1.08 * tail_len, 0, body_z - 0.34 * height), 2.0 * tail_r,
                  1.6 * tail_r),
                R((rear_x - 1.15 * tail_len, 0, body_z - 0.36 * height), 0.5 * tail_r,
                  0.5 * tail_r),
            ]
        parts.append(Part("tail_tuft", tuft_rings, weights={"tail4": 1.0}, tag="tuft",
                          segments=16 if wolf else 12,
                          rough=(0.014 * height / 0.8, 32.0 * 0.8 / height) if wolf
                          else (0.012, 28.0)))

    def paint(ctx):
        P = ctx.P
        if tiger:
            orange = np.array([0.78, 0.30, 0.055])
            white = np.array([0.90, 0.84, 0.72])
            belly = nz.smoothstep(0.23, -0.70, ctx.N[..., 2]) * nz.smoothstep(0.70, 0.40, ctx.z)
            base = nz.mix(orange, white, belly * 0.92)
            phase = P[..., 0] * 27.0 + 0.9 * np.sin(P[..., 1] * 10.0 + P[..., 2] * 7.0)
            phase += 0.35 * np.sin(P[..., 2] * 16.0 + P[..., 0] * 4.0)
            phase += 0.9 * (nz.fbm(P * np.array([2.0, 8.0, 8.0]), 3.0, 3, seed=31) - 0.5)
            stripe = (np.sin(phase) > 0.78).astype(float)
            stripe *= np.clip(ctx.tag("body") + ctx.tag("leg") * 0.9 + ctx.tag("tail") * 0.5, 0, 1)
            stripe *= 1.0 - belly * 0.8
            inner_legs = ctx.tag("leg") * nz.smoothstep(0.55 * height, 0.25 * height, ctx.z)
            inner_legs *= nz.smoothstep(0.45, 0.85, -ctx.N[..., 1] * np.sign(ctx.y))
            stripe *= 1.0 - 0.8 * inner_legs
            coat = nz.mix(base, np.array([0.075, 0.045, 0.032]), stripe)
            ring = np.sin(P[..., 0] * 45.0 + P[..., 2] * 6.0) > 0.35
            coat = nz.mix(coat, np.array([0.045, 0.03, 0.025]), ring * ctx.tag("tail"))
            coat = nz.mix(coat, white, ctx.tag("jaw") * 0.85)
        elif zebra:
            phase = P[..., 0] * 18.0 + 0.75 * np.sin(P[..., 2] * 11.0)
            phase += np.where(P[..., 0] < -0.15, P[..., 2] * 13.0, 0.0)
            phase += np.where(P[..., 2] < 0.34, P[..., 0] * 18.0, 0.0)
            stripe = (np.sin(phase) > 0.12).astype(float)
            stripe *= np.clip(1.0 - ctx.tag("hoof") - ctx.tag("nose") * 0.8, 0, 1)
            base = np.where(stripe[..., None] > 0, np.array([0.025, 0.025, 0.023]),
                            np.array([0.88, 0.86, 0.80]))
            mane_stripe = (np.sin(P[..., 0] * 38.0 + P[..., 2] * 8.0) > 0).astype(float)
            mane = ctx.tag("mane")
            base = nz.mix(base, np.array([0.025, 0.025, 0.023]), mane)
            base = nz.mix(base, np.array([0.88, 0.86, 0.80]), mane * mane_stripe)
            coat = base
            coat = nz.mix(coat, np.array([0.055, 0.04, 0.035]), ctx.tag("hoof") * 0.9)
            coat = nz.mix(coat, np.array([0.08, 0.055, 0.045]), ctx.tag("nose"))
            coat = nz.mix(coat, np.array([0.035, 0.03, 0.028]), ctx.tag("jaw") * 0.7)
        elif wolf and coat_variant == "african_wild_dog":
            # Painted dogs are individuals in motley rather than grey wolves with spots. Broad,
            # low-frequency masks make readable black, ochre and white patches at zoo distance;
            # the ears and muzzle remain characteristically dark and the tail ends in white.
            ochre = np.array([0.61, 0.34, 0.13])
            black = np.array([0.055, 0.045, 0.035])
            white = np.array([0.82, 0.78, 0.65])
            grain = nz.fbm(P, 22.0, 3, seed=203)
            broad = nz.fbm(P * np.array([1.0, 1.25, 0.9]), 3.8, 4, seed=204)
            pale = nz.fbm(P * np.array([1.2, 0.9, 1.1]), 4.6, 4, seed=205)
            coat = ochre * (0.88 + 0.20 * grain)[..., None]
            coat = nz.mix(coat, black, nz.smoothstep(0.43, 0.58, broad))
            coat = nz.mix(coat, white, nz.smoothstep(0.51, 0.66, pale))
            coat = nz.mix(coat, black, np.clip(ctx.tag("ear") * 0.95 + ctx.tag("nose"), 0, 1))
            coat = nz.mix(coat, white, ctx.tag("tuft") * 0.82)
        elif wolf and coat_variant == "dingo":
            ginger = np.array([0.72, 0.38, 0.12])
            cream = np.array([0.88, 0.78, 0.59])
            grain = nz.fbm(P, 24.0, 3, seed=211)
            coat = ginger * (0.90 + 0.18 * grain)[..., None]
            underside = nz.smoothstep(0.08, -0.58, ctx.N[..., 2])
            socks = ctx.tag("leg") * nz.smoothstep(0.30 * height, 0.10 * height, ctx.z)
            coat = nz.mix(coat, cream, np.clip(underside * 0.72 + socks * 0.82 +
                                               ctx.tag("jaw") * 0.75, 0, 0.92))
            coat = nz.mix(coat, np.array([0.16, 0.10, 0.065]), ctx.tag("nose") * 0.98)
        elif wolf and coat_variant == "dhole":
            rust = np.array([0.67, 0.22, 0.075])
            cream = np.array([0.86, 0.72, 0.50])
            dark = np.array([0.12, 0.075, 0.045])
            grain = nz.fbm(P, 26.0, 3, seed=221)
            coat = rust * (0.88 + 0.20 * grain)[..., None]
            underside = nz.smoothstep(0.15, -0.58, ctx.N[..., 2])
            coat = nz.mix(coat, cream, np.clip(underside * 0.72 + ctx.tag("jaw") * 0.62, 0, 0.86))
            coat = nz.mix(coat, dark, np.clip(ctx.tag("tuft") * 0.82 + ctx.tag("nose"), 0, 0.98))
        elif wolf:
            juvenile = config.get("juvenile", False)
            grey = np.array([0.44, 0.40, 0.34]) if juvenile else np.array([0.40, 0.36, 0.30])
            pale = np.array([0.80, 0.75, 0.65]) if juvenile else np.array([0.76, 0.70, 0.59])
            dark = np.array([0.25, 0.23, 0.20]) if juvenile else np.array([0.20, 0.19, 0.17])
            noise = nz.fbm(P, 19.0, 4, seed=18)
            saddle = nz.smoothstep(0.32, 0.72, ctx.N[..., 2])
            saddle *= nz.smoothstep(0.30, 0.48, ctx.z)
            base = nz.mix(pale, grey, 0.45 + 0.25 * noise)
            base = nz.mix(base, dark, saddle * 0.60)
            underside = nz.smoothstep(0.15, -0.65, ctx.N[..., 2])
            legs = ctx.tag("leg")
            base = nz.mix(base, pale, np.clip(underside * 0.65 + legs * 0.32, 0, 0.85))
            coat = base * (0.86 + 0.25 * noise)[..., None]
            coat = nz.mix(coat, np.array([0.22, 0.16, 0.12]), ctx.tag("nose") * 0.95)
        elif kind == "brown_bear":
            brown = np.array([0.34, 0.19, 0.095])
            dark = np.array([0.16, 0.10, 0.06])
            grain = nz.fbm(P, 24.0, 4, seed=23)
            coat = brown * (0.83 + 0.27 * grain)[..., None]
            coat = nz.mix(coat, np.array([0.22, 0.13, 0.075]), ctx.tag("hump") * 0.5)
            coat = nz.mix(coat, dark, ctx.tag("leg") * 0.5 + ctx.tag("claw") * 0.8)
            coat = nz.mix(coat, np.array([0.11, 0.075, 0.055]), ctx.tag("nose"))
        else:
            cream = np.array([0.88, 0.84, 0.73])
            yellow = np.array([0.83, 0.78, 0.64])
            warmth = nz.smoothstep(0.55, -0.5, ctx.N[..., 2]) * 0.18
            coat = nz.mix(cream, yellow, warmth)
            coat = nz.mix(coat, np.array([0.045, 0.035, 0.03]), ctx.tag("nose") * 0.98)
            coat = nz.mix(coat, np.array([0.12, 0.085, 0.075]), ctx.tag("jaw") * 0.5)
        eye_col = np.array([0.18, 0.11, 0.045]) if not zebra else np.array([0.06, 0.05, 0.04])
        coat = nz.mix(coat, eye_col, np.clip(ctx.tag("eye") * 3.0, 0, 1))
        if tiger:
            coat = nz.mix(coat, np.array([0.09, 0.05, 0.035]), ctx.tag("ear") * 0.75)
            cheek = nz.smoothstep(nose_x - 0.20 * length, nose_x - 0.14 * length, P[..., 0])
            cheek *= nz.smoothstep(0.065 * height, 0.12 * height, np.abs(P[..., 1]))
            cheek *= nz.smoothstep(head_z - 0.05 * height, head_z + 0.04 * height, P[..., 2])
            brow = nz.smoothstep(nose_x - 0.18 * length, nose_x - 0.12 * length, P[..., 0])
            brow *= nz.smoothstep(head_z + 0.035 * height, head_z + 0.075 * height, P[..., 2])
            coat = nz.mix(coat, white, np.clip(cheek * 0.82 + brow * 0.72, 0, 1))
            coat = nz.mix(coat, np.array([0.55, 0.37, 0.20]), ctx.tag("ruff") * 0.6)
            ear_spot = ctx.tag("ear") * nz.smoothstep(0.0, 0.75, -ctx.N[..., 0])
            ear_spot *= nz.smoothstep(head_z + 0.04 * height, head_z + 0.09 * height, ctx.z)
            coat = nz.mix(coat, white, ear_spot)
        if bear:
            coat = nz.mix(coat, np.array([0.12, 0.075, 0.045]), ctx.tag("claw"))
        return coat

    def surface_height(ctx):
        # Anisotropic three-dimensional noise makes short guard hairs run predominantly along
        # the body. Eyes and the wet nose stay smooth instead of inheriting fur relief.
        P = ctx.P * np.array([0.42, 1.25, 1.8])
        fine = nz.fbm(P, 110.0, 3, seed=118)
        clumps = nz.fbm(ctx.P, 28.0, 3, seed=119)
        fur = 0.90 * fine + 0.10 * clumps
        smooth = np.clip(ctx.tag("eye") * 4.0 + ctx.tag("nose") * 1.5, 0.0, 1.0)
        return 0.5 + (fur - 0.5) * (1.0 - smooth)

    def front(side):
        return Leg("upper_arm" + side, "forearm" + side, "hand" + side,
                   "fingers" + side, "scapula" + side)

    def hind(side):
        return Leg("thigh" + side, "shin" + side, "foot" + side, "toes" + side)

    rig = Rig(body="hips", legs={"FL": front(".L"), "FR": front(".R"),
                                  "HL": hind(".L"), "HR": hind(".R")},
              spine=["spine1", "spine2"], chest="chest", neck=["neck1", "neck2"],
              head="head", jaw="jaw", tail=["tail1", "tail2", "tail3", "tail4"],
              ears=["ear.L", "ear.R"])

    def clips(solver):
        walk_speed = config["walk_speed"]
        trot_speed = config["trot_speed"]
        walk_period = config.get("walk_period", 1.15)
        walk_duty = 0.64 if not bear else 0.69
        trot_period = config.get("trot_period", 0.62)
        trot_duty = 0.30 if wolf else 0.42
        walk_offsets = (quad.PACE if config.get("walk_pattern") == "pace" else
                        quad.PACE if bear else quad.WALK)
        walk = quad.Gait("walk", period=walk_period,
                         stance=walk_speed * walk_period * walk_duty, duty=walk_duty,
                         offsets=walk_offsets, lift=config.get("walk_lift", 0.08), bob=0.012,
                         hip_yaw=4.0, chest_yaw=3.0, tail_sway=7.0,
                         drop=config.get("walk_drop"))
        fast = quad.Gait("trot", period=trot_period,
                         stance=trot_speed * trot_period * trot_duty, duty=trot_duty,
                         offsets=(quad.PACE if config.get("fast_pattern") == "pace" else
                                  quad.WALK if config.get("fast_pattern") == "walk" else
                                  quad.GALLOP if config.get("fast_pattern") == "gallop" else
                                  quad.TROT),
                         lift=config.get("fast_lift", 0.12), bob=-0.016,
                         hip_yaw=3.0, chest_yaw=2.0, tail_sway=5.0,
                         swing_pitch=config.get("fast_swing_pitch", 54.0),
                         drop=config.get("fast_drop"))
        head_down = quad.HeadDown(body_pitch=20.0, crouch=0.09, chest=13.0,
                                  neck=(48.0, 40.0), head=20.0)
        if wolf:
            head_down = quad.HeadDown(body_pitch=24.0, crouch=0.12, chest=17.0,
                                      neck=(58.0, 48.0), head=25.0)
        if kind == "brown_bear":
            head_down = quad.HeadDown(body_pitch=35.0, crouch=0.0, chest=35.0,
                                      neck=(100.0, 85.0), head=50.0)
        if kind == "polar_bear":
            head_down = quad.HeadDown(body_pitch=35.0, crouch=0.0, chest=35.0,
                                      neck=(100.0, 85.0), head=50.0)
        if zebra:
            head_down = quad.HeadDown(body_pitch=30.0, crouch=0.0, chest=10.0,
                                      neck=(25.0, 15.0), head=5.0)
        head_down = config.get("head_down") or head_down
        params = quad.Params(gaits=[walk, fast],
                             lie=quad.Lie(drop=config.get("lie_drop", 0.42 if bear else 0.48),
                                          hind_tuck=config.get("hind_tuck", 0.10),
                                          hind_out=config.get("hind_out", 0.07)),
                             head_down=head_down,
                             sleep_neck=(42.0, 36.0) if wolf or kind == "polar_bear"
                             else config.get("sleep_neck", (26.0, 26.0)),
                             sleep_head=21.0 if wolf or kind == "polar_bear"
                             else config.get("sleep_head", 14.0),
                             drink_height=config.get("drink_height",
                                                     0.06 if bear or zebra else 0.03),
                             look_yaw=16.0, tail_idle=8.0,
                             idle_shift=config.get("idle_shift", .012),
                             idle_bob=config.get("idle_bob", .004),
                             idle_roll=config.get("idle_roll", .7),
                             call_neck=config.get("call_neck", -9.0),
                             call_head=config.get("call_head", -14.0))
        return quad.standard_clips(solver, params)

    meta = {"common_name": config["common_name"],
            "scientific_name": config["scientific_name"],
            "shoulder_height_m": height,
            "body_length_m": length,
            "roughness": 0.9}
    if wolf:
        meta["surface_detail"] = "anisotropic double-coat normal map"
    meta.update(config.get("meta", {}))
    return Spec(name=name, bones=bones, parts=parts, paint=paint, texture_size=1024,
                surface_height=surface_height if wolf else None,
                surface_strength=1.35 if wolf else 2.0,
                voxel=config.get("voxel", 0.010 if height < 1.1 else 0.012),
                target_tris=config.get("target_tris", 18000),
                smooth_iters=8, meta=meta,
                rig=rig, clips=clips, validate_mesh=config.get("validate_mesh", False))
