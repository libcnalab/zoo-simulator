# SPDX-License-Identifier: MIT
"""African penguin: 0.65 m standing to the head top, also the metadata height.

The data's shoulderHeightM means standing head height for this biped. The upright
body is about 0.32 m deep, with a 0.12 m stiff tail and short rear-set legs.
"""
import math

import numpy as np

from zoogen import biped, noise as nz
from zoogen.geom import Bone, Part, R, Spec
from zoogen.rig import Clip, Rig


def _skeleton():
    bones = [Bone("root", None, (0, 0, 0), (.1, 0, 0), False),
             Bone("hips", "root", (-.045, 0, .16), (0, 0, .32)),
             Bone("chest", "hips", (0, 0, .32), (.015, 0, .48)),
             Bone("neck1", "chest", (.015, 0, .48), (.04, 0, .555)),
             Bone("head", "neck1", (.04, 0, .555), (.145, 0, .57)),
             Bone("tail1", "hips", (-.085, 0, .15), (-.19, 0, .025))]
    legs = biped.add_leg(bones, "hips", [(-.045, .075, .16), (.015, .075, .095),
        (-.055, .075, .035), (.015, .075, .025), (.10, .075, .022)])
    bones += [Bone("flipper1.L", "chest", (0, .105, .44), (-.02, .175, .31)),
              Bone("flipper2.L", "flipper1.L", (-.02, .175, .31), (-.06, .20, .18))]
    rig = Rig("hips", legs, chest="chest", neck=["neck1"], head="head", tail=["tail1"],
              extra={"muzzle": ("head", (.223, 0, .565)),
                     "wings": {side: ["flipper1." + side, "flipper2." + side] for side in ("L", "R")}})
    return bones, rig


def _parts(bones):
    return [Part("continuous_body", [
        R((-.045, 0, .085), .065, .065, .08),
        R((-.035, 0, .13), .095, .095, .11),
        R((-.025, 0, .20), .115, .125, .14),
        R((-.005, 0, .31), .125, .13, .155),
        R((.005, 0, .415), .10, .11, .13),
        R((.02, 0, .49), .064, .065, .075),
        R((.043, 0, .56), .072, .065, .075),
        R((.043, 0, .61), .058, .053, .057),
        R((.04, 0, .64), .025, .025, .026),
    ], chain=biped.chain(bones, "hips", "chest", "neck1", "head"), segments=32,
        cap_scale=.22),
        Part("bill", [R((.09, 0, .575), .039, .031), R((.16, 0, .57), .032, .025),
                      R((.218, 0, .565), .007, .009)], weights={"head": 1}, tag="beak",
             segments=20, cap_scale=.3),
        biped.eye_part((.078, .062, .593), .008),
        Part("flipper.L", [R((0, .10, .435), .055, .020),
              R((-.015, .15, .365), .063, .016), R((-.035, .19, .26), .042, .011),
              R((-.06, .20, .18), .011, .006)],
             chain=biped.chain(bones, "flipper1.L", "flipper2.L"),
             side_ref=(1, 0, 0), mirror=True, segments=20, fuse=False, tag="flipper"),
        Part("leg.L", [R((-.045, .075, .17), .033, .042), R((.015, .075, .095), .026, .03),
                       R((-.055, .075, .035), .02, .022)],
             chain=biped.chain(bones, "thigh.L", "shin.L", "foot.L"), mirror=True,
             segments=16, tag="leg"),
        biped.webbed_foot(.015, .075, .12, .047, .016),
        Part("tail", [R((-.07, 0, .16), .058, .045), R((-.14, 0, .08), .035, .022),
                      R((-.195, 0, .019), .014, .008)],
             weights={"tail1": 1}, segments=18, tag="tail", cap_scale=.2)]


def paint(ctx):
    black = np.array([.025, .031, .035])
    white = np.array([.88, .89, .83])
    front = nz.smoothstep(.015, .075, ctx.x)
    belly = front * nz.smoothstep(.52, .45, ctx.z)
    color = nz.mix(black, white, belly)
    # One inverted U band across the upper chest, descending along the flanks.
    band_height = .405 - .16 * (np.abs(ctx.y) / .125) ** 2
    band = nz.smoothstep(.022, .011, np.abs(ctx.z - band_height)) * front
    color = nz.mix(color, black, band * nz.smoothstep(.17, .23, ctx.z))
    for y, z in ((.034, .28), (-.055, .32), (.063, .23), (-.015, .20), (.012, .345)):
        spot = ((ctx.y - y) / .009) ** 2 + ((ctx.z - z) / .011) ** 2
        color = nz.mix(color, black, nz.smoothstep(1.2, .6, spot) * front)
    # White facial ribbon from above the eye around the cheek into the throat.
    cheek = ((ctx.x - .026) / .068) ** 2 + ((ctx.z - .565) / .066) ** 2
    ribbon = nz.smoothstep(.24, .42, cheek) * nz.smoothstep(1.1, .85, cheek)
    ribbon *= nz.smoothstep(.035, .055, np.abs(ctx.y))
    color = nz.mix(color, white, ribbon)
    pink = ((ctx.x - .077) / .020) ** 2 + ((ctx.z - .607) / .010) ** 2
    color = nz.mix(color, np.array([.73, .40, .42]), nz.smoothstep(1.3, .6, pink))
    for tag in ("beak", "foot", "flipper", "tail", "eye"):
        color = nz.mix(color, black * (.35 if tag == "eye" else 1.0), ctx.tag(tag))
    color *= (.96 + .07 * nz.fbm(ctx.P, 160, 2, seed=51))[..., None]
    return color


def _pose(solver, name, phase):
    angle = math.tau * phase
    pose = biped.standing(solver)
    if name == "idle":
        pose.r("head", rz=12 * math.sin(angle), ry=2 * math.sin(2 * angle))
        for side, sign in (("L", 1), ("R", -1)):
            pose.r("flipper1." + side, rx=sign * 3 * math.sin(2 * angle))
    elif name in ("eat", "drink"):
        pose.move("hips", dz=-.020)
        pose.r("hips", ry=64)
        pose.r("neck1", ry=14)
        pose.r("head", ry=13 + math.sin(4 * angle))
    elif name == "rest":
        pose.move("hips", dz=-.012)
        pose.r("hips", ry=70)
        pose.r("neck1", ry=-50)
        pose.r("head", ry=-20, rz=3 * math.sin(angle))
    elif name == "sleep":
        pose.r("neck1", ry=15, rz=35)
        pose.r("head", ry=40, rz=60)
    elif name == "vocalize":
        pose.r("neck1", ry=-45)
        pose.r("head", ry=-48)
        for side, sign in (("L", 1), ("R", -1)):
            pose.r("flipper1." + side, rx=sign * (58 + 12 * math.sin(2 * angle)))
    return pose


def _clips(solver):
    clips = [biped.gait_clip(solver, biped.Stride("walk", .5, .40, .64, .026,
                 bob=.003, roll=5, wing=12, nod=1)),
             biped.gait_clip(solver, biped.Stride("trot", 1.05, .30, .43, .037,
                 bob=.006, roll=6, lean=12, wing=28))]
    for name in ("idle", "eat", "drink", "rest", "sleep", "vocalize"):
        def pose(phase, clip_name=name):
            return _pose(solver, clip_name, phase)
        clips.append(Clip(name, 4.0, pose))
    return clips


def make_spec():
    bones, rig = _skeleton()
    return Spec("penguin", bones, _parts(bones), paint=paint, voxel=.004,
                target_tris=7000, smooth_iters=4, rig=rig, clips=_clips, key_all_locations=True,
                meta={"common_name": "African penguin", "scientific_name": "Spheniscus demersus",
                      "shoulder_height_m": .65, "body_length_m": .65, "roughness": .88})
