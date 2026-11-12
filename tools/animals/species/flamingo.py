# SPDX-License-Identifier: MIT
"""Greater flamingo: 1.4 m standing head height, about 1.2 m head and body.

The simulation's shoulderHeightM and manifest height both refer to the head top.
The trunk is carried at 0.89 m, above long bare legs; an eleven-bone S-shaped neck
has enough arc length to reach water without stretching or breaking its curve.
"""
import math

import numpy as np

from zoogen import biped, noise as nz
from zoogen.geom import Bone, Part, R, Spec
from zoogen.quadruped import bisect_effort, muzzle
from zoogen.rig import Clip, Rig

NECK = [(.18, 0, .92), (.245, 0, .965), (.278, 0, 1.015), (.275, 0, 1.065),
        (.245, 0, 1.12), (.195, 0, 1.18), (.155, 0, 1.24), (.145, 0, 1.29),
        (.16, 0, 1.33), (.20, 0, 1.35), (.245, 0, 1.35), (.29, 0, 1.335)]


def _skeleton():
    bones = [Bone("root", None, (0, 0, 0), (.15, 0, 0), False),
             Bone("hips", "root", (-.06, 0, .87), (.04, 0, .90)),
             Bone("chest", "hips", (.04, 0, .90), NECK[0])]
    neck = biped.add_chain(bones, "neck", "chest", NECK)
    bones.append(Bone("head", neck[-1], NECK[-1], (.39, 0, 1.33)))
    legs = biped.add_leg(bones, "hips", [(-.06, .07, .87), (-.21, .07, .46),
        (-.02, .07, .055), (.04, .07, .022), (.14, .07, .018)])
    bones += [Bone("wing.L", "chest", (.13, .105, .96), (-.32, .13, .88)),
              Bone("tail1", "hips", (-.29, 0, .90), (-.43, 0, .88))]
    return bones, Rig("hips", legs, chest="chest", neck=neck, head="head", tail=["tail1"],
        extra={"muzzle": ("head", (.465, 0, 1.235)),
               "wings": {side: ["wing." + side] for side in ("L", "R")}})


def _parts(bones):
    return [Part("oval_body", [R((-.40, 0, .89), .025, .035),
        R((-.28, 0, .89), .105, .115), R((-.08, 0, .89), .145, .145, .14),
        R((.10, 0, .90), .13, .125, .14), R((.24, 0, .92), .06, .07)],
        chain=biped.chain(bones, "hips", "chest"), segments=32, cap_scale=.3),
        Part("S_neck", [R(point, width, depth) for point, width, depth in zip(
            NECK, (.055, .041, .033, .028, .025, .023, .021, .021, .021, .022, .024, .027),
            (.066, .045, .037, .032, .028, .026, .024, .023, .024, .026, .028, .031))],
             chain=biped.chain(bones, *[f"neck{index}" for index in range(1, len(NECK))], "head"),
             segments=24, cap_start="flat", cap_end="flat"),
        Part("head", [R((.275, 0, 1.345), .028, .035),
            R((.32, 0, 1.35), .043, .048, .040), R((.366, 0, 1.33), .031, .028)],
             weights={"head": 1}, segments=24, cap_scale=.25, tag="head"),
        Part("bent_bill", [R((.36, 0, 1.328), .031, .029),
            R((.414, 0, 1.306), .033, .032), R((.449, 0, 1.267), .027, .029),
            R((.465, 0, 1.235), .015, .018)], weights={"head": 1},
             segments=24, tag="beak", cap_scale=.25),
        biped.eye_part((.334, .040, 1.36), .0065),
        Part("wing.L", [R((.13, .095, .93), .025, .035),
            R((-.03, .139, .885), .021, .060), R((-.23, .12, .87), .018, .060),
            R((-.36, .07, .875), .01, .023)], weights={"wing.L": 1}, mirror=True,
             fuse=False, segments=20, tag="wing", cap_scale=.3),
        Part("bare_leg.L", [R((-.06, .07, .87), .020, .025),
            R((-.13, .07, .68), .012, .014), R((-.21, .07, .46), .015, .017),
            R((-.12, .07, .26), .009, .011), R((-.02, .07, .055), .010, .012)],
             chain=biped.chain(bones, "thigh.L", "shin.L", "foot.L"),
             mirror=True, fuse=False, segments=16, subdiv=3, tag="bare"),
        Part("ankle.L", [R((-.02, .07, .055), .011, .012), R((.04, .07, .022), .020, .012)],
             weights={"foot.L": 1}, mirror=True, fuse=False, segments=14, tag="bare"),
        biped.webbed_foot(.04, .07, .12, .038, .011),
        Part("tail", [R((-.28, 0, .9), .06, .035), R((-.43, 0, .88), .012, .01)],
             weights={"tail1": 1}, segments=16, tag="tail", cap_scale=.3)]


def paint(ctx):
    pale = np.array([.90, .72, .69])
    pink = np.array([.85, .34, .39])
    black = np.array([.026, .022, .026])
    grain = nz.fbm(ctx.P * [2, 3, 6], 75, 2, seed=73)
    color = pale * (.94 + .09 * grain)[..., None]
    coverts = ctx.tag("wing")
    color = nz.mix(color, pink * (.88 + .16 * grain)[..., None], coverts * .86)
    edge = coverts * nz.smoothstep(-.16, -.30, ctx.x) * nz.smoothstep(.94, .86, ctx.z)
    color = nz.mix(color, black, edge)
    color = nz.mix(color, np.array([.79, .32, .37]), ctx.tag("bare") + ctx.tag("foot"))
    beak = nz.mix(np.array([.91, .68, .63]), black, nz.smoothstep(1.29, 1.265, ctx.z))
    color = nz.mix(color, beak, ctx.tag("beak"))
    color = nz.mix(color, np.array([.85, .65, .12]), ctx.tag("eye"))
    # A tiny dark centre keeps the yellow eye from becoming a painted facial patch.
    pupil = ((ctx.x - .336) / .003) ** 2 + ((ctx.z - 1.36) / .0035) ** 2
    color = nz.mix(color, black, ctx.tag("eye") * nz.smoothstep(1, .4, pupil))
    return color


def _one_leg(solver, sleeping=False):
    pose = biped.standing(solver)
    folded = solver.toe_base("R")
    folded[:] = (.10, -.075, .73)
    pose.plant("R", folded, foot_pitch=65, toe_pitch=65, pole=(-1, -.1, -1))
    if sleeping:
        angles = [85, 95, 110, 130, 145, 165, 190, 215, 240, 265, 290]
        biped.sagittal_profile(solver, pose, solver.rig.neck, angles)
        biped.sagittal_profile(solver, pose, ["head"], [0])
    return pose


def _feeding(solver, effort):
    pose = biped.standing(solver)
    pose.move("hips", dz=-.30 * effort)
    pose.r("hips", ry=10 * effort)
    start = [math.degrees(math.atan2(tail[2] - head[2], tail[0] - head[0]))
             for head, tail in zip(NECK, NECK[1:])]
    end = [-70 - 5 * index for index in range(len(start))]
    angles = [a + (b - a) * effort for a, b in zip(start, end)]
    biped.sagittal_profile(solver, pose, solver.rig.neck, angles)
    biped.sagittal_profile(solver, pose, ["head"], [-130])
    return pose


def _clips(solver):
    clips = [biped.gait_clip(solver, biped.Stride("walk", .7, .6, .65, .11, bob=.006, nod=.6)),
             biped.gait_clip(solver, biped.Stride("trot", 1.45, .4, .45, .15, bob=.012, wing=5))]
    feeding = {}
    for name, height in (("eat", .075), ("drink", .035)):
        effort = bisect_effort(lambda value: muzzle(solver, _feeding(solver, value))[2], height, name)
        feeding[name] = _feeding(solver, effort)
    postures = {"idle": _one_leg(solver), "rest": _one_leg(solver, True),
                "sleep": _one_leg(solver, True), **feeding}
    for name in ("idle", "eat", "drink", "rest", "sleep", "vocalize"):
        def pose(phase, clip_name=name):
            angle = math.tau * phase
            result = biped.copy_pose(postures[clip_name]) if clip_name in postures else biped.standing(solver)
            if clip_name == "idle":
                result.r("head", rz=12 * math.sin(angle))
            elif clip_name in ("eat", "drink"):
                result.r("neck1", rz=7 * math.sin(angle))
            elif clip_name == "vocalize":
                angles = [85 - 2 * index for index in range(len(solver.rig.neck))]
                biped.sagittal_profile(solver, result, solver.rig.neck, angles)
                result.r("head", rz=35 * math.sin(2 * angle))
            return result
        clips.append(Clip(name, 5, pose))
    return clips


def make_spec():
    bones, rig = _skeleton()
    return Spec("flamingo", bones, _parts(bones), paint=paint, voxel=.0045,
                target_tris=8000, smooth_iters=3, rig=rig, clips=_clips, key_all_locations=True,
                meta={"common_name": "Greater flamingo", "scientific_name": "Phoenicopterus roseus",
                      "shoulder_height_m": 1.4, "body_length_m": 1.2, "roughness": .89})
