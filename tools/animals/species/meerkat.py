# SPDX-License-Identifier: MIT
"""Adult meerkat: 0.25 m at the shoulder and 0.35 m body length, tail excluded."""
import math
from functools import partial

import numpy as np

from zoogen import activity
from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Part, R
from zoogen.rig import Clip, Pose
from zoogen.anatomy import chain, leg_bones
from zoogen.wild_quadruped import make_wild_quadruped


TAIL_TIP = np.array([-.332, 0, .062])
JUVENILE_SCALE = .68


def _tail_prop(solver, body_pose, low=60.0, high=140.0, floor=.003, tail_tip=TAIL_TIP):
    """The tail1 pitch at which the tail tip rests on the ground under a still upright body:
    the third leg of the tripod a standing meerkat leans on."""
    for _ in range(22):
        mid = (low + high) * .5
        body_pose.rot.pop("tail1", None)
        body_pose.r("tail1", ry=mid)
        height = solver.point(solver.solve(body_pose), "tail4", tail_tip)[2]
        if height < floor:
            low = mid
        else:
            high = mid
    body_pose.rot.pop("tail1", None)
    return (low + high) * .5


# Sentry duty: where the guard looks (head yaw, head pitch: - up), in quick turns and long
# holds, one sky check for raptors; the chest follows a third of each turn.
SENTRY_LOOKS = [(0.00, (0.0, 0.0)), (0.10, (48.0, -4.0), .04), (0.30, (12.0, 2.0), .04),
                (0.40, (-50.0, -2.0), .05), (0.58, (-20.0, -32.0), .05),
                (0.70, (-20.0, -4.0), .04), (0.80, (25.0, 0.0), .05)]


def _sentry_body(solver, scale=1.0):
    """Standing tall on the hind feet: legs nearly straight, back hollowed, fore paws hanging
    in front of the chest."""
    p = Pose()
    p.r("hips", ry=-86.0)
    p.move("hips", dz=.010 * scale)
    for key in ("HL", "HR"):
        target = solver.toe_base(key)
        target[0] = -.090 * scale
        p.plant(key, target, pole=(1, 0, 0))
    p.r("spine2", ry=-3.0)
    p.r("chest", ry=-3.0)
    for side in ("L", "R"):
        p.r("scapula." + side, ry=48.0)
        p.r("upper_arm." + side, ry=30.0)
        p.r("forearm." + side, ry=-14.0)
        p.r("hand." + side, ry=40.0)
    return p


def _sentry_clip(solver, scale=1.0):
    """`sentry`: the meerkat on guard, upright on its hind feet with the tail as a prop, the
    head turning in quick moves and holding to watch, once tipped up to the sky."""
    tail_angle = _tail_prop(solver, _sentry_body(solver, scale), floor=.003 * scale,
                            tail_tip=TAIL_TIP * scale)

    def pose(u):
        look = activity.held(u, SENTRY_LOOKS)
        p = _sentry_body(solver, scale)
        p.r("tail1", ry=tail_angle)
        p.r("spine2", rx=.15 * look[0])
        p.r("chest", rx=.15 * look[0])
        p.r("neck1", ry=56.0)
        p.r("neck2", ry=22.0)
        p.r("head", ry=8.0 + look[1], rz=.7 * look[0])
        # Breathing: the chest swells a little; the stance itself does not move.
        p.r("chest", ry=.8 * math.sin(4.0 * math.tau * u))
        return p
    return Clip("sentry", 9.0, pose)


def _dig_clip(solver, scale=1.0):
    """`dig`: crouched over a hole, the fore paws scratching in turn, the nose low; once a
    loop the paws pause and the nose goes into the hole."""
    strokes = 7
    base_front = {key: solver.toe_base(key) for key in ("FL", "FR")}

    def pose(u):
        p = Pose()
        # Pause: the last fifth of the loop the paws rest and the head goes down.
        work = 1.0 - activity.held(u, [(0.0, 0.0), (0.78, 1.0, .06), (0.96, 0.0, .04)])
        sniff = 1.0 - work
        p.move("hips", dz=(-.030 - .006 * sniff) * scale)
        p.r("hips", ry=14.0 + 3.0 * sniff, rx=1.5 * work * math.sin(strokes * math.tau * u))
        for key in ("HL", "HR"):
            target = solver.toe_base(key)
            target[1] += (.006 if key == "HL" else -.006) * scale
            p.plant(key, target)
        for key, offset in (("FL", 0.0), ("FR", .5)):
            phase = (strokes * u + offset) % 1.0
            # Power stroke on the ground backwards under the chest, then up and forward again.
            if phase < .55:
                s = phase / .55
                dx = (.028 - .056 * s) * scale
                dz = 0.0
            else:
                s = (phase - .55) / .45
                dx = (-.028 + .056 * s) * scale
                dz = .022 * scale * math.sin(math.pi * s)
            target = base_front[key] + [scale * .018 + work * dx, 0, work * dz]
            # The paw flexes back as it lifts, claws trailing.
            p.plant(key, target, foot_pitch=-25.0 * work * dz / (.022 * scale))
        p.r("chest", ry=6.0)
        p.r("neck1", ry=22.0 + 10.0 * sniff)
        p.r("neck2", ry=10.0 + 6.0 * sniff)
        p.r("head", ry=20.0 + 8.0 * sniff + 3.0 * work * math.sin(2 * strokes * math.tau * u))
        for i, bone in enumerate(("tail1", "tail2", "tail3", "tail4")):
            p.r(bone, ry=-6.0 + 2.0 * i, rz=4.0 * math.sin(math.tau * u - .5 * i))
        return p
    return Clip("dig", 3.0, pose)


def _tail(spec):
    by = {b.name: b for b in spec.bones}
    points = [(-.147, 0, .175), (-.19, 0, .145), (-.235, 0, .112),
              (-.285, 0, .086), (-.322, 0, .065)]
    for i in range(4):
        bone = by[f"tail{i + 1}"]
        bone.head = points[i]
        bone.tail = points[i + 1]
    spec.parts = [p for p in spec.parts if p.name not in ("tail", "tail_tuft")]
    chain = [(f"tail{i + 1}", points[i], points[i + 1]) for i in range(4)]
    spec.parts.append(Part("long_tail", [R(points[0],.013,.013),R(points[1],.011,.011),
        R(points[2],.009,.009),R(points[3],.006,.006),R(points[4],.004,.004)],
        chain=chain, segments=10, tag="tail", cap_end="flat"))
    spec.parts.append(Part("dark_tail_tip", [R(points[3],.006,.006),
        R(points[4],.005,.005),R((-.332,0,.062),.002,.002)],
        weights={"tail4":1.0}, segments=10, tag="tail_tip", rough=(.003,24.0)))


def _bend_legs(spec):
    spec.parts = [part for part in spec.parts if part.name not in
                  ("torso", "front_leg.L", "hind_leg.L", "front_paw.L", "hind_paw.L")]
    spec.parts.append(Part("torso", [
        R((-.15, 0, .17), .023, .032, .037), R((-.11, 0, .17), .045, .056, .060),
        R((-.055, 0, .17), .048, .069, .065), R((.005, 0, .17), .043, .073, .060),
        R((.055, 0, .17), .037, .071, .052), R((.078, 0, .17), .024, .055, .037)],
        chain=chain(spec.bones, "hips", "spine1", "spine2", "chest"), segments=20))
    for front, x in ((True, .065), (False, -.10)):
        y = .039
        points = ([(x, y, .20), (x, y, .18), (x - .035, y, .10),
                   (x + .005, y, .038), (x + .010, y, .018), (x + .04, y, .012)] if front else
                  [(x, y, .17), (x + .05, y, .105), (x - .025, y, .045),
                   (x + .005, y, .018), (x + .04, y, .012)])
        limb_chain = leg_bones(spec.bones, front, points)
        joint = points[2] if front else points[1]
        label = "front" if front else "hind"
        spec.parts.append(Part(label + "_leg.L", [
            R(points[1] if front else points[0], .020, .025),
            R(joint, .015, .019), R((x, y, .025), .010, .012)],
            chain=limb_chain, mirror=True, segments=12, tag="leg"))
        spec.parts.append(Part(label + "_paw.L", [R((x, y, .015), .014, .012),
            R((x + .035, y, .012), .017, .01)], chain=limb_chain[-2:],
            mirror=True, segments=12, tag="paw", cap_scale=.3))
        foot_bone = "fingers.L" if front else "toes.L"
        for digit, offset in enumerate((-.014, -.0047, .0047, .014)):
            spec.parts.append(Part(f"{label}_toe.{digit}.L", [
                R((x + .026, y + offset, .013), .006, .0055, .005),
                R((x + .045, y + offset, .011), .005, .0045, .004)],
                weights={foot_bone: 1.0}, mirror=True, fuse=False, segments=8,
                tag="toe", cap_start="flat", cap_end="flat"))
            claw_length = .014 if front else .009
            spec.parts.append(Part(f"{label}_claw.{digit}.L", [
                R((x + .045, y + offset, .011), .0045, .004, .004),
                R((x + .045 + claw_length, y + offset, .007), .0015, .0015, .0015)],
                weights={foot_bone: 1.0}, mirror=True, fuse=False, segments=8,
                tag="claw", cap_start="flat", cap_end="point"))


def _refine_anatomy(spec):
    """Replace the generic small-quadruped masses with a meerkat's lean mongoose outline.

    The skeleton and every contact target stay put: sentry, digging and locomotion therefore
    keep their solved contacts.  Only the lofts change.  In particular, the head must taper
    through a broad rounded cranium into the short pointed muzzle instead of reading as one
    sphere, and the neck, trunk and limbs must not carry a primate-like amount of mass.
    """
    replace = {"torso", "neck", "skull", "jaw", "ear.L", "eye.L", "nose",
               "front_leg.L", "hind_leg.L", "front_paw.L", "hind_paw.L"}
    spec.parts = [part for part in spec.parts if part.name not in replace]

    spec.parts.extend([
        Part("torso", [
            R((-.150, 0, .170), .021, .029, .033),
            R((-.112, 0, .170), .039, .048, .052),
            R((-.058, 0, .170), .041, .055, .052),
            R(( .002, 0, .170), .038, .057, .049),
            R(( .053, 0, .171), .033, .052, .043),
            R(( .079, 0, .174), .022, .039, .031),
        ], chain=chain(spec.bones, "hips", "spine1", "spine2", "chest"), segments=20,
           cap_scale=.45),
        Part("neck", [
            R((.050, 0, .174), .031, .044, .039),
            R((.078, 0, .193), .027, .038, .034),
            R((.096, 0, .216), .022, .031, .027),
            R((.103, 0, .225), .020, .027, .025),
        ], chain=chain(spec.bones, "chest", "neck1", "neck2", "head"), segments=18,
           cap_start="flat", cap_end="flat"),
        Part("skull", [
            R((.096, 0, .228), .020, .024, .025),
            R((.113, 0, .229), .027, .027, .026, n=2.2),
            R((.130, 0, .226), .026, .024, .023, n=2.3),
            R((.148, 0, .220), .019, .018, .016, n=2.4),
            R((.165, 0, .215), .012, .011, .010, n=2.5),
        ], weights={"head": 1.0}, tag="head", segments=20, cap_scale=.35),
        Part("jaw", [
            R((.125, 0, .207), .018, .0080, .0080),
            R((.148, 0, .205), .015, .0070, .0070),
            R((.164, 0, .204), .011, .0055, .0055),
        ], weights={"jaw": 1.0}, tag="jaw", segments=14, cap_scale=.3),
        # Small rounded pinnae sit low on the sides of the skull; a meerkat does not have
        # the generic generator's upright triangular ears.
        Part("ear.L", [
            R((.103, .019, .235), .0075, .0070),
            R((.101, .026, .241), .0080, .0080),
            R((.101, .030, .244), .0050, .0050),
        ], weights={"ear.L": 1.0}, tag="ear", segments=12, mirror=True,
           side_ref=(.2, 1, 0), cap_scale=.25),
        # Two close rings stretch the visible eye horizontally, matching the species'
        # elongated pupil and preventing the black eye patch from becoming a featureless dot.
        Part("eye.L", [R((.124, .0255, .232), .0040, .0033),
                        R((.131, .0255, .231), .0040, .0033)],
             weights={"head": 1.0}, tag="eye", segments=12, subdiv=1,
             fuse=False, mirror=True, cap_scale=.3),
        Part("nose", [R((.164, 0, .216), .0105, .0070),
                       R((.177, 0, .214), .0075, .0050)],
             weights={"head": 1.0}, tag="nose", segments=12, subdiv=1,
             fuse=False, cap_scale=.3),
    ])

    for front, x in ((True, .065), (False, -.10)):
        y = .039
        points = ([(x, y, .20), (x, y, .18), (x - .035, y, .10),
                   (x + .005, y, .038), (x + .010, y, .018), (x + .04, y, .012)] if front else
                  [(x, y, .17), (x + .05, y, .105), (x - .025, y, .045),
                   (x + .005, y, .018), (x + .04, y, .012)])
        limb_chain = leg_bones(spec.bones, front, points)
        joint = points[2] if front else points[1]
        label = "front" if front else "hind"
        spec.parts.append(Part(label + "_leg.L", [
            R(points[1] if front else points[0], .016, .020),
            R(joint, .012, .015), R((x, y, .025), .0085, .010)],
            chain=limb_chain, mirror=True, segments=12, tag="leg"))
        spec.parts.append(Part(label + "_paw.L", [R((x, y, .015), .0115, .0095),
            R((x + .035, y, .012), .0145, .0085)], chain=limb_chain[-2:],
            mirror=True, segments=12, tag="paw", cap_scale=.3))



def _paint(ctx, scale=1.0, juvenile=False):
    # Work in adult coordinates so the markings retain their width on the smaller variant.
    P = ctx.P / scale
    x, z = ctx.x / scale, ctx.z / scale
    sandy = np.array([.54,.43,.30]) if juvenile else np.array([.48,.36,.24])
    pale = np.array([.86,.78,.65]) if juvenile else np.array([.82,.72,.57])
    dark = np.array([.25,.18,.12]) if juvenile else np.array([.20,.13,.085])
    band = np.sin((x + .3) * 57.0 + .8 * np.sin(z * 28.0))
    band = (band > .94).astype(float) * ctx.tag("body")
    band *= nz.smoothstep(-.025, -.06, x) * nz.smoothstep(.1, .7, ctx.N[..., 2])
    coat = nz.mix(sandy, pale, nz.smoothstep(.20,-.75,ctx.N[...,2])*.65)
    coat = nz.mix(coat, dark, band * .68)
    # The juvenile head transform shortens the face around x=.098 after uniform scaling.
    # Move the painted mask with the physical eye instead of leaving a dark adult-position scar.
    eye_x = .119 if juvenile else .128
    eye_distance = ((x - eye_x) / .012) ** 2 + ((z - .231) / .008) ** 2
    eye_patch = nz.smoothstep(1.4, .65, eye_distance) * nz.smoothstep(.015, .025, np.abs(ctx.y))
    coat = nz.mix(coat, dark * .85, eye_patch)
    coat = nz.mix(coat, np.array([.035, .025, .018]), ctx.tag("eye"))
    coat = nz.mix(coat, np.array([.06,.045,.035]), ctx.tag("nose"))
    coat = nz.mix(coat, dark*.72, ctx.tag("ear")*.5)
    coat = nz.mix(coat, np.array([.10,.07,.045]), ctx.tag("tail_tip"))
    coat = nz.mix(coat, np.array([.11, .075, .045]), ctx.tag("toe") * .55)
    coat = nz.mix(coat, np.array([.035, .025, .018]), ctx.tag("claw"))
    coat *= (.92 + .14 * nz.fbm(P,70.0,3,seed=23))[...,None]
    return coat


def _surface_height(ctx, scale=1.0):
    """Short guard hairs aligned along the body, excluding the bare face details and claws."""
    P = ctx.P / scale
    fine = nz.fbm(P * np.array([.42, 1.10, 1.75]), 135.0, 3, seed=33)
    clumps = nz.fbm(P, 44.0, 3, seed=34)
    fur = .88 * fine + .12 * clumps
    smooth = np.clip(ctx.tag("eye") * 4.0 + ctx.tag("nose") * 3.0
                     + ctx.tag("claw") * 2.0, 0.0, 1.0)
    return .5 + (fur - .5) * (1.0 - smooth)


def _juvenile_shape(spec):
    """Scale the adult rig, then give a pup a rounder head and proportionally large extremities."""
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

    # A young meerkat's shorter face, rounder cranium and conspicuous eyes/ears are what keep
    # it from reading as an adult uniformly shrunk by the renderer.
    pivot = np.array([.098, 0, .220]) * scale

    def shorten_head(point):
        point[0] = pivot[0] + .72 * (point[0] - pivot[0])
        point[1] = pivot[1] + 1.10 * (point[1] - pivot[1])
        point[2] = pivot[2] + 1.08 * (point[2] - pivot[2])

    for bone in spec.bones:
        if bone.name in {"neck2", "head", "jaw", "ear.L"}:
            shorten_head(bone.head)
            shorten_head(bone.tail)
    for part in spec.parts:
        if part.name in {"skull", "jaw", "ear.L", "eye.L", "nose"}:
            for ring in part.rings:
                shorten_head(ring.c)
        if part.name == "skull":
            for ring in part.rings:
                ring.ry *= 1.22
                ring.up *= 1.18
                ring.dn *= 1.18
        elif part.name == "eye.L":
            for ring in part.rings:
                ring.ry *= 1.38
                ring.up *= 1.38
                ring.dn *= 1.38
        elif part.name == "ear.L":
            for ring in part.rings:
                ring.ry *= 1.18
                ring.up *= 1.18
                ring.dn *= 1.18
        elif part.name.endswith("_paw.L"):
            for ring in part.rings:
                ring.ry *= 1.20
                ring.up *= 1.14
                ring.dn *= 1.14

    # Chains contain rest positions copied when the parts were made; refresh them after the
    # head joints moved so skinning and the armature agree.
    bones = {bone.name: bone for bone in spec.bones}
    for part in spec.parts:
        if part.chain is not None:
            part.chain = [(name, bones[name].head.copy(), bones[name].tail.copy())
                          for name, _, _ in part.chain]

    spec.name = "meerkat_juvenile"
    spec.voxel = .0025
    spec.target_tris = 8000
    spec.paint = partial(_paint, scale=scale, juvenile=True)
    spec.surface_height = partial(_surface_height, scale=scale)
    spec.meta.update({"shoulder_height_m": .17, "body_length_m": .238,
                      "variant_of": "meerkat", "variant": "juvenile",
                      "relative_height": scale,
                      "age_shape": "Pup: shorter face, rounder skull, larger eyes, ears and paws"})


def make_spec(juvenile=False):
    scale = JUVENILE_SCALE if juvenile else 1.0
    walk_speed = .42 if juvenile else .6
    trot_speed = 1.05 if juvenile else 1.5
    walk_period = .25 if juvenile else .30
    trot_period = .19 if juvenile else .22
    spec = make_wild_quadruped({
        "name":"meerkat", "kind":"meerkat", "common_name":"Meerkat",
        "scientific_name":"Suricata suricatta", "height":.25, "length":.35,
        "walk_speed":walk_speed, "trot_speed":trot_speed,
        "walk_period":walk_period, "trot_period":trot_period,
        "walk_lift":.025 * scale, "fast_lift":.035 * scale,
        "lie_drop":.05 * scale, "hind_tuck":.015 * scale, "hind_out":.01 * scale,
        "head_down":quad.HeadDown(body_pitch=8,crouch=.07 * scale,chest=4,
                                   neck=(12,10),head=5),
        "sleep_neck":(5,5), "sleep_head":5,
    })
    _tail(spec)
    _bend_legs(spec)
    _refine_anatomy(spec)
    spec.paint = _paint
    spec.surface_height = _surface_height
    spec.surface_strength = 1.05
    spec.voxel = .0035
    spec.target_tris = 8000
    spec.smooth_iters = 4
    spec.meta["roughness"] = .91
    spec.meta["surface_detail"] = "anisotropic short-coat normal map"
    spec.meta["anatomy"] = ("Third-pass lean mongoose outline: tapered face, low rounded ears, "
                            "slender neck, trunk and limbs")
    def clips(solver):
        return quad.standard_clips(solver, quad.Params(
            gaits=[quad.Gait("walk", walk_period, walk_speed * walk_period * .64, .64, quad.WALK,
                       lift=.018 * scale, bob=.002 * scale, hip_yaw=1, chest_yaw=1,
                       sway=.001 * scale, roll=.2, heel_off=12, swing_pitch=20, land_pitch=0,
                       drop=.02 * scale),
                   quad.Gait("trot", trot_period, trot_speed * trot_period * .42, .42, quad.TROT,
                       lift=.025 * scale, bob=.002 * scale, hip_yaw=1, chest_yaw=1,
                       sway=.001 * scale, roll=.2, heel_off=12, swing_pitch=20, land_pitch=0,
                       drop=.03 * scale)],
            lie=quad.Lie(drop=.05 * scale, hind_tuck=.015 * scale, hind_out=.01 * scale),
            head_down=quad.HeadDown(body_pitch=8, crouch=.07 * scale, chest=4,
                                    neck=(12, 10), head=5),
            sleep_neck=(5, 5), sleep_head=5, idle_shift=0, idle_bob=.001 * scale,
            idle_roll=0, tail_idle=3))

    def sentinel(solver):
        generated = clips(solver)
        for clip in generated:
            if clip.name != "idle":
                continue

            # The body and tail do not move vertically while scanning, so solve once.
            prop_pose = Pose().r("hips", ry=-82.0).move("hips", dz=-.065 * scale)
            tail_angle = _tail_prop(solver, prop_pose, 90.0, 120.0, .003 * scale,
                                    TAIL_TIP * scale)

            def upright(u):
                a = 2.0 * math.pi * u
                p = Pose()
                p.r("hips", ry=-82.0)
                p.move("hips", dz=-.065 * scale)
                # Plant the hind pads while the body rises vertically above the pelvis.
                for key in ("HL", "HR"):
                    target = solver.toe_base(key)
                    target[0] = -.095 * scale
                    p.plant(key, target, pole=(1, 0, 0))
                for side in ("L", "R"):
                    p.r("scapula." + side, ry=55.0)
                    p.r("upper_arm." + side, ry=27.0)
                    p.r("forearm." + side, ry=12.0)
                p.r("neck1", ry=50.0)
                p.r("neck2", ry=20.0)
                p.r("head", ry=10.0, rz=18.0 * math.sin(a))
                p.r("tail1", ry=tail_angle)
                return p

            clip.pose = upright
        return generated + [_sentry_clip(solver, scale), _dig_clip(solver, scale)]

    spec.clips = sentinel
    # The game's clips for two still activities (AnimalRenderer): the guard on duty stands
    # tall on its hind feet; a forager digs with its fore paws.
    spec.meta["activity_clips"] = {"sentinel": "sentry", "dig": "dig"}
    if juvenile:
        _juvenile_shape(spec)
    return spec
