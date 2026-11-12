# SPDX-License-Identifier: MIT
"""Adult African bush elephant, 3.2 m at the shoulder and 6.0 m body length.

The shoulder is the highest point of the barrel; the domed head and trunk rise
above it. Length excludes the tail.
"""
import math

import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Bone, Part, R
from zoogen.anatomy import chain, leg_bones, round_foot, set_bone
from zoogen.wild_quadruped import make_wild_quadruped


def _trunk_bones(spec):
    points = [(2.65, 0.0, 2.85), (2.82, 0.0, 2.42), (2.91, 0.0, 1.97),
              (2.96, 0.0, 1.50), (3.00, 0.0, 1.03), (3.04, 0.0, .57),
              (3.08, 0.0, .17)]
    parent = "head"
    for i in range(6):
        name = f"trunk{i + 1}"
        spec.bones.append(Bone(name, parent, points[i], points[i + 1]))
        parent = name
    chain = [(f"trunk{i + 1}", points[i], points[i + 1]) for i in range(6)]
    rings = [R(points[0], .38, .34), R(points[1], .30, .28),
             R(points[2], .24, .23), R(points[3], .19, .19),
             R(points[4], .15, .15), R(points[5], .11, .11),
             R(points[6], .07, .07)]
    spec.parts.append(Part("trunk", rings, chain=chain, segments=22,
                           tag="trunk", cap_start="flat", cap_end="round"))


def _ears(spec):
    spec.bones.append(Bone("ear_flap.L", "head", (1.95, .56, 3.05), (1.40, .92, 2.15)))
    rings = [R((1.65, .77, 3.10), .045, .22, .20),
             R((1.56, .92, 2.90), .055, .52, .48),
             R((1.44, .94, 2.54), .055, .65, .54),
             R((1.37, .93, 2.08), .045, .43, .35),
             R((1.43, .87, 1.65), .030, .12, .10)]
    spec.parts.append(Part("ear_flap.L", rings, weights={"ear_flap.L": 1.0},
                           mirror=True, segments=24, tag="ear", cap_scale=.20, fuse=False))
    spec.rig.ears = ["ear_flap.L", "ear_flap.R"]


def _tusks(spec):
    for side in (-1.0, 1.0):
        suffix = "L" if side > 0 else "R"
        points = [(2.47, side * .39, 2.48), (2.80, side * .44, 2.24),
                  (3.16, side * .47, 2.16), (3.43, side * .48, 2.32)]
        spec.parts.append(Part(f"tusk.{suffix}", [R(points[0], .12, .11),
            R(points[1], .095, .09), R(points[2], .055, .05),
            R(points[3], .004, .004)], weights={"head": 1.0}, fuse=False,
            tag="tusk", segments=16, cap_start="flat", cap_end="point"))


def _anatomy(spec):
    spine = chain(spec.bones, "hips", "spine1", "spine2", "chest")
    neck = chain(spec.bones, "chest", "neck1", "neck2", "head")
    spec.parts = [Part("barrel", [
        R((-2.38, 0, 2.18), .18, .30, .34),
        R((-2.08, 0, 2.18), .64, .74, .73),
        R((-1.50, 0, 2.18), .87, .87, .91),
        R((-.70, 0, 2.18), .99, .82, 1.02),
        R((.15, 0, 2.18), 1.02, .87, 1.04),
        R((.90, 0, 2.18), .91, 1.02, 1.00),
        R((1.40, 0, 2.18), .64, .85, .78),
    ], chain=spine, segments=36, cap_scale=.40),
        Part("neck", [R((1.03, 0, 2.45), .62, .65), R((1.62, 0, 2.72), .54, .62),
                      R((2.03, 0, 2.84), .48, .53)], chain=neck, segments=28),
        Part("head", [R((1.66, 0, 2.83), .32, .47, .48),
                      R((1.99, 0, 2.88), .55, .67, .64),
                      R((2.30, 0, 2.85), .54, .63, .57),
                      R((2.55, 0, 2.72), .39, .44, .40)],
             weights={"head": 1.0}, segments=28, cap_scale=.40)]
    spec.parts.append(Part("eye.L", [R((2.40, .49, 3.02), .038, .038),
        R((2.44, .49, 3.02), .034, .034)], weights={"head": 1.0},
        mirror=True, fuse=False, segments=12, tag="eye"))
    for front, x, y, top in ((True, 1.02, .66, 2.65), (False, -1.59, .65, 2.45)):
        points = ([(x, y, top + .12), (x, y, top), (x - .15, y, 1.32),
                   (x, y, .34), (x, y, .12), (x + .13, y, .07)] if front else
                  [(x, y, top), (x + .19, y, .96), (x, y, .32),
                   (x, y, .12), (x + .13, y, .07)])
        limb_chain = leg_bones(spec.bones, front, points)
        label = "front" if front else "hind"
        spec.parts.append(Part(label + "_leg.L", [
            R((x, .38, top), .20, .24), R((x, y, 1.85), .32, .34),
            R((x + (-.08 if front else .09), y, 1.10), .27, .28),
            R((x, y, .53), .25, .26), R((x, y, .24), .27, .28),
        ], chain=limb_chain, mirror=True, segments=24, cap_start="flat", cap_end="flat"))
        foot_bone = "fingers.L" if front else "toes.L"
        spec.parts.append(round_foot(label + "_foot.L", x, y, .31, .30, foot_bone))
        for digit in range(3):
            dy = (digit - 1) * .16
            spec.parts.append(Part(f"{label}_nail{digit}.L", [
                R((x + .265, y + dy, .10), .062, .07),
                R((x + .291, y + dy, .10), .060, .065)],
                weights={foot_bone: 1.0}, mirror=True, fuse=False, segments=10, tag="nail"))
    points = [(-2.24, 0, 2.27), (-2.48, 0, 1.87), (-2.57, 0, 1.48),
              (-2.59, 0, 1.08), (-2.61, 0, .79)]
    for i in range(4):
        set_bone(spec.bones, f"tail{i + 1}", points[i], points[i + 1])
    spec.parts.append(Part("tail", [R(point, .045 - .006 * i, .045 - .006 * i)
        for i, point in enumerate(points)], chain=chain(spec.bones, "tail1", "tail2", "tail3", "tail4"),
        segments=12))
    spec.parts.append(Part("tuft", [R(points[-1], .065, .09), R((-2.63, 0, .63), .045, .06)],
        weights={"tail4": 1.0}, segments=12, tag="tuft"))


def _paint(ctx):
    P = ctx.P
    grey = np.array([.39, .39, .36])
    grain = nz.fbm(P, 45.0, 3, seed=18)
    coat = grey * (.91 + .14 * grain)[..., None]
    # Wrinkles belong on the flexible trunk, not as ribs across the barrel.
    wrinkle = np.maximum(0.0, np.sin(ctx.z * 65.0 + np.sin(ctx.x * 17.0)))
    coat *= (1.0 - .12 * wrinkle * ctx.tag("trunk"))[..., None]
    coat = nz.mix(coat, np.array([.65, .61, .51]), ctx.tag("nail"))
    coat = nz.mix(coat, np.array([.10, .085, .07]), ctx.tag("tuft"))
    coat = nz.mix(coat, np.array([.035, .028, .022]), ctx.tag("eye"))
    coat = nz.mix(coat, np.array([.78, .69, .52]), ctx.tag("tusk"))
    ear_inner = ctx.tag("ear") * nz.smoothstep(.50, .85, -ctx.N[..., 1] * np.sign(ctx.y))
    coat = nz.mix(coat, np.array([.39, .25, .23]), ear_inner * .48)
    return coat


def make_spec():
    spec = make_wild_quadruped({
        "name": "elephant", "kind": "elephant", "common_name": "African bush elephant",
        "scientific_name": "Loxodonta africana", "height": 3.2, "length": 6.0,
        "walk_speed": 1.3, "trot_speed": 2.8,
        "walk_pattern": "walk", "fast_pattern": "walk",
        "head_down": quad.HeadDown(body_pitch=0.0, crouch=.40, chest=0.0,
                                    neck=(0.0, 0.0), head=0.0),
    })
    _anatomy(spec)
    _trunk_bones(spec)
    _ears(spec)
    _tusks(spec)
    spec.rig.extra["muzzle"] = ("trunk6", (3.08, 0.0, .17))
    spec.paint = _paint
    spec.voxel = .022
    spec.smooth_iters = 5
    spec.target_tris = 16000
    spec.meta["roughness"] = .94
    def clips(solver):
        return quad.standard_clips(solver, quad.Params(
            gaits=[quad.Gait("walk", 1.15, 1.3 * 1.15 * .68, .68, quad.WALK,
                       lift=.09, bob=.008, hip_yaw=1, chest_yaw=1, sway=.004, roll=.2,
                       heel_off=8, swing_pitch=12, land_pitch=0, toe_follow=0, scapula=.15),
                   quad.Gait("trot", .72, 2.8 * .72 * .45, .45, quad.WALK,
                       lift=.14, bob=.006, hip_yaw=1, chest_yaw=1, sway=.004, roll=.2,
                       heel_off=10, swing_pitch=15, land_pitch=0, toe_follow=0, scapula=.15)],
            lie=quad.Lie(drop=.9),
            head_down=quad.HeadDown(body_pitch=0, crouch=.4, chest=0, neck=(0, 0), head=0),
            idle_shift=0, idle_bob=0, idle_roll=0, look_yaw=5, ear_flick=5,
            call_neck=0, call_head=0, sleep_neck=(0, 0), sleep_head=0))

    def animate_trunk(solver):
        generated = clips(solver)
        for clip in generated:
            old_pose = clip.pose

            def pose(u, prior=old_pose, name=clip.name):
                p = prior(u)
                a = 2.0 * math.pi * u
                if name == "idle":
                    amount = 5.0 * math.sin(a)
                    p.r("ear_flap.L", rz=8.0 * math.sin(a))
                    p.r("ear_flap.R", rz=-8.0 * math.sin(a))
                elif name in ("eat", "drink"):
                    amount = 22.0 * (1.0 - math.cos(a)) * .5
                elif name in ("rest", "sleep"):
                    amount = 35.0
                elif name == "vocalize":
                    amount = -12.0
                else:
                    amount = 0.0
                if name in ("idle", "vocalize"):
                    p.move("hips", dz=-.035)
                for i in range(1, 7):
                    p.r(f"trunk{i}", ry=amount * i / 6.0)
                return p

            clip.pose = pose
        return generated

    spec.clips = animate_trunk
    return spec
