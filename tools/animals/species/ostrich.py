# SPDX-License-Identifier: MIT
"""Male common ostrich: 2.4 m standing head height, about 2.1 m head and body.

The data's shoulderHeightM and meta.shoulder_height_m both mean the top of the
head. The horizontal trunk rides at 1.25 m; the four-segment bare neck rises in
an S. Flight feathers are painted on folded wings, never individual meshes.
"""
import math
from functools import partial

import numpy as np

from zoogen import activity, biped, noise as nz
from zoogen.geom import Bone, Part, R, Spec
from zoogen.quadruped import bisect_effort, muzzle
from zoogen.rig import Clip, Rig

NECK = [(.40, 0, 1.43), (.53, 0, 1.65), (.49, 0, 1.87),
        (.53, 0, 2.09), (.65, 0, 2.30)]


def _skeleton():
    bones = [Bone("root", None, (0, 0, 0), (.3, 0, 0), False),
             Bone("hips", "root", (-.18, 0, 1.22), (.05, 0, 1.26)),
             Bone("chest", "hips", (.05, 0, 1.26), (.40, 0, 1.43))]
    neck = biped.add_chain(bones, "neck", "chest", NECK)
    bones.append(Bone("head", neck[-1], NECK[-1], (.92, 0, 2.31)))
    legs = biped.add_leg(bones, "hips", [(-.18, .17, 1.22), (-.46, .17, .66),
        (-.12, .17, .095), (.00, .17, .045), (.24, .17, .035)])
    bones += [Bone("wing.L", "chest", (.15, .30, 1.40), (-.68, .37, 1.25)),
              Bone("tail1", "hips", (-.80, 0, 1.37), (-1.21, 0, 1.40))]
    return bones, Rig("hips", legs, chest="chest", neck=neck, head="head", tail=["tail1"],
        extra={"muzzle": ("head", (.98, 0, 2.31)),
               "wings": {side: ["wing." + side] for side in ("L", "R")}})


def _parts(bones):
    parts = [Part("egg_body", [
        R((-1.00, 0, 1.27), .09, .10), R((-.79, 0, 1.27), .28, .26, .24),
        R((-.45, 0, 1.27), .39, .34, .32), R((-.10, 0, 1.27), .40, .35, .34),
        R((.20, 0, 1.29), .33, .31, .32), R((.43, 0, 1.34), .18, .20, .23),
    ], chain=biped.chain(bones, "hips", "chest"), segments=36, rough=(.008, 22), cap_scale=.4),
        Part("bare_neck", [R(NECK[0], .105, .13), R(NECK[1], .061, .068),
              R(NECK[2], .048, .052), R(NECK[3], .045, .048), R(NECK[4], .049, .053)],
             chain=biped.chain(bones, "neck1", "neck2", "neck3", "neck4", "head"),
             segments=24, tag="bare", cap_start="flat", cap_end="flat"),
        Part("skull", [R((.60, 0, 2.325), .043, .050, .048),
            R((.69, 0, 2.33), .074, .069, .056), R((.78, 0, 2.315), .057, .042, .037)],
             weights={"head": 1}, segments=24, tag="bare", cap_scale=.3),
        Part("broad_bill", [R((.77, 0, 2.31), .057, .019),
             R((.88, 0, 2.31), .060, .016), R((.98, 0, 2.305), .032, .012)],
             weights={"head": 1}, segments=20, tag="beak", cap_scale=.25),
        biped.eye_part((.722, .060, 2.347), .017),
        Part("folded_wing.L", [R((.20, .25, 1.39), .040, .075),
            R((-.10, .37, 1.28), .045, .13), R((-.45, .37, 1.22), .045, .13),
            R((-.72, .31, 1.22), .025, .08)], weights={"wing.L": 1},
             mirror=True, fuse=False, segments=28, tag="wing", rough=(.009, 28), cap_scale=.4),
        Part("tail_plume", [R((-.80, 0, 1.37), .15, .13), R((-1.02, 0, 1.42), .24, .16),
             R((-1.23, 0, 1.38), .22, .13), R((-1.40, 0, 1.31), .085, .055)],
             weights={"tail1": 1}, segments=24,
             tag="plume", rough=(.025, 18), cap_scale=.4),
        Part("leg.L", [R((-.18, .17, 1.22), .075, .10), R((-.29, .17, .98), .073, .082),
            R((-.46, .17, .66), .044, .052), R((-.32, .17, .42), .028, .032),
            R((-.12, .17, .095), .026, .031)],
             chain=biped.chain(bones, "thigh.L", "shin.L", "foot.L"), mirror=True,
             segments=20, tag="bare", cap_start="flat"),
        Part("ankle.L", [R((-.12, .17, .095), .030, .028), R((0, .17, .043), .045, .032)],
             weights={"foot.L": 1}, mirror=True, segments=16, tag="bare")]
    for index, (dy, length, width) in enumerate(((0, .27, .036), (.075, .17, .023))):
        parts.append(Part(f"digit{index}.L", [R((-.015, .17 + dy * .3, .034), width, .027),
            R((length * .55, .17 + dy, .030), width, .023),
            R((length, .17 + dy, .026), width * .6, .019)],
            weights={"toes.L": 1}, mirror=True, segments=16, tag="digit", cap_scale=.25))
    return parts


def paint(ctx):
    black = np.array([.027, .025, .026])
    white = np.array([.84, .82, .73])
    grain = nz.fbm(ctx.P * [1, 1, 4], 40, 2, seed=61)
    color = black * (.8 + .5 * grain)[..., None]
    scallop = .96 + .04 * grain
    wing_white = ctx.tag("wing") * nz.smoothstep(-.28, -.53, ctx.x)
    color = nz.mix(color, white * scallop[..., None], np.maximum(wing_white, ctx.tag("plume")))
    bare = np.array([.52, .36, .33]) * (.91 + .13 * grain)[..., None]
    color = nz.mix(color, bare, np.maximum(ctx.tag("bare"), ctx.tag("digit")))
    claw = ctx.tag("digit") * nz.smoothstep(.21, .25, ctx.x)
    color = nz.mix(color, np.array([.15, .11, .08]), claw)
    color = nz.mix(color, np.array([.58, .48, .37]), ctx.tag("beak"))
    color = nz.mix(color, np.array([.045, .025, .012]), ctx.tag("eye"))
    return color


def _feeding(solver, effort):
    pose = biped.standing(solver)
    pose.move("hips", dz=-.32 * effort)
    pose.r("hips", ry=25 * effort)
    angles = [60 - 122 * effort, 100 - 168 * effort, 80 - 158 * effort, 60 - 130 * effort]
    biped.sagittal_profile(solver, pose, solver.rig.neck, angles)
    biped.sagittal_profile(solver, pose, ["head"], [-5])
    return pose


def _sitting(solver, asleep):
    pose = biped.standing(solver)
    pose.move("hips", dz=-.72)
    for key in solver.rig.legs:
        target = solver.toe_base(key)
        target[0] = .18
        pose.plant(key, target, pole=(-1, .60 if key == "L" else -.60, 0))
    if asleep:
        biped.sagittal_profile(solver, pose, solver.rig.neck, [130, 160, 190, 205])
        biped.sagittal_profile(solver, pose, ["head"], [175])
    return pose


# Dust bathing: [neck yaw (+ to the left), neck laid low (0 the sitting bird's upright neck,
# 1 along the ground), body roll (+ leans onto the left side), wing lift, wing shaking].
DUST_BATHE = [
    (0.00, (0, 0, 0, 0, 0)),
    (0.08, (55, 1, 9, 22, 1), .08),
    (0.30, (20, .85, 5, 18, .6), .05),
    (0.40, (-55, 1, -9, 22, 1), .08),
    (0.60, (0, .35, 0, 48, 1), .07),
    (0.78, (0, .2, 0, 30, .4), .05),
    (0.86, (0, 0, 0, 0, 0), .08),
]
NECK_LAID = [-35, -60, -35, -10]


def _dust_bathe(solver, sitting, phase):
    """`dust_bathe`: sitting in a dry hollow, the bird rocks from side to side, lays its neck
    along the ground and sweeps it to throw sand over its flank, and shakes its loosened wings."""
    angle = math.tau * phase
    yaw, low, roll, lift, shake = activity.held(phase, DUST_BATHE)
    pose = biped.copy_pose(sitting)
    pose.r("hips", rx=-roll)
    neutral = [math.degrees(math.atan2(b[2] - a[2], b[0] - a[0])) for a, b in zip(NECK, NECK[1:])]
    biped.sagittal_profile(solver, pose, solver.rig.neck,
                           [a + (b - a) * low for a, b in zip(neutral, NECK_LAID)])
    biped.sagittal_profile(solver, pose, ["head"], [-1 - 9 * low])
    # The neck swings about the upright through its base, sweeping the sand while it is low.
    sweep = yaw + 12 * low * math.sin(6 * angle)
    activity.turn_about_vertical(solver, pose, "neck1", sweep)
    pose.r("head", rz=-.3 * sweep)
    for side, sign in (("L", 1), ("R", -1)):
        # The wing bone points back along the flank: turning it about the upright swings the
        # folded wing out from the body, pitching it raises its tip; shaking rattles both.
        flutter = 9 * shake * math.sin(24 * angle + sign)
        pose.r("wing." + side, rz=-sign * (lift + flutter), ry=.35 * lift + 5 * shake * math.sin(12 * angle))
    return pose


def _clips(solver):
    clips = [biped.gait_clip(solver, biped.Stride("walk", 1.2, .8, .64, .13,
                   bob=.012, nod=1.2)),
             biped.gait_clip(solver, biped.Stride("trot", 3.2, .5, .40, .24,
                   bob=.035, lean=4, wing=16))]
    feeding = {}
    for name, height in (("eat", .075), ("drink", .035)):
        effort = bisect_effort(lambda value: muzzle(solver, _feeding(solver, value))[2], height, name)
        feeding[name] = _feeding(solver, effort)
    postures = {"rest": _sitting(solver, False), "sleep": _sitting(solver, True), **feeding}
    for name in ("idle", "eat", "drink", "rest", "sleep", "vocalize"):
        def pose(phase, clip_name=name):
            angle = math.tau * phase
            result = biped.copy_pose(postures[clip_name]) if clip_name in postures else biped.standing(solver)
            if clip_name == "idle":
                result.r("head", rz=18 * math.sin(angle))
                result.r("neck4", ry=2 * math.sin(2 * angle))
            elif clip_name in ("eat", "drink"):
                result.r("head", rz=4 * math.sin(angle))
            elif clip_name == "vocalize":
                result.r("neck1", ry=-8)
                result.r("neck4", ry=-8)
                result.r("head", ry=-12)
                for side, sign in (("L", 1), ("R", -1)):
                    result.r("wing." + side, rx=sign * (25 + 4 * math.sin(angle)))
            return result
        clips.append(Clip(name, 4, pose))
    clips.append(Clip("dust_bathe", 10, partial(_dust_bathe, solver, postures["rest"])))
    return clips


def make_spec():
    bones, rig = _skeleton()
    return Spec("ostrich", bones, _parts(bones), paint=paint, voxel=.009,
                target_tris=14000, smooth_iters=4, rig=rig, clips=_clips, key_all_locations=True,
                meta={"common_name": "Common ostrich", "scientific_name": "Struthio camelus",
                      "shoulder_height_m": 2.4, "body_length_m": 2.1, "sex": "male", "roughness": .93,
                      # The game's clip for a still activity (AnimalRenderer).
                      "activity_clips": {"dust_bathe": "dust_bathe"}})
