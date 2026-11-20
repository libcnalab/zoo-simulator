# SPDX-License-Identifier: MIT
"""Bactrian camel: 2.1 m withers, about 3.0 m nose-to-rump excluding tail.

The humps rise above a level back; a five-bone U-shaped neck carries the head
above them. The limbs end in broad paired pads rather than carnivore paws.
"""
import math
from functools import partial

import numpy as np

from zoogen import activity
from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.anatomy import chain, leg_bones, set_bone, split_foot
from zoogen.geom import Bone, Part, R
from zoogen.rig import Clip
from zoogen.wild_quadruped import make_wild_quadruped


JUVENILE_SCALE = .60


def _anatomy(spec):
    points = [(.51, 0, 1.86), (.85, 0, 1.48), (1.17, 0, 1.44),
              (1.35, 0, 1.85), (1.33, 0, 2.30), (1.36, 0, 2.77)]
    for i in range(2):
        set_bone(spec.bones, f"neck{i + 1}", points[i], points[i + 1])
    for i in range(2, 5):
        spec.bones.append(Bone(f"neck{i + 1}", f"neck{i}", points[i], points[i + 1]))
    head = next(bone for bone in spec.bones if bone.name == "head")
    head.parent = "neck5"
    set_bone(spec.bones, "head", points[-1], (1.90, 0, 2.68))
    set_bone(spec.bones, "jaw", (1.48, 0, 2.62), (1.94, 0, 2.57))
    set_bone(spec.bones, "ear.L", (1.37, .12, 2.84), (1.31, .19, 2.93))
    spec.rig.neck = [f"neck{i}" for i in range(1, 6)]
    spec.rig.extra["muzzle"] = ("head", (1.96, 0, 2.66))
    spec.parts = [Part("torso", [
        R((-1.00, 0, 1.68), .12, .17, .22), R((-.85, 0, 1.68), .34, .37, .30),
        R((-.52, 0, 1.68), .43, .39, .34), R((-.12, 0, 1.68), .47, .41, .43),
        R((.29, 0, 1.68), .44, .42, .51), R((.57, 0, 1.68), .33, .39, .49),
        R((.74, 0, 1.68), .20, .28, .32)],
        chain=chain(spec.bones, "hips", "spine1", "spine2", "chest"), segments=28, cap_scale=.35),
        Part("neck", [R(points[0], .24, .28), R(points[1], .20, .24),
                      R(points[2], .17, .21), R(points[3], .16, .20),
                      R(points[4], .14, .18), R(points[5], .13, .16)],
             chain=chain(spec.bones, *spec.rig.neck), segments=24, cap_start="flat", cap_end="flat"),
        Part("head", [R((1.29, 0, 2.79), .12, .16, .13),
                      R((1.47, 0, 2.80), .16, .18, .16),
                      R((1.67, 0, 2.72), .125, .11, .12),
                      R((1.89, 0, 2.67), .095, .08, .09, n=2.7)],
             weights={"head": 1.0}, segments=22, cap_scale=.30, tag="head"),
        Part("jaw", [R((1.48, 0, 2.62), .12, .045, .065),
                     R((1.87, 0, 2.57), .085, .04, .045)],
             weights={"jaw": 1.0}, segments=16, tag="jaw"),
        Part("ear.L", [R((1.37, .12, 2.84), .055, .05), R((1.31, .19, 2.92), .04, .055)],
             weights={"ear.L": 1.0}, mirror=True, segments=14, tag="ear"),
        Part("eye.L", [R((1.55, .141, 2.81), .023, .023), R((1.57, .143, 2.81), .022, .022)],
             weights={"head": 1.0}, mirror=True, fuse=False, segments=12, tag="eye"),
    ]
    # Upright closed lofts keep each hump rounded without bridging their saddle.
    for label, x, bone in (("rear", -.62, "spine1"), ("front", .20, "chest")):
        spec.parts.append(Part("hump." + label, [
            R((x, 0, 1.93), .31, .35), R((x, 0, 2.17), .32, .34),
            R((x, 0, 2.44), .28, .29), R((x, 0, 2.63), .16, .17)],
            weights={bone: 1.0}, segments=28, cap_scale=.65, tag="hump"))
    for front, x, y, top in ((True, .46, .28, 1.90), (False, -.75, .28, 1.83)):
        points = ([(x, y, top + .10), (x, y, top), (x - .16, y, .94),
                   (x + .04, y, .30), (x + .06, y, .09), (x + .20, y, .05)] if front else
                  [(x, y, top), (x + .27, y, 1.03), (x - .11, y, .40),
                   (x + .02, y, .09), (x + .16, y, .05)])
        limb_chain = leg_bones(spec.bones, front, points)
        label = "front" if front else "hind"
        knee = (x - .16, .94) if front else (x + .27, 1.03)
        spec.parts.append(Part(label + "_leg.L", [
            R((x, .14, top), .11, .15), R((x, .25, 1.60), .13, .16),
            R((x, y, 1.45), .12, .15),
            R((knee[0], y, knee[1]), .11, .125),
            R((x - .05, y, .53), .066, .08), R((x + .06, y, .045), .085, .085)],
            chain=limb_chain, mirror=True, segments=18, tag="leg", cap_start="flat", cap_end="flat"))
        spec.parts.extend(split_foot(label + "_pad", x + .09, y, .16, .32, .10,
                                    "fingers.L" if front else "toes.L", tag="pad", exponent=2.3))
    tail_points = [(-.96, 0, 1.77), (-1.15, 0, 1.50), (-1.20, 0, 1.24),
                   (-1.23, 0, 1.02), (-1.26, 0, .87)]
    for i in range(4):
        set_bone(spec.bones, f"tail{i + 1}", tail_points[i], tail_points[i + 1])
    spec.parts.append(Part("tail", [R(point, .045 - .005 * i, .045 - .005 * i)
        for i, point in enumerate(tail_points)],
        chain=chain(spec.bones, "tail1", "tail2", "tail3", "tail4"), segments=12, tag="tail"))


def _paint(ctx, juvenile=False, scale=1.0):
    P = ctx.P / scale if juvenile else ctx.P
    sand = np.array([.62, .44, .27]) if juvenile else np.array([.56, .37, .20])
    pale = np.array([.82, .69, .49]) if juvenile else np.array([.77, .63, .43])
    grain = nz.fbm(P, 32.0, 3, seed=21)
    under = nz.smoothstep(.15, -.70, ctx.N[..., 2])
    coat = nz.mix(sand, pale, under * .40) * (.90 + .16 * grain)[..., None]
    coat = nz.mix(coat, np.array([.16, .10, .055]), ctx.tag("eye"))
    coat = nz.mix(coat, np.array([.32, .22, .13]), ctx.tag("pad") * .7)
    # A fine vertical philtrum suggests the divided mobile upper lip.
    lip = nz.smoothstep(1.82, 1.93, P[..., 0]) * nz.smoothstep(.018, .004,
                                                               np.abs(P[..., 1]))
    coat = nz.mix(coat, np.array([.23, .14, .075]), lip * ctx.tag("head"))
    return coat


def _juvenile_shape(spec):
    scale = JUVENILE_SCALE
    for bone in spec.bones:
        bone.head = np.asarray(bone.head, dtype=float) * scale
        bone.tail = np.asarray(bone.tail, dtype=float) * scale
    for part in spec.parts:
        for ring in part.rings:
            ring.c *= scale
            ring.ry *= scale
            ring.up *= scale
            ring.dn *= scale
        if part.chain is not None:
            part.chain = [(name, np.asarray(head) * scale, np.asarray(tail) * scale)
                          for name, head, tail in part.chain]
        if part.rough is not None:
            part.rough = (part.rough[0] * scale, part.rough[1] / scale)

    neck_base = np.array([.51, 0, 1.86]) * scale

    def shorten_neck(point):
        point[:] = neck_base + .88 * (point - neck_base)

    for bone in spec.bones:
        if bone.name.startswith("neck") or bone.name in {"head", "jaw", "ear.L"}:
            shorten_neck(bone.head)
            shorten_neck(bone.tail)
    for part in spec.parts:
        if part.name in {"neck", "head", "jaw", "ear.L", "eye.L"}:
            for ring in part.rings:
                shorten_neck(ring.c)

    # Shorten the face a second time while keeping the large juvenile cranium at the end of
    # the neck. Calves have only low developing humps and relatively long legs with broad pads.
    face_pivot = np.array([1.29, 0, 2.79]) * scale
    shorten_neck(face_pivot)

    def shorten_face(point):
        point[0] = face_pivot[0] + .78 * (point[0] - face_pivot[0])

    for bone in spec.bones:
        if bone.name in {"head", "jaw", "ear.L"}:
            shorten_face(bone.head)
            shorten_face(bone.tail)
        if bone.name == "ear.L":
            # Seat the calf's small ears into its enlarged skull. Besides reading more
            # naturally upright, this keeps their roots above ground at full neck flexion.
            bone.head[2] -= .03
            bone.tail[2] -= .03
    for part in spec.parts:
        if part.name in {"head", "jaw", "ear.L", "eye.L"}:
            for ring in part.rings:
                shorten_face(ring.c)
        if part.name == "head":
            for ring in part.rings:
                ring.ry *= 1.20
                ring.up *= 1.18
                ring.dn *= 1.18
        elif part.name == "eye.L":
            for ring in part.rings:
                ring.ry *= 1.38
                ring.up *= 1.38
                ring.dn *= 1.38
        elif part.name == "ear.L":
            for ring in part.rings:
                ring.c[2] -= .03
                ring.ry *= 1.22
                ring.up *= 1.22
                ring.dn *= 1.22
        elif part.name == "torso":
            for ring in part.rings:
                ring.ry *= .92
                ring.up *= .92
                ring.dn *= .92
        elif part.name.startswith("hump."):
            back = 1.68 * scale
            for ring in part.rings:
                ring.c[2] = back + .55 * (ring.c[2] - back)
                ring.ry *= .74
                ring.up *= .58
                ring.dn *= .58
        elif "_pad." in part.name and part.name.endswith(".L"):
            for ring in part.rings:
                ring.ry *= 1.18
                ring.up *= 1.10
                ring.dn *= 1.10

    bones = {bone.name: bone for bone in spec.bones}
    for part in spec.parts:
        if part.chain is not None:
            part.chain = [(name, bones[name].head.copy(), bones[name].tail.copy())
                          for name, _, _ in part.chain]
    muzzle_bone, muzzle_point = spec.rig.extra["muzzle"]
    muzzle_point = np.asarray(muzzle_point, dtype=float) * scale
    shorten_neck(muzzle_point)
    shorten_face(muzzle_point)
    spec.rig.extra["muzzle"] = (muzzle_bone, muzzle_point)

    spec.name = "camel_juvenile"
    spec.paint = partial(_paint, juvenile=True, scale=scale)
    spec.voxel = .008
    spec.target_tris = 18000
    spec.meta.update({"shoulder_height_m": 1.26, "body_length_m": 1.65,
                      "variant_of": "camel", "variant": "juvenile",
                      "relative_height": scale,
                      "age_shape": "Calf: low humps, slim body, shorter face, larger eyes and pads"})


def make_spec(juvenile=False):
    scale = JUVENILE_SCALE if juvenile else 1.0
    spec = make_wild_quadruped({
        "name": "camel", "kind": "camel", "common_name": "Bactrian camel",
        "scientific_name": "Camelus bactrianus", "height": 2.1, "length": 3.0,
        "walk_speed": .72 if juvenile else 1.1, "trot_speed": 1.55 if juvenile else 2.4,
        "walk_period": .82 if juvenile else 1.15,
        "trot_period": .48 if juvenile else .62,
        "walk_lift": .055 if juvenile else .08,
        "fast_lift": .08 if juvenile else .12,
        # The calf's shorter face needs a slightly higher water target to keep the head
        # joint above ground while the muzzle still visibly touches the water surface.
        "drink_height": .055 if juvenile else .03,
        "walk_pattern": "pace", "fast_pattern": "pace",
        "head_down": quad.HeadDown(body_pitch=12, crouch=.24 * scale, chest=8,
                                   neck=(8, 18, 32, 40, 45), head=15),
        "sleep_neck": (15, 20, 30, 35, 40), "sleep_head": 10,
        "lie_drop": .90 * scale,
        "hind_tuck": .10 * scale, "hind_out": .07 * scale,
        "idle_shift": .012 * scale, "idle_bob": .004 * scale,
    })
    _anatomy(spec)
    spec.paint = _paint
    spec.voxel = .014
    spec.smooth_iters = 5
    spec.meta["roughness"] = .94
    standard = spec.clips

    def clips(solver):
        generated = standard(solver)
        idle = next(clip for clip in generated if clip.name == "idle")

        def ruminate(u):
            angle = math.tau * u
            pose = activity.copy_pose(idle.pose(u))
            # A slow, asymmetric side-to-side grind rather than the quick vertical bites of eat.
            pose.r("jaw", ry=2.5 + 2.0 * math.sin(6 * angle), rz=4.5 * math.sin(6 * angle + .7))
            pose.r("head", ry=1.2 * math.sin(angle), rz=1.5 * math.sin(2 * angle))
            return pose

        return generated + [Clip("ruminate", 10, ruminate)]

    spec.clips = clips
    # The game's clip for the camel's still cud-chewing activity.
    spec.meta["activity_clips"] = {"ruminate": "ruminate"}
    if juvenile:
        _juvenile_shape(spec)
    return spec
