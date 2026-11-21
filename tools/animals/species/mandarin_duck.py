# SPDX-License-Identifier: MIT
"""Aix galericulata. Original lofted waterfowl with webbed feet and folded wings."""
import math
from functools import partial

import numpy as np

from zoogen import biped, noise
from zoogen.geom import Bone, Part, R, Spec
from zoogen.rig import Clip, Pose, Rig


def _paint(ctx, female):
    flecks = noise.fbm(ctx.P, 110, 2, seed=71)
    colour = noise.mix([.32, .25, .17], [.56, .45, .32], flecks)
    flank = noise.smoothstep(.018, .079, np.abs(ctx.y))
    if not female:
        breast = noise.smoothstep(-.02, .08, ctx.x)
        colour = noise.mix([.70, .48, .26], [.29, .15, .28], breast)
        bars = np.sin(95 * ctx.x + 19 * ctx.z) > .96
        colour = noise.mix(colour, [.90, .77, .59], flank * bars * (ctx.x < .025))
    belly = noise.smoothstep(.13, .06, ctx.z)
    colour = noise.mix(colour, [.85, .81, .69], belly)
    head = ctx.tag("head")
    eye_line = noise.smoothstep(.017, .008, np.abs(ctx.z - (.287 + .18 * (ctx.x - .13))))
    if female:
        face = noise.mix([.40, .37, .32], [.81, .78, .69], eye_line)
    else:
        face = noise.mix([.74, .39, .16], [.11, .32, .24], noise.smoothstep(.277, .304, ctx.z))
        face = noise.mix(face, [.95, .87, .69], eye_line)
    colour = noise.mix(colour, face, head)
    feather = noise.mix([.19, .16, .13], [.48, .43, .36], flecks) if female else noise.mix(
        [.13, .24, .20], [.31, .39, .24], flecks)
    colour = noise.mix(colour, feather, ctx.tag("wing"))
    colour = noise.mix(colour, [.82, .39, .10], ctx.tag("sail"))
    colour = noise.mix(colour, [.57, .33, .22] if female else [.78, .21, .14], ctx.tag("bill"))
    colour = noise.mix(colour, [.78, .46, .20], ctx.tag("foot"))
    return noise.mix(colour, [.027, .024, .016], ctx.tag("eye"))


def _clips(solver):
    clips = [biped.gait_clip(solver, stride) for stride in (
        biped.Stride("walk", .34, .60, .58, .023, bob=.003, roll=3, nod=3),
        biped.Stride("trot", .63, .41, .52, .033, bob=.005, roll=4, nod=4))]
    def pose(name, u):
        a = math.tau * u
        p = biped.standing(solver)
        if name == "swim":
            p = Pose().move("hips", dz=.004 * math.sin(a))
            for side, sign in (("L", 1), ("R", -1)):
                p.r("thigh." + side, ry=38 + 20 * sign * math.sin(a))
                p.r("foot." + side, ry=-23 - 26 * sign * math.sin(a))
        elif name in ("eat", "drink"):
            # The upper bill dips into the low pan while the webbed feet stay planted.
            neck = 48 if name == "eat" else 58
            p.r("hips", ry=8).r("neck", ry=neck + 4 * math.cos(a)).r("head", ry=4)
            p.r("jaw", ry=4 + 2 * math.sin(2 * a))
        elif name in ("sleep", "rest", "preen"):
            p.r("neck", rz=90 if name == "rest" else 120, ry=10)
            p.r("head", ry=22 + 3 * math.sin(a))
        else:
            p.r("head", rz=9 * math.sin(a), ry=3 * math.cos(a))
            if name == "vocalize":
                p.r("jaw", ry=7 + 6 * math.sin(a))
        return p
    for name, duration in (("idle", 4), ("rest", 7), ("sleep", 8), ("eat", 3), ("drink", 3.5),
                           ("vocalize", 2), ("preen", 5), ("swim", 1.1)):
        clips.append(Clip(name, duration, partial(pose, name), speed=.36 if name == "swim" else 0))
    return clips


def make_spec(female=False):
    def bone(name, parent, head, tail):
        return Bone(name, parent, np.array(head), np.array(tail))
    bones = [bone("root", None, (0,0,0), (.03,0,0)),
             bone("hips", "root", (-.06,0,.145), (.06,0,.17)),
             bone("neck", "hips", (.065,0,.17), (.13,0,.269)),
             bone("head", "neck", (.13,0,.269), (.20,0,.284)),
             bone("jaw", "head", (.17,0,.267), (.241,0,.256)),
             bone("wing.L", "hips", (.034,.057,.180), (-.154,.069,.146))]
    legs = biped.add_leg(bones, "hips", [np.array(x) for x in [
        (-.028,.044,.125), (.013,.047,.088), (-.033,.049,.057),
        (-.018,.049,.009), (.029,.050,.007)]])
    parts = []
    def loft(name, joint, rows, tag, mirror=False, fuse=True):
        parts.append(Part(name, [R((x,y,z),w,h) for x,y,z,w,h in rows], weights={joint:1},
                          tag=tag, mirror=mirror, fuse=fuse, segments=18, subdiv=3))
    loft("body","hips",[(-.243,0,.180,.013,.007),(-.176,0,.157,.047,.032),
         (-.09,0,.146,.081,.063),(.010,0,.144,.088,.077),(.079,0,.161,.061,.069),
         (.104,0,.178,.038,.045)],"body")
    loft("neck","neck",[(.059,0,.177,.048,.047),(.086,0,.206,.050,.047),
         (.110,0,.242,.042,.043),(.134,0,.268,.041,.040)],"head")
    loft("head","head",[(.045,0,.265,.023,.013),(.09,0,.282,.043,.035),
         (.132,0,.287,.052,.043),(.17,0,.284,.039,.034),(.19,0,.276,.027,.021)],"head")
    loft("bill","head",[(.176,0,.272,.024,.012),(.199,0,.265,.022,.010),
         (.226,0,.258,.019,.007),(.244,0,.256,.012,.005)],"bill",fuse=False)
    loft("lower_bill","jaw",[(.178,0,.260,.020,.005),(.218,0,.251,.018,.004),
         (.240,0,.250,.010,.003)],"bill",fuse=False)
    loft("wing.L","wing.L",[(-.212,.038,.169,.009,.009),(-.15,.058,.169,.021,.025),
         (-.069,.074,.171,.023,.041),(.010,.068,.165,.025,.044),
         (.065,.052,.174,.017,.022)],"wing",True,False)
    if not female:
        loft("sail.L","wing.L",[(-.163,.062,.228,.010,.014),(-.145,.068,.224,.009,.040),
             (-.122,.073,.211,.008,.049),(-.096,.076,.186,.007,.024)],"sail",True,False)
    for name, joint, a, b, radius in [
            ("thigh.L", "thigh.L", (-.028,.044,.125),(.013,.047,.088),.016),
            ("shin.L","shin.L",(.013,.047,.088),(-.033,.049,.057),.011),
            ("shank.L","foot.L",(-.033,.049,.057),(-.018,.049,.009),.005)]:
        loft(name,joint,[(*a,radius,radius),(*b,radius*.83,radius*.83)],"foot",True,False)
    parts += [biped.webbed_foot(.004,.049,.079,.025,.006),
              biped.eye_part((.152,.046,.291),.006)]
    rig = Rig(body="hips", legs=legs, neck=["neck"], head="head", jaw="jaw",
              extra={"wings":{"L":["wing.L"],"R":["wing.R"]}})
    meta = dict(common_name="Mandarin duck",scientific_name="Aix galericulata",body_length_m=.49,
                shoulder_height_m=.33,roughness=.84,swim={"clip":"swim","depth_m":.16,"speed":.36},
                activity_clips={"preen":"preen"})
    if female:
        meta.update(variant_of="mandarin_duck",variant="female",relative_height=.96)
    return Spec(name="mandarin_duck_female" if female else "mandarin_duck", bones=bones,parts=parts,rig=rig,
                clips=_clips,paint=partial(_paint,female=female),meta=meta,voxel=.003,
                target_tris=10500,texture_size=1024,key_all_locations=True)
