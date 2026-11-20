# SPDX-License-Identifier: MIT
"""Male red kangaroo: 1.6 m upright including the erect ears, 1.5 m head/body.

The simulation's shoulderHeightM is standing height, not the quadruped withers;
meta.shoulder_height_m uses that same 1.6 m measurement. Feet are 0.35 m long,
with a five-bone muscular support tail about 1.2 m long behind the pelvis.
"""
import math
from functools import partial

import numpy as np

from zoogen import biped, noise as nz
from zoogen.geom import Bone, Part, R, Spec
from zoogen.quadruped import Gait, bisect_effort, foot_path, muzzle
from zoogen.rig import Clip, Leg, Pose, Rig

TAIL = [(-.24, 0, .60), (-.48, 0, .40), (-.72, 0, .22),
        (-.96, 0, .09), (-1.20, 0, .045), (-1.36, 0, .030)]
CONTACT = {"L": .025, "R": .025, "FL": .040, "FR": .040}
JUVENILE_SCALE = .55
JUVENILE_TAIL = .88


def _skeleton():
    bones = [Bone("root", None, (0, 0, 0), (.2, 0, 0), False),
             Bone("hips", "root", (-.12, 0, .57), (-.01, 0, .83)),
             Bone("chest", "hips", (-.01, 0, .83), (.12, 0, 1.13)),
             Bone("neck1", "chest", (.12, 0, 1.13), (.21, 0, 1.31)),
             Bone("head", "neck1", (.21, 0, 1.31), (.50, 0, 1.345)),
             Bone("ear.L", "head", (.185, .064, 1.38), (.15, .095, 1.57))]
    legs = biped.add_leg(bones, "hips", [(-.12, .17, .57), (.17, .17, .34),
        (-.23, .17, .075), (-.12, .17, .025), (.13, .17, .022)])
    arm_names = ("upper_arm.L", "forearm.L", "hand.L", "fingers.L")
    points = [(.12, .13, 1.04), (.28, .13, .88), (.22, .13, .67),
              (.24, .13, .63), (.29, .13, .61)]
    parent = "chest"
    for name, head, tail in zip(arm_names, points, points[1:]):
        bones.append(Bone(name, parent, head, tail))
        parent = name
    for side in ("L", "R"):
        legs["F" + side] = Leg(*(name[:-1] + side for name in arm_names))
    tail = biped.add_chain(bones, "tail", "hips", TAIL)
    return bones, Rig("hips", legs, chest="chest", neck=["neck1"], head="head",
        tail=tail, ears=["ear.L", "ear.R"],
        extra={"muzzle": ("head", (.528, 0, 1.312)), "contact_height": CONTACT,
               "support_tail_tip": TAIL[-1]})


def _parts(bones):
    parts = [Part("pear_body", [R((-.16, 0, .34), .12, .14, .16),
        R((-.15, 0, .48), .22, .24, .25), R((-.11, 0, .65), .24, .26, .25),
        R((-.04, 0, .84), .18, .19, .21), R((.05, 0, 1.04), .13, .14, .16),
        R((.12, 0, 1.16), .085, .10, .11), R((.21, 0, 1.30), .067, .075, .085)],
        chain=biped.chain(bones, "hips", "chest", "neck1", "head"), segments=32, cap_scale=.4),
        Part("deer_head", [R((.17, 0, 1.335), .060, .070),
            R((.245, 0, 1.35), .090, .087, .075), R((.34, 0, 1.338), .076, .063, .055),
            R((.43, 0, 1.32), .057, .045, .035), R((.50, 0, 1.312), .050, .036, .030)],
             weights={"head": 1}, segments=24, cap_scale=.3, tag="head"),
        Part("nose", [R((.498, 0, 1.32), .043, .027), R((.528, 0, 1.312), .037, .024)],
             weights={"head": 1}, fuse=False, segments=16, tag="nose", cap_scale=.3),
        biped.eye_part((.305, .078, 1.373), .012),
        Part("ear.L", [R((.185, .064, 1.38), .024, .016),
            R((.17, .080, 1.47), .034, .020), R((.15, .095, 1.555), .025, .015),
            R((.145, .098, 1.59), .008, .006)], weights={"ear.L": 1},
             side_ref=(0, 1, 0), mirror=True, fuse=False, segments=20, tag="ear", cap_scale=.3),
        Part("hind_leg.L", [R((-.12, .12, .63), .105, .17),
            R((.04, .17, .47), .105, .16), R((.17, .17, .34), .065, .078),
            R((-.04, .17, .20), .045, .052), R((-.23, .17, .075), .029, .031)],
             chain=biped.chain(bones, "thigh.L", "shin.L", "foot.L"), mirror=True,
             segments=24, cap_start="flat", tag="limb"),
        Part("long_foot.L", [R((-.23, .17, .046), .033, .030),
            R((-.12, .17, .027), .036, .023), R((.04, .17, .024), .035, .020),
            R((.13, .17, .022), .028, .018)],
             chain=biped.chain(bones, "foot.L", "toes.L"), mirror=True,
             segments=20, tag="foot", cap_scale=.2),
        Part("forearm.L", [R((.12, .13, 1.04), .043, .051),
            R((.28, .13, .88), .030, .034), R((.22, .13, .67), .022, .024)],
             chain=biped.chain(bones, "upper_arm.L", "forearm.L", "hand.L"),
             mirror=True, segments=18, tag="limb"),
        Part("small_hand.L", [R((.22, .13, .67), .025, .022),
            R((.25, .13, .63), .028, .020), R((.29, .13, .61), .023, .017)],
             chain=biped.chain(bones, "hand.L", "fingers.L"), mirror=True,
             segments=16, tag="hand", cap_scale=.3)]
    # Kangaroo feet are not smooth paddles: digit IV carries most of the long foot, digit V is
    # smaller outside it, and the tiny syndactylous II/III pair forms an inner grooming toe.
    for name, y, length, width in (("IV", .170, .175, .020),
                                    ("V", .207, .145, .013),
                                    ("IIIII", .136, .118, .010)):
        parts.append(Part(f"hind_digit.{name}.L", [
            R((.025, y, .028), width, .015),
            R((length * .70, y, .024), width * .75, .011),
            R((length, y, .023), width * .28, .006)],
            weights={"toes.L": 1}, mirror=True, fuse=False, segments=10,
            tag="hind_digit", cap_start="flat", cap_end="point"))
    # Five narrow fingers replace each mitten. Their distal colour supplies the dark claws
    # without adding a second set of tiny overlapping meshes.
    for index, (y, length) in enumerate(zip((.093, .111, .130, .149, .167),
                                            (.048, .061, .068, .060, .045))):
        parts.append(Part(f"finger.{index}.L", [
            R((.275, y, .620), .0075, .0085),
            R((.275 + length * .72, y, .606), .0065, .0065),
            R((.275 + length, y, .600), .0025, .0030)],
            weights={"fingers.L": 1}, mirror=True, fuse=False, segments=8,
            tag="finger", cap_start="flat", cap_end="point"))
    parts.append(Part("muscular_tail", [R(point, radius, radius) for point, radius in
            zip(TAIL, (.14, .115, .080, .049, .029, .019))],
             chain=biped.chain(bones, "tail1", "tail2", "tail3", "tail4", "tail5"),
             segments=24, tag="tail", cap_scale=.25))
    return parts


def paint(ctx):
    red = np.array([.59, .28, .13])
    pale = np.array([.82, .68, .49])
    grain = nz.fbm(ctx.P * [2, 3, 5], 70, 3, seed=89)
    front = nz.smoothstep(-.03, .12, ctx.x) * nz.smoothstep(-.2, .75, ctx.N[..., 0])
    color = nz.mix(red, pale, front * .80)
    limb_pale = ctx.tag("limb") * (.18 + .30 * nz.smoothstep(.55, .25, ctx.z))
    color = nz.mix(color, pale * .92, limb_pale + ctx.tag("foot") * .50)
    muzzle_mask = ctx.tag("head") * nz.smoothstep(.36, .49, ctx.x)
    color = nz.mix(color, pale, muzzle_mask)
    cheek_height = 1.345 - .17 * (ctx.x - .31)
    cheek = ctx.tag("head") * nz.smoothstep(.075, .045, np.abs(ctx.x - .34))
    dark = cheek * nz.smoothstep(.012, .005, np.abs(ctx.z - cheek_height))
    light = cheek * nz.smoothstep(.012, .005, np.abs(ctx.z - (cheek_height - .027)))
    color = nz.mix(color, np.array([.25, .15, .095]), dark)
    color = nz.mix(color, np.array([.88, .79, .64]), light)
    inner_ear = ctx.tag("ear") * nz.smoothstep(-.1, .6, ctx.N[..., 0])
    color = nz.mix(color, np.array([.47, .28, .23]), inner_ear)
    claw = (ctx.tag("hind_digit") * nz.smoothstep(.105, .155, ctx.x)
            + ctx.tag("finger") * nz.smoothstep(.295, .335, ctx.x))
    color = nz.mix(color, np.array([.12, .075, .045]), np.clip(claw, 0, 1))
    color *= (.92 + .13 * grain)[..., None]
    color = nz.mix(color, np.array([.045, .032, .022]), ctx.tag("eye") + ctx.tag("nose"))
    return color


def paint_juvenile(ctx):
    # Evaluate markings in adult coordinates so their scale and placement remain coherent.
    P = ctx.P / JUVENILE_SCALE
    grey = np.array([.48, .36, .27])
    pale = np.array([.78, .68, .54])
    grain = nz.fbm(P * [2, 3, 5], 70, 3, seed=89)
    front = nz.smoothstep(-.03, .12, P[..., 0]) * nz.smoothstep(-.2, .75, ctx.N[..., 0])
    color = nz.mix(grey, pale, front * .72)
    limb_pale = ctx.tag("limb") * (.18 + .30 * nz.smoothstep(.55, .25, P[..., 2]))
    color = nz.mix(color, pale * .94, limb_pale + ctx.tag("foot") * .50)
    muzzle_mask = ctx.tag("head") * nz.smoothstep(.36, .49, P[..., 0])
    color = nz.mix(color, pale, muzzle_mask)
    cheek_height = 1.345 - .17 * (P[..., 0] - .31)
    cheek = ctx.tag("head") * nz.smoothstep(.075, .045, np.abs(P[..., 0] - .34))
    dark = cheek * nz.smoothstep(.012, .005, np.abs(P[..., 2] - cheek_height))
    light = cheek * nz.smoothstep(.012, .005, np.abs(P[..., 2] - (cheek_height - .027)))
    color = nz.mix(color, np.array([.27, .20, .16]), dark)
    color = nz.mix(color, np.array([.88, .80, .68]), light)
    inner_ear = ctx.tag("ear") * nz.smoothstep(-.1, .6, ctx.N[..., 0])
    color = nz.mix(color, np.array([.48, .31, .29]), inner_ear)
    claw = (ctx.tag("hind_digit") * nz.smoothstep(.105, .155, P[..., 0])
            + ctx.tag("finger") * nz.smoothstep(.295, .335, P[..., 0]))
    color = nz.mix(color, np.array([.13, .09, .065]), np.clip(claw, 0, 1))
    color *= (.92 + .13 * grain)[..., None]
    color = nz.mix(color, np.array([.045, .032, .022]), ctx.tag("eye") + ctx.tag("nose"))
    return color


def surface_height(ctx, scale=1.0):
    """Dense short-fur relief, excluding the nose, eyes and smooth distal digits."""
    P = ctx.P / scale
    directional = nz.fbm(P * np.array([.42, 1.15, 1.90]), 112.0, 3, seed=90)
    clumps = nz.fbm(P, 37.0, 3, seed=91)
    coat = .86 * directional + .14 * clumps
    smooth = np.clip(ctx.tag("eye") * 4.0 + ctx.tag("nose") * 3.0
                     + ctx.tag("hind_digit") * 2.0 + ctx.tag("finger") * 2.0, 0.0, 1.0)
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

    # A young-at-foot joey has a shorter muzzle, large sensory features and feet, and has not
    # yet developed the buck's massive thighs and tail. Keep the long hind-limb proportions.
    face_pivot = np.array([.17, 0, 1.335]) * scale

    def shorten_face(point):
        point[0] = face_pivot[0] + .72 * (point[0] - face_pivot[0])

    head = next(bone for bone in spec.bones if bone.name == "head")
    shorten_face(head.tail)
    for part in spec.parts:
        if part.name in {"deer_head", "nose", "eye.L"}:
            for ring in part.rings:
                shorten_face(ring.c)
        if part.name == "deer_head":
            for ring in part.rings:
                ring.ry *= 1.25
                ring.up *= 1.23
                ring.dn *= 1.23
        elif part.name == "eye.L":
            for ring in part.rings:
                ring.ry *= 1.45
                ring.up *= 1.45
                ring.dn *= 1.45
        elif part.name == "ear.L":
            ear_base = np.array([.185, .064, 1.38]) * scale
            for ring in part.rings:
                ring.c[:] = ear_base + 1.12 * (ring.c - ear_base)
                ring.ry *= 1.18
                ring.up *= 1.18
                ring.dn *= 1.18
        elif part.name == "pear_body":
            for ring in part.rings:
                ring.ry *= .88
                ring.up *= .90
                ring.dn *= .90
        elif part.name == "hind_leg.L":
            for ring in part.rings:
                ring.ry *= .86
                ring.up *= .86
                ring.dn *= .86
        elif part.name == "long_foot.L":
            for ring in part.rings:
                ring.ry *= 1.22
                ring.up *= 1.16
                ring.dn *= 1.16
        elif part.name == "forearm.L":
            for ring in part.rings:
                ring.ry *= .88
                ring.up *= .88
                ring.dn *= .88
        elif part.name == "small_hand.L":
            for ring in part.rings:
                ring.ry *= 1.15
                ring.up *= 1.15
                ring.dn *= 1.15
        elif part.name.startswith("hind_digit.") or part.name.startswith("finger."):
            root = part.rings[0].c.copy()
            for ring in part.rings:
                ring.c[:] = root + 1.15 * (ring.c - root)
                ring.ry *= 1.15
                ring.up *= 1.15
                ring.dn *= 1.15

    ear_base = np.array([.185, .064, 1.38]) * scale
    ear = next(bone for bone in spec.bones if bone.name == "ear.L")
    ear.tail[:] = ear_base + 1.12 * (ear.tail - ear_base)

    tail_base = np.asarray(TAIL[0], dtype=float) * scale

    def shorten_tail(point):
        point[:] = tail_base + JUVENILE_TAIL * (point - tail_base)

    for bone in spec.bones:
        if bone.name.startswith("tail"):
            shorten_tail(bone.head)
            shorten_tail(bone.tail)
    tail_part = next(part for part in spec.parts if part.name == "muscular_tail")
    for ring in tail_part.rings:
        shorten_tail(ring.c)
        ring.ry *= .72
        ring.up *= .72
        ring.dn *= .72

    bones = {bone.name: bone for bone in spec.bones}
    for part in spec.parts:
        if part.chain is not None:
            part.chain = [(name, bones[name].head.copy(), bones[name].tail.copy())
                          for name, _, _ in part.chain]

    muzzle_bone, muzzle_point = spec.rig.extra["muzzle"]
    muzzle_point = np.asarray(muzzle_point, dtype=float) * scale
    shorten_face(muzzle_point)
    spec.rig.extra["muzzle"] = (muzzle_bone, muzzle_point)
    tip = np.asarray(spec.rig.extra["support_tail_tip"], dtype=float) * scale
    shorten_tail(tip)
    spec.rig.extra["support_tail_tip"] = tip
    spec.rig.extra["contact_height"] = {
        key: height * scale for key, height in spec.rig.extra["contact_height"].items()}

    spec.name = "kangaroo_juvenile"
    spec.paint = paint_juvenile
    spec.surface_height = partial(surface_height, scale=scale)
    spec.voxel = .005
    spec.target_tris = 14000
    spec.meta.update({"shoulder_height_m": .89, "body_length_m": .82,
                      "variant_of": "kangaroo", "variant": "juvenile",
                      "relative_height": scale,
                      "age_shape": "Joey at foot: large head, eyes, ears and feet; slim thighs and tail",
                      "sex": "juvenile"})


def _stand(solver):
    pose = Pose()
    for key in ("L", "R"):
        pose.plant(key, solver.toe_base(key))
    return pose


def _crawl(solver, phase):
    pose = Pose().move("hips", dz=-.245).r("hips", ry=55)
    pose.r("neck1", ry=-15)
    pose.r("head", ry=-35)
    for key in solver.rig.legs:
        front = key.startswith("F")
        duty, offset = (.64, .38) if front else (.42, 0)
        path = Gait("walk", .6, .6 * duty, duty, {}, lift=.10 if front else .07,
                    heel_off=0, swing_pitch=0, land_pitch=0)
        dx, dz, _, _ = foot_path(path, (phase - offset) % 1)
        y = solver.toe_base(key)[1]
        position = [(.49 if front else -.04) + dx, y, CONTACT[key] + dz]
        pose.plant(key, position, pole=(-1, 0, .2) if front else (1, 0, .25))
    tail_path = Gait("walk", .6, .6 * .64, .64, {}, lift=.10, heel_off=0, swing_pitch=0, land_pitch=0)
    dx, dz, _, _ = foot_path(tail_path, (phase - .38) % 1)
    biped.target_chain(solver, pose, solver.rig.tail, [-1.10 + dx, 0, .030 + dz])
    return pose


def _hop(solver, phase):
    duty = .4
    flight = max(0, (phase - duty) / (1 - duty))
    rise = .13 * math.sin(math.pi * flight) ** 2
    pose = Pose().move("hips", dz=rise - .04 - .025 * math.cos(math.tau * phase)).r("hips", ry=25)
    pose.r("neck1", ry=-10)
    pose.r("head", ry=-10)
    path = Gait("trot", .5, 2.8 * .5 * duty, duty, {}, lift=.18,
                heel_off=0, swing_pitch=0, land_pitch=0)
    dx, dz, _, _ = foot_path(path, phase)
    for key in ("L", "R"):
        pose.plant(key, solver.toe_base(key) + [dx, 0, dz])
    for index, bone in enumerate(solver.rig.tail):
        pose.r(bone, ry=3 * math.sin(math.tau * phase) / (index + 1))
    return pose


def _lying(solver, sleeping):
    # Tip the upright spine forward, then roll the belly sideways around that spine.
    pose = Pose().move("hips", dz=-.30).r("hips", ry=76).r("hips", rz=90)
    for key in solver.rig.legs:
        side = 1 if key.endswith("L") else -1
        if key == "FR":
            pose.plant(key, (.53, .36, .04), pole=(.1, .45, -1))
        elif key == "FL":
            pose.plant(key, (.55, .25, .22), pole=(-1, 0, .5))
        else:
            pose.plant(key, (.02, side * .26, .025), pole=(1, 0, .4))
    biped.sagittal_profile(solver, pose, solver.rig.tail, [-158, -166, -173, -180, -180])
    biped.sagittal_profile(solver, pose, ["neck1"], [-40 if sleeping else 35])
    biped.sagittal_profile(solver, pose, ["head"], [-42 if sleeping else 5])
    return pose


def _clips(solver):
    def crawl(phase):
        return _crawl(solver, phase)
    def hop(phase):
        return _hop(solver, phase)
    walk = Clip("walk", .6, crawl, speed=1, events=[(0, "step"), (0, "step"), (.38, "step"), (.38, "step")])
    trot = Clip("trot", .5, hop, speed=2.8, events=[(0, "step"), (0, "step")])
    trot.contact_keys = ("L", "R")
    clips = [walk, trot]
    base_feed = _crawl(solver, .40)
    def feeding(effort):
        pose = biped.copy_pose(base_feed)
        pose.r("chest", ry=10 * effort)
        pose.r("neck1", ry=55 * effort)
        pose.r("head", ry=65 * effort)
        biped.sagittal_profile(solver, pose, solver.rig.tail, [-145, -155, -165, -175, -180])
        return pose
    feeding_poses = {}
    for name, height in (("eat", .075), ("drink", .035)):
        effort = bisect_effort(lambda value: muzzle(solver, feeding(value))[2], height, name)
        feeding_poses[name] = feeding(effort)
    postures = {"rest": _lying(solver, False), "sleep": _lying(solver, True), **feeding_poses}
    for name in ("idle", "eat", "drink", "rest", "sleep", "vocalize"):
        def pose(phase, clip_name=name):
            angle = math.tau * phase
            result = biped.copy_pose(postures[clip_name]) if clip_name in postures else _stand(solver)
            if clip_name == "idle":
                result.r("head", rz=12 * math.sin(angle))
                for side, sign in (("L", 1), ("R", -1)):
                    result.r("ear." + side, rz=sign * 16 * math.sin(angle + sign))
            elif clip_name == "vocalize":
                result.r("chest", ry=-8)
                result.r("head", ry=-15 + 2 * math.sin(2 * angle))
            elif clip_name in ("eat", "drink"):
                result.r("head", rz=2 * math.sin(angle))
            return result
        clips.append(Clip(name, 5, pose))
    return clips


def _juvenile_crawl(solver, phase):
    scale = JUVENILE_SCALE
    period, speed = .52, .58
    heights = solver.rig.extra["contact_height"]
    pose = Pose().move("hips", dz=-.245 * scale).r("hips", ry=55)
    pose.r("neck1", ry=-15)
    pose.r("head", ry=-35)
    for key in solver.rig.legs:
        front = key.startswith("F")
        duty, offset = (.64, .38) if front else (.42, 0)
        path = Gait("walk", period, speed * period * duty, duty, {},
                    lift=(.10 if front else .07) * scale,
                    heel_off=0, swing_pitch=0, land_pitch=0)
        dx, dz, _, _ = foot_path(path, (phase - offset) % 1)
        y = solver.toe_base(key)[1]
        position = [(.49 if front else -.04) * scale + dx, y, heights[key] + dz]
        pose.plant(key, position, pole=(-1, 0, .2) if front else (1, 0, .25))
    duty = .64
    tail_path = Gait("walk", period, speed * period * duty, duty, {},
                     lift=.10 * scale, heel_off=0, swing_pitch=0, land_pitch=0)
    dx, dz, _, _ = foot_path(tail_path, (phase - .38) % 1)
    biped.target_chain(solver, pose, solver.rig.tail,
                       [-1.10 * scale * JUVENILE_TAIL + dx, 0, .030 * scale + dz])
    return pose


def _juvenile_hop(solver, phase):
    scale = JUVENILE_SCALE
    period, speed, duty = .48, 2.2, .4
    flight = max(0, (phase - duty) / (1 - duty))
    rise = .13 * scale * math.sin(math.pi * flight) ** 2
    pose = Pose().move("hips", dz=rise - .04 * scale - .025 * scale * math.cos(math.tau * phase))
    pose.r("hips", ry=25).r("neck1", ry=-10).r("head", ry=-10)
    path = Gait("trot", period, speed * period * duty, duty, {}, lift=.18 * scale,
                heel_off=0, swing_pitch=0, land_pitch=0)
    dx, dz, _, _ = foot_path(path, phase)
    for key in ("L", "R"):
        pose.plant(key, solver.toe_base(key) + [dx, 0, dz])
    for index, bone in enumerate(solver.rig.tail):
        pose.r(bone, ry=3 * math.sin(math.tau * phase) / (index + 1))
    return pose


def _juvenile_lying(solver, sleeping):
    scale = JUVENILE_SCALE
    pose = Pose().move("hips", dz=-.30 * scale).r("hips", ry=76).r("hips", rz=90)
    for key in solver.rig.legs:
        side = 1 if key.endswith("L") else -1
        if key == "FR":
            pose.plant(key, np.array((.53, .36, .04)) * scale, pole=(.1, .45, -1))
        elif key == "FL":
            pose.plant(key, np.array((.55, .25, .22)) * scale, pole=(-1, 0, .5))
        else:
            pose.plant(key, np.array((.02, side * .26, .025)) * scale, pole=(1, 0, .4))
    biped.sagittal_profile(solver, pose, solver.rig.tail, [-158, -166, -173, -180, -180])
    biped.sagittal_profile(solver, pose, ["neck1"], [-40 if sleeping else 35])
    biped.sagittal_profile(solver, pose, ["head"], [-42 if sleeping else 5])
    return pose


def _juvenile_clips(solver):
    scale = JUVENILE_SCALE
    walk = Clip("walk", .52, lambda phase: _juvenile_crawl(solver, phase), speed=.58,
                events=[(0, "step"), (0, "step"), (.38, "step"), (.38, "step")])
    trot = Clip("trot", .48, lambda phase: _juvenile_hop(solver, phase), speed=2.2,
                events=[(0, "step"), (0, "step")])
    trot.contact_keys = ("L", "R")
    clips = [walk, trot]
    base_feed = _juvenile_crawl(solver, .40)

    def feeding(effort):
        pose = biped.copy_pose(base_feed)
        pose.r("chest", ry=10 * effort)
        pose.r("neck1", ry=55 * effort)
        pose.r("head", ry=65 * effort)
        biped.sagittal_profile(solver, pose, solver.rig.tail, [-145, -155, -165, -175, -180])
        return pose

    feeding_poses = {}
    # The shorter joey muzzle meets low forage and the water surface without forcing the
    # head joint below ground; these remain below the shared 6 cm drink-contact limit.
    for name, height in (("eat", .058), ("drink", .055)):
        effort = bisect_effort(lambda value: muzzle(solver, feeding(value))[2], height, name)
        feeding_poses[name] = feeding(effort)
    postures = {"rest": _juvenile_lying(solver, False),
                "sleep": _juvenile_lying(solver, True), **feeding_poses}
    for name in ("idle", "eat", "drink", "rest", "sleep", "vocalize"):
        def pose(phase, clip_name=name):
            angle = math.tau * phase
            result = biped.copy_pose(postures[clip_name]) if clip_name in postures else _stand(solver)
            if clip_name == "idle":
                result.r("head", rz=12 * math.sin(angle))
                for side, sign in (("L", 1), ("R", -1)):
                    result.r("ear." + side, rz=sign * 16 * math.sin(angle + sign))
            elif clip_name == "vocalize":
                result.r("chest", ry=-8)
                result.r("head", ry=-15 + 2 * math.sin(2 * angle))
            elif clip_name in ("eat", "drink"):
                result.r("head", rz=2 * math.sin(angle))
            return result
        clips.append(Clip(name, 5, pose))
    return clips


def make_spec(juvenile=False):
    bones, rig = _skeleton()
    spec = Spec("kangaroo", bones, _parts(bones), paint=paint,
                surface_height=surface_height, surface_strength=1.0, voxel=.007,
                target_tris=14500, smooth_iters=4, rig=rig,
                clips=_juvenile_clips if juvenile else _clips, key_all_locations=True,
                meta={"common_name": "Red kangaroo", "scientific_name": "Osphranter rufus",
                      "shoulder_height_m": 1.6, "body_length_m": 1.5,
                      "sex": "male", "roughness": .9,
                      "surface_detail": "anisotropic short-fur normal map"})
    if juvenile:
        _juvenile_shape(spec)
    return spec
