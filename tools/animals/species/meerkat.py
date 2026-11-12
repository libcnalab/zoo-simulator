# SPDX-License-Identifier: MIT
"""Adult meerkat: 0.25 m at the shoulder and 0.35 m body length, tail excluded."""
import math

import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Part, R
from zoogen.rig import Pose
from zoogen.anatomy import chain, leg_bones
from zoogen.wild_quadruped import make_wild_quadruped


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
            tip = np.array([-.332, 0, .062])
            low, high = 90.0, 120.0
            for _ in range(22):
                mid = (low + high) * .5
                prop_pose.rot.pop("tail1", None)
                prop_pose.r("tail1", ry=mid)
                height = solver.point(solver.solve(prop_pose), "tail4", tip)[2]
                if height < .003:
                    low = mid
                else:
                    high = mid
            tail_angle = (low + high) * .5

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
        return generated

    spec.clips = sentinel
    return spec
