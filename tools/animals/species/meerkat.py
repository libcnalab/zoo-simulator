# SPDX-License-Identifier: MIT
"""Adult meerkat: 0.25 m at the shoulder and 0.35 m body length, tail excluded."""
import math

import numpy as np

from zoogen import activity
from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Part, R
from zoogen.rig import Clip, Pose
from zoogen.anatomy import chain, leg_bones
from zoogen.wild_quadruped import make_wild_quadruped


TAIL_TIP = np.array([-.332, 0, .062])


def _tail_prop(solver, body_pose, low=60.0, high=140.0, floor=.003):
    """The tail1 pitch at which the tail tip rests on the ground under a still upright body:
    the third leg of the tripod a standing meerkat leans on."""
    for _ in range(22):
        mid = (low + high) * .5
        body_pose.rot.pop("tail1", None)
        body_pose.r("tail1", ry=mid)
        height = solver.point(solver.solve(body_pose), "tail4", TAIL_TIP)[2]
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


def _sentry_body(solver):
    """Standing tall on the hind feet: legs nearly straight, back hollowed, fore paws hanging
    in front of the chest."""
    p = Pose()
    p.r("hips", ry=-86.0)
    p.move("hips", dz=.010)
    for key in ("HL", "HR"):
        target = solver.toe_base(key)
        target[0] = -.090
        p.plant(key, target, pole=(1, 0, 0))
    p.r("spine2", ry=-3.0)
    p.r("chest", ry=-3.0)
    for side in ("L", "R"):
        p.r("scapula." + side, ry=48.0)
        p.r("upper_arm." + side, ry=30.0)
        p.r("forearm." + side, ry=-14.0)
        p.r("hand." + side, ry=40.0)
    return p


def _sentry_clip(solver):
    """`sentry`: the meerkat on guard, upright on its hind feet with the tail as a prop, the
    head turning in quick moves and holding to watch, once tipped up to the sky."""
    tail_angle = _tail_prop(solver, _sentry_body(solver))

    def pose(u):
        look = activity.held(u, SENTRY_LOOKS)
        p = _sentry_body(solver)
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


def _dig_clip(solver):
    """`dig`: crouched over a hole, the fore paws scratching in turn, the nose low; once a
    loop the paws pause and the nose goes into the hole."""
    strokes = 7
    base_front = {key: solver.toe_base(key) for key in ("FL", "FR")}

    def pose(u):
        p = Pose()
        # Pause: the last fifth of the loop the paws rest and the head goes down.
        work = 1.0 - activity.held(u, [(0.0, 0.0), (0.78, 1.0, .06), (0.96, 0.0, .04)])
        sniff = 1.0 - work
        p.move("hips", dz=-.030 - .006 * sniff)
        p.r("hips", ry=14.0 + 3.0 * sniff, rx=1.5 * work * math.sin(strokes * math.tau * u))
        for key in ("HL", "HR"):
            target = solver.toe_base(key)
            target[1] += (.006 if key == "HL" else -.006)
            p.plant(key, target)
        for key, offset in (("FL", 0.0), ("FR", .5)):
            phase = (strokes * u + offset) % 1.0
            # Power stroke on the ground backwards under the chest, then up and forward again.
            if phase < .55:
                s = phase / .55
                dx = .028 - .056 * s
                dz = 0.0
            else:
                s = (phase - .55) / .45
                dx = -.028 + .056 * s
                dz = .022 * math.sin(math.pi * s)
            target = base_front[key] + [.018 + work * dx, 0, work * dz]
            # The paw flexes back as it lifts, claws trailing.
            p.plant(key, target, foot_pitch=-25.0 * work * dz / .022)
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
        R((-.15, 0, .17), .025, .035, .04), R((-.11, 0, .17), .049, .06, .065),
        R((-.055, 0, .17), .053, .075, .07), R((.005, 0, .17), .047, .08, .065),
        R((.055, 0, .17), .04, .078, .055), R((.078, 0, .17), .026, .06, .04)],
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
            R(points[1] if front else points[0], .023, .028),
            R(joint, .018, .022), R((x, y, .025), .012, .014)],
            chain=limb_chain, mirror=True, segments=12, tag="leg"))
        spec.parts.append(Part(label + "_paw.L", [R((x, y, .015), .014, .012),
            R((x + .035, y, .012), .017, .01)], chain=limb_chain[-2:],
            mirror=True, segments=12, tag="paw", cap_scale=.3))



def _paint(ctx):
    P = ctx.P
    sandy = np.array([.48,.36,.24])
    pale = np.array([.82,.72,.57])
    dark = np.array([.20,.13,.085])
    band = np.sin((ctx.x + .3) * 57.0 + .8 * np.sin(ctx.z * 28.0))
    band = (band > .94).astype(float) * ctx.tag("body")
    band *= nz.smoothstep(-.025, -.06, ctx.x) * nz.smoothstep(.1, .7, ctx.N[..., 2])
    coat = nz.mix(sandy, pale, nz.smoothstep(.20,-.75,ctx.N[...,2])*.65)
    coat = nz.mix(coat, dark, band * .68)
    eye_distance = ((ctx.x - .121) / .009) ** 2 + ((ctx.z - .231) / .008) ** 2
    eye_patch = nz.smoothstep(1.4, .65, eye_distance) * nz.smoothstep(.015, .025, np.abs(ctx.y))
    coat = nz.mix(coat, dark * .85, eye_patch)
    coat = nz.mix(coat, np.array([.035, .025, .018]), ctx.tag("eye"))
    coat = nz.mix(coat, np.array([.06,.045,.035]), ctx.tag("nose"))
    coat = nz.mix(coat, dark*.72, ctx.tag("ear")*.5)
    coat = nz.mix(coat, np.array([.10,.07,.045]), ctx.tag("tail_tip"))
    coat *= (.92 + .14 * nz.fbm(P,70.0,3,seed=23))[...,None]
    return coat


def make_spec():
    spec = make_wild_quadruped({
        "name":"meerkat", "kind":"meerkat", "common_name":"Meerkat",
        "scientific_name":"Suricata suricatta", "height":.25, "length":.35,
        "walk_speed":.6, "trot_speed":1.5,
        "walk_period":.30, "trot_period":.22,
        "walk_lift":.025, "fast_lift":.035,
        "lie_drop":.05, "hind_tuck":.015, "hind_out":.01,
        "head_down":quad.HeadDown(body_pitch=8,crouch=.07,chest=4,
                                   neck=(12,10),head=5),
        "sleep_neck":(5,5), "sleep_head":5,
    })
    _tail(spec)
    _bend_legs(spec)
    for part in spec.parts:
        if part.name == "eye.L":
            part.rings = [R((.119, .026, .231), .0035, .0035),
                          R((.123, .026, .231), .0035, .0035)]
    spec.paint = _paint
    spec.voxel = .0035
    spec.target_tris = 8000
    spec.smooth_iters = 4
    spec.meta["roughness"] = .91
    def clips(solver):
        return quad.standard_clips(solver, quad.Params(
            gaits=[quad.Gait("walk", .30, .6 * .30 * .64, .64, quad.WALK,
                       lift=.018, bob=.002, hip_yaw=1, chest_yaw=1, sway=.001, roll=.2,
                       heel_off=12, swing_pitch=20, land_pitch=0, drop=.02),
                   quad.Gait("trot", .22, 1.5 * .22 * .42, .42, quad.TROT,
                       lift=.025, bob=.002, hip_yaw=1, chest_yaw=1, sway=.001, roll=.2,
                       heel_off=12, swing_pitch=20, land_pitch=0, drop=.03)],
            lie=quad.Lie(drop=.05, hind_tuck=.015, hind_out=.01),
            head_down=quad.HeadDown(body_pitch=8, crouch=.07, chest=4, neck=(12, 10), head=5),
            sleep_neck=(5, 5), sleep_head=5, idle_shift=0, idle_bob=.001,
            idle_roll=0, tail_idle=3))

    def sentinel(solver):
        generated = clips(solver)
        for clip in generated:
            if clip.name != "idle":
                continue

            # The body and tail do not move vertically while scanning, so solve once.
            prop_pose = Pose().r("hips", ry=-82.0).move("hips", dz=-.065)
            tail_angle = _tail_prop(solver, prop_pose, 90.0, 120.0)

            def upright(u):
                a = 2.0 * math.pi * u
                p = Pose()
                p.r("hips", ry=-82.0)
                p.move("hips", dz=-.065)
                # Plant the hind pads while the body rises vertically above the pelvis.
                for key in ("HL", "HR"):
                    target = solver.toe_base(key)
                    target[0] = -.095
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
        return generated + [_sentry_clip(solver), _dig_clip(solver)]

    spec.clips = sentinel
    # The game's clips for two still activities (AnimalRenderer): the guard on duty stands
    # tall on its hind feet; a forager digs with its fore paws.
    spec.meta["activity_clips"] = {"sentinel": "sentry", "dig": "dig"}
    return spec
