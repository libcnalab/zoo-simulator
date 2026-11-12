# SPDX-License-Identifier: MIT
"""Adult southern white rhinoceros, 1.8 m at the shoulder and 3.8 m body length.

The broad barrel is low slung; the head is carried below the shoulder and the
nuchal hump rises behind it. Length excludes the tail.
"""
import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Bone, Part, R, Spec
from zoogen.anatomy import round_foot
from zoogen.rig import Leg, Rig

YF, YH = 0.36, 0.34


def _bones():
    B = Bone
    return [
        B("root", None, (0, 0, 0), (0.2, 0, 0), deform=False),
        B("hips", "root", (-1.42, 0, 1.12), (-1.05, 0, 1.12)),
        B("spine1", "hips", (-1.05, 0, 1.12), (-0.52, 0, 1.12)),
        B("spine2", "spine1", (-0.52, 0, 1.12), (0.12, 0, 1.15)),
        B("chest", "spine2", (0.12, 0, 1.15), (0.65, 0, 1.20)),
        B("neck1", "chest", (0.65, 0, 1.20), (1.08, 0, 1.20)),
        B("neck2", "neck1", (1.08, 0, 1.20), (1.43, 0, 1.12)),
        B("head", "neck2", (1.43, 0, 1.12), (1.92, 0, 1.06)),
        B("jaw", "head", (1.72, 0, 0.99), (2.08, 0, 0.98)),
        B("ear.L", "head", (1.35, .20, 1.31), (1.27, .25, 1.53)),
        B("tail1", "hips", (-1.42, 0, 1.12), (-1.68, 0, .94)),
        B("tail2", "tail1", (-1.68, 0, .94), (-1.83, 0, .78)),
        B("tail3", "tail2", (-1.83, 0, .78), (-1.89, 0, .64)),
        B("tail4", "tail3", (-1.89, 0, .64), (-1.94, 0, .55)),
        B("scapula.L", "chest", (.38, YF, 1.18), (.42, YF, .90)),
        B("upper_arm.L", "scapula.L", (.42, YF, .90), (.52, YF, .58)),
        B("forearm.L", "upper_arm.L", (.52, YF, .58), (.42, YF, .28)),
        B("hand.L", "forearm.L", (.42, YF, .28), (.42, YF, .09)),
        B("fingers.L", "hand.L", (.42, YF, .09), (.58, YF, .05)),
        B("thigh.L", "hips", (-1.15, YH, 1.10), (-1.02, YH, .69)),
        B("shin.L", "thigh.L", (-1.02, YH, .69), (-1.15, YH, .35)),
        B("foot.L", "shin.L", (-1.15, YH, .35), (-1.15, YH, .09)),
        B("toes.L", "foot.L", (-1.15, YH, .09), (-.99, YH, .05)),
    ]


def _chain(bones, *names):
    by = {b.name: b for b in bones}
    return [(n, by[n].head, by[n].tail) for n in names]


def _parts(b):
    spine = _chain(b, "hips", "spine1", "spine2", "chest")
    neck = _chain(b, "chest", "neck1", "neck2", "head")
    front = _chain(b, "scapula.L", "upper_arm.L", "forearm.L", "hand.L", "fingers.L")
    hind = _chain(b, "thigh.L", "shin.L", "foot.L", "toes.L")
    tail = _chain(b, "hips", "tail1", "tail2", "tail3", "tail4")
    return [
        Part("barrel", [R((-1.55,0,1.13),.12,.18,.18), R((-1.32,0,1.10),.43,.48,.50),
             R((-.82,0,1.08),.56,.57,.60), R((-.18,0,1.10),.59,.58,.61),
             R((.40,0,1.16),.55,.56,.62), R((.78,0,1.22),.40,.48,.56)],
             chain=spine, segments=32, cap_scale=.55),
        Part("hump", [R((.25,0,1.38),.42,.38,.35), R((.52,0,1.50),.37,.42,.38),
             R((.78,0,1.40),.28,.34,.30)], chain=spine[2:], segments=24, tag="hump"),
        Part("neck", [R((.58,0,1.23),.34,.42,.40), R((.95,0,1.23),.30,.36,.37),
             R((1.30,0,1.16),.27,.31,.33)], chain=neck, segments=24),
        Part("head", [R((1.25,0,1.16),.28,.30,.29), R((1.52,0,1.10),.30,.30,.29),
             R((1.78,0,1.02),.34,.24,.25), R((2.00,0,1.00),.34,.18,.20,n=3.0),
             R((2.19,0,1.00),.25,.14,.15,n=3.2)], weights={"head":1.0}, segments=24),
        Part("jaw", [R((1.67,0,.91),.23,.11,.13),R((1.96,0,.91),.30,.12,.13,n=3),
             R((2.16,0,.93),.23,.10,.11,n=3)], weights={"jaw":1.0}, tag="jaw"),
        Part("horn.front", [R((2.02,0,1.17),.16,.13,.13),R((2.22,0,1.30),.12,.12,.12),
             R((2.42,0,1.48),.055,.055,.055),R((2.47,0,1.58),.008,.008,.008)],
             weights={"head":1.0}, fuse=False, tag="horn", segments=16, cap_start="flat"),
        Part("horn.back", [R((1.73,0,1.22),.12,.10,.10),R((1.86,0,1.33),.08,.08,.08),
             R((1.97,0,1.40),.006,.006,.006)], weights={"head":1.0}, fuse=False,
             tag="horn", segments=14, cap_start="flat"),
        Part("ear.L", [R((1.35,.20,1.30),.12,.10,.12),R((1.28,.27,1.49),.12,.105,.12),
             R((1.23,.30,1.64),.075,.08,.085)], weights={"ear.L":1.0}, mirror=True,
             segments=14, tag="ear", cap_end="flat"),
        Part("frontleg.L", [R((.40,YF,1.12),.27,.29,.29),R((.45,YF,.88),.24,.28,.28),
             R((.45,YF,.57),.20,.23,.23),R((.43,YF,.29),.17,.19,.19),
             R((.48,YF,.10),.22,.12,.12),R((.59,YF,.055),.23,.09,.09,n=3)],
             chain=front, mirror=True, segments=20, radius_scale=1.15),
        Part("hindleg.L", [R((-1.20,YH,1.08),.27,.28,.27),R((-1.00,YH,.82),.24,.25,.24),
             R((-.96,YH,.68),.20,.22,.22),R((-1.11,YH,.37),.16,.18,.18),
             R((-1.07,YH,.10),.21,.12,.12),R((-.91,YH,.055),.22,.09,.09,n=3)],
             chain=hind, mirror=True, segments=20, radius_scale=1.15),
        Part("tail", [R((-1.43,0,1.10),.08,.08),R((-1.68,0,.84),.06,.06),
             R((-1.83,0,.55),.045,.045),R((-1.89,0,.30),.035,.035)], chain=tail, segments=12),
    ]



def _feet(parts):
    for front, x, y in ((True, .44, YF), (False, -1.10, YH)):
        label = "front" if front else "hind"
        leg = next(part for part in parts if part.name == label + "leg.L")
        leg.rings = leg.rings[:-2] + [R((x, y, .19), .18, .20), R((x, y, .10), .19, .20)]
        bone = "fingers.L" if front else "toes.L"
        parts.append(round_foot(label + "_sole.L", x, y, .21, .18, bone))
        for digit in range(3):
            toe_y = y + (digit - 1) * .13
            parts.append(Part(f"{label}_toe{digit}.L", [
                R((x + .13, toe_y, .066), .068, .060),
                R((x + .23, toe_y, .064), .064, .055)],
                weights={bone: 1.0}, mirror=True, segments=12, tag="toe", cap_scale=.25))
    return parts

def paint(ctx):
    P, N = ctx.P, ctx.N
    grey = np.array([.36,.36,.33])
    crease = nz.fbm(P, 10.0, 3, seed=41)
    coat = grey * (.92 + .12 * nz.fbm(P, 48.0, 3, seed=7))[...,None]
    # Two local skin folds replace the periodic bands that looked like ribs.
    folds = np.exp(-((ctx.x - .55) / .075) ** 2) + np.exp(-((ctx.x + 1.14) / .085) ** 2)
    folds *= nz.smoothstep(.3, .8, np.abs(N[..., 1]))
    coat *= (1.0 - .08 * folds * (.5 + .5 * crease))[..., None]
    coat = nz.mix(coat, np.array([.72,.69,.60]), ctx.tag("horn"))
    coat = nz.mix(coat, np.array([.22,.21,.19]), ctx.tag("ear") * .25)
    ear_rim = ctx.tag("ear") * nz.smoothstep(1.53, 1.63, ctx.z)
    ear_rim *= nz.smoothstep(.15, .75, ctx.N[..., 2])
    coat = nz.mix(coat, np.array([.12, .11, .095]), ear_rim)
    coat = nz.mix(coat, np.array([.29, .28, .24]), ctx.tag("toe") * .65)
    return coat


def _rig():
    return Rig(body="hips", legs={"FL":Leg("upper_arm.L","forearm.L","hand.L","fingers.L","scapula.L"),
        "FR":Leg("upper_arm.R","forearm.R","hand.R","fingers.R","scapula.R"),
        "HL":Leg("thigh.L","shin.L","foot.L","toes.L"),
        "HR":Leg("thigh.R","shin.R","foot.R","toes.R")}, spine=["spine1","spine2"],
        chest="chest", neck=["neck1","neck2"], head="head", jaw="jaw",
        tail=["tail1","tail2","tail3","tail4"], ears=["ear.L","ear.R"],
        extra={"muzzle":("head",(2.32,0,1.0))})


def _clips(solver):
    return quad.standard_clips(solver, quad.Params(gaits=[
        quad.Gait("walk",1.35,1.00,.68,quad.WALK,lift=.055,bob=.008,hip_yaw=2,chest_yaw=1.5),
        quad.Gait("trot",1.35,1.20,.40,quad.TROT,lift=.08,bob=-.004,hip_yaw=2,
                 chest_yaw=1.5,swing_pitch=52)],
        lie=quad.Lie(drop=.54,hind_tuck=.08,hind_out=.07),
        head_down=quad.HeadDown(body_pitch=12,crouch=.05,chest=6,neck=(24,22),head=10),
        look_yaw=10, tail_idle=5))


def make_spec():
    bones = _bones()
    return Spec(name="rhino", bones=bones, parts=_feet(_parts(bones)), paint=paint,
        texture_size=1024, voxel=.018, target_tris=18000, smooth_iters=3,
        meta={"common_name":"Southern white rhinoceros","scientific_name":"Ceratotherium simum simum",
              "shoulder_height_m":1.8,"roughness":.93}, rig=_rig(), clips=_clips)
