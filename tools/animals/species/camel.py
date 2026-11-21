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
FEMALE_RELATIVE_HEIGHT = .92


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
        R((-1.00, 0, 1.66), .10, .18, .16), R((-.84, 0, 1.68), .30, .32, .28),
        R((-.55, 0, 1.69), .38, .34, .30), R((-.18, 0, 1.72), .40, .36, .33),
        R((.18, 0, 1.75), .39, .38, .40), R((.48, 0, 1.76), .32, .37, .46),
        R((.69, 0, 1.76), .18, .25, .30)],
        chain=chain(spec.bones, "hips", "spine1", "spine2", "chest"), segments=28, cap_scale=.35),
        Part("neck", [R(points[0], .21, .25), R(points[1], .19, .22),
                      R(points[2], .16, .19), R(points[3], .15, .18),
                      R(points[4], .13, .16), R(points[5], .12, .145)],
             chain=chain(spec.bones, *spec.rig.neck), segments=24, cap_start="flat", cap_end="flat"),
        Part("head", [R((1.28, 0, 2.79), .105, .14, .12),
                      R((1.40, 0, 2.81), .145, .16, .14),
                      R((1.53, 0, 2.79), .15, .15, .13),
                      R((1.65, 0, 2.73), .12, .105, .11),
                      R((1.79, 0, 2.69), .10, .08, .09, n=2.5),
                      R((1.91, 0, 2.66), .105, .075, .085, n=2.7)],
             weights={"head": 1.0}, segments=22, cap_scale=.30, tag="head"),
        Part("jaw", [R((1.46, 0, 2.64), .115, .040, .060),
                     R((1.68, 0, 2.61), .095, .038, .050),
                     R((1.88, 0, 2.57), .09, .042, .055)],
             weights={"jaw": 1.0}, segments=16, tag="jaw"),
        Part("ear.L", [R((1.37, .115, 2.84), .048, .045),
                       R((1.31, .18, 2.92), .032, .048)],
             weights={"ear.L": 1.0}, mirror=True, segments=14, tag="ear"),
        Part("eye.L", [R((1.49, .144, 2.815), .026, .012),
                        R((1.56, .147, 2.805), .025, .011)],
             weights={"head": 1.0}, mirror=True, fuse=False, segments=12, tag="eye"),
        Part("nostril.L", [R((1.78, .094, 2.710), .018, .010),
                            R((1.84, .098, 2.695), .016, .009)],
             weights={"head": 1.0}, mirror=True, fuse=False, segments=8, tag="nostril",
             cap_scale=.25),
        # Bactrian camels carry a long winter mane over the crown and both sides of the neck.
        # This second fused loft changes the outline; the normal map supplies the shorter fibres.
        Part("neck_mane", [R(points[0], .26, .29), R(points[1], .23, .27),
                           R(points[2], .20, .24), R(points[3], .19, .23),
                           R(points[4], .16, .20), R(points[5], .14, .17)],
             chain=chain(spec.bones, *spec.rig.neck), segments=24, tag="mane",
             rough=(.025, 20.0), cap_start="flat", cap_end="flat"),
        Part("forelock", [R((1.30, 0, 2.83), .10, .11),
                           R((1.42, 0, 2.88), .09, .10),
                           R((1.50, 0, 2.84), .035, .045)],
             weights={"head": 1.0}, segments=18, tag="mane", rough=(.018, 24.0),
             cap_start="flat", cap_end="point"),
    ]
    # Fed Bactrian humps rise from broad bases and lean slightly apart rather than reading as
    # two identical vertical pills. Their separate lofts preserve the deep saddle between them.
    humps = {
        "rear": ([(-.60, 1.91, .27, .28), (-.64, 2.10, .32, .32),
                  (-.68, 2.30, .30, .28), (-.70, 2.48, .22, .19),
                  (-.69, 2.64, .10, .085)], "spine1"),
        "front": ([(.22, 1.94, .28, .29), (.25, 2.14, .33, .32),
                   (.24, 2.36, .30, .27), (.20, 2.54, .21, .18),
                   (.17, 2.68, .10, .085)], "chest"),
    }
    for label, (rings, bone) in humps.items():
        spec.parts.append(Part("hump." + label, [
            R((x, 0, z), width, depth) for x, z, width, depth in rings],
            weights={bone: 1.0}, segments=28, cap_scale=.45, tag="hump",
            cap_start="flat", cap_end="round"))
    for front, x, y, top in ((True, .46, .28, 1.90), (False, -.75, .28, 1.83)):
        points = ([(x, y, top + .10), (x, y, top), (x - .16, y, .94),
                   (x + .04, y, .30), (x + .06, y, .09), (x + .20, y, .05)] if front else
                  [(x, y, top), (x + .27, y, 1.03), (x - .11, y, .40),
                   (x + .02, y, .09), (x + .16, y, .05)])
        limb_chain = leg_bones(spec.bones, front, points)
        label = "front" if front else "hind"
        knee = (x - .16, .94) if front else (x + .27, 1.03)
        upper = (.10, .115, .10, .082, .052, .085) if front else \
                (.10, .11, .098, .084, .056, .085)
        spec.parts.append(Part(label + "_leg.L", [
            R((x, .14, top), upper[0], .14), R((x, .25, 1.60), upper[1], .145),
            R((x, y, 1.45), upper[2], .13),
            R((knee[0], y, knee[1]), upper[3], .105),
            R((x - .05, y, .53), upper[4], .072),
            R((x + .06, y, .045), upper[5], .072)],
            chain=limb_chain, mirror=True, segments=18, tag="leg", cap_start="flat", cap_end="flat"))
        spec.parts.extend(split_foot(label + "_pad", x + .09, y, .16, .32, .10,
                                    "fingers.L" if front else "toes.L", tag="pad", exponent=2.3))
        foot_bone = "fingers.L" if front else "toes.L"
        # The two toes spread, but the weight-bearing sole below them is a single broad pad.
        spec.parts.append(Part(label + "_sole.L", [
            R((x - .02, y, .040), .155, .032, n=2.8),
            R((x + .20, y, .036), .17, .030, n=2.8)],
            weights={foot_bone: 1.0}, mirror=True, fuse=False, segments=10, tag="pad",
            cap_scale=.18))
        for digit, offset in (("inner", -.083), ("outer", .083)):
            spec.parts.append(Part(label + "_nail." + digit + ".L", [
                R((x + .185, y + offset, .066), .031, .025, .020, n=2.4),
                R((x + .225, y + offset, .052), .018, .014, .012, n=2.4),
            ], weights={foot_bone: 1.0}, mirror=True, fuse=False, segments=10,
                tag="nail", cap_start="flat", cap_end="point"))
    tail_points = [(-.96, 0, 1.77), (-1.15, 0, 1.50), (-1.20, 0, 1.24),
                   (-1.23, 0, 1.02), (-1.26, 0, .87)]
    for i in range(4):
        set_bone(spec.bones, f"tail{i + 1}", tail_points[i], tail_points[i + 1])
    spec.parts.append(Part("tail", [R(point, .045 - .005 * i, .045 - .005 * i)
        for i, point in enumerate(tail_points)],
        chain=chain(spec.bones, "tail1", "tail2", "tail3", "tail4"), segments=12, tag="tail"))
    spec.parts.append(Part("tail_tuft", [
        R(tail_points[-1], .075, .075), R((-1.27, 0, .75), .09, .085),
        R((-1.27, 0, .63), .06, .055), R((-1.27, 0, .56), .018, .018)],
        weights={"tail4": 1.0}, segments=16, tag="mane", rough=(.018, 24.0),
        cap_start="flat", cap_end="point"))


def _paint(ctx, juvenile=False, scale=1.0):
    P = ctx.P / scale if juvenile else ctx.P
    sand = np.array([.62, .44, .27]) if juvenile else np.array([.56, .37, .20])
    pale = np.array([.82, .69, .49]) if juvenile else np.array([.77, .63, .43])
    grain = nz.fbm(P, 32.0, 3, seed=21)
    under = nz.smoothstep(.15, -.70, ctx.N[..., 2])
    coat = nz.mix(sand, pale, under * .40) * (.90 + .16 * grain)[..., None]
    coat = nz.mix(coat, np.array([.16, .10, .055]), ctx.tag("eye"))
    coat = nz.mix(coat, np.array([.13, .075, .035]), ctx.tag("nostril"))
    coat = nz.mix(coat, np.array([.32, .22, .13]), ctx.tag("pad") * .7)
    coat = nz.mix(coat, np.array([.20, .13, .075]), ctx.tag("nail"))
    # A fine vertical philtrum suggests the divided mobile upper lip.
    lip = nz.smoothstep(1.82, 1.93, P[..., 0]) * nz.smoothstep(.018, .004,
                                                               np.abs(P[..., 1]))
    coat = nz.mix(coat, np.array([.23, .14, .075]), lip * ctx.tag("head"))
    return coat


def _surface_height(ctx, juvenile=False, scale=1.0):
    """Longitudinal guard-hair relief, with the face and feet kept smooth."""
    P = ctx.P / scale if juvenile else ctx.P
    directional = nz.fbm(P * np.array([.36, .82, 1.55]), 58.0, 4, seed=31)
    clumps = nz.fbm(P, 19.0, 3, seed=32)
    coat = .84 * directional + .16 * clumps
    smooth = np.clip(ctx.tag("eye") * 4.0 + ctx.tag("nostril") * 4.0 + ctx.tag("pad") * 2.0
                     + ctx.tag("nail") * 2.0, 0.0, 1.0)
    return .5 + (coat - .5) * (1.0 - smooth)


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
        if part.name in {"neck", "head", "jaw", "ear.L", "eye.L", "nostril.L"}:
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
        if part.name in {"head", "jaw", "ear.L", "eye.L", "nostril.L"}:
            for ring in part.rings:
                shorten_face(ring.c)
        if part.name == "head":
            for ring in part.rings:
                ring.ry *= 1.20
                ring.up *= 1.18
                ring.dn *= 1.18
        elif part.name == "eye.L":
            for ring in part.rings:
                ring.c[1] *= 1.38
                ring.ry *= 1.38
                ring.up *= 1.38
                ring.dn *= 1.38
        elif part.name == "nostril.L":
            for ring in part.rings:
                ring.c[1] *= 1.45
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
        elif (("_pad." in part.name or "_sole." in part.name) and part.name.endswith(".L")):
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
    spec.surface_height = partial(_surface_height, juvenile=True, scale=scale)
    spec.voxel = .008
    spec.target_tris = 17000
    spec.meta.update({"shoulder_height_m": 1.26, "body_length_m": 1.65,
                      "variant_of": "camel", "variant": "juvenile",
                      "relative_height": scale,
                      "age_shape": "Calf: low humps, slim body, shorter face, larger eyes and pads"})


def _female_shape(spec):
    """Give the adult cow a conservative lighter build; coat colour is not sex-specific."""
    slender = {"torso": .92, "neck": .92, "neck_mane": .88,
               "head": .94, "jaw": .94, "forelock": .90}
    for part in spec.parts:
        leg = part.name.startswith(("front_leg.", "hind_leg."))
        factor = slender.get(part.name)
        if factor is None and (part.name.startswith("hump.") or leg):
            factor = .93
        if factor is None:
            continue
        for index, ring in enumerate(part.rings):
            # Keep the final ankle wide enough to overlap both broad pad halves after remeshing.
            if leg and index == len(part.rings) - 1:
                continue
            ring.ry *= factor
            ring.up *= factor
            ring.dn *= factor

    spec.name = "camel_female"
    spec.target_tris = 17000
    spec.meta.update({"common_name": "Bactrian camel cow", "sex": "female",
                      "variant_of": "camel", "variant": "female",
                      "relative_height": FEMALE_RELATIVE_HEIGHT,
                      "age_shape": "Adult cow: shorter and lighter than the adult bull"})


def make_spec(juvenile=False, female=False):
    if juvenile and female:
        raise ValueError("juvenile and female camel variants are mutually exclusive")
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
    spec.target_tris = 17000
    spec.paint = _paint
    spec.surface_height = _surface_height
    spec.surface_strength = 1.15
    spec.voxel = .014
    spec.smooth_iters = 5
    spec.meta["roughness"] = .94
    spec.meta["surface_detail"] = "anisotropic shaggy-coat normal map"
    spec.meta["anatomy"] = ("Third-pass Bactrian outline: tapered head and neck, asymmetric "
                            "humps, tucked trunk, lean limbs and continuous sole pads")
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
    elif female:
        _female_shape(spec)
    return spec
