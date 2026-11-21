# SPDX-License-Identifier: MIT
"""Aldabra giant tortoise. Animal axes are forward, left and up, in metres."""
import math
from functools import partial

import numpy as np

from zoogen import noise, quadruped
from zoogen.geom import Bone, Part, R, Spec
from zoogen.rig import Clip, Leg, Pose, Rig


def _scutes(ctx, scale):
    """The dorsal row, offset costals and smaller marginal shields meet edge to edge."""
    x, y, z = ctx.x / scale, np.abs(ctx.y) / scale, ctx.z / scale
    latitude = np.arctan2(y / .39, np.maximum(.012, (z - .32) / .42))
    first = np.full(x.shape, np.inf)
    second = np.full(x.shape, np.inf)
    for angle, centers in [(0, [-.47, -.26, -.035, .19, .40]),
                           (.78, [-.40, -.15, .105, .35]),
                           (1.45, np.linspace(-.52, .47, 11))]:
        for cx in centers:
            distance = ((x - cx) / (.115 if angle < 1.4 else .072))**2 + ((latitude - angle) / .50)**2
            second = np.minimum(second, np.maximum(first, distance))
            first = np.minimum(first, distance)
    seam = 1 - noise.smoothstep(.025, .18, second - first)
    rings = .5 + .5 * np.cos(np.sqrt(first) * 30)
    return seam, rings


def _colour(ctx, scale):
    seam, rings = _scutes(ctx, scale)
    weather = noise.fbm(ctx.P / scale, 12, 3, seed=51)
    skin = noise.mix([.22, .215, .177], [.47, .435, .35], weather)
    shell = noise.mix([.20, .19, .165], [.35, .328, .269], .64 * weather + .36 * rings)
    shell = noise.mix(shell, [.105, .099, .087], .86 * seam)
    colour = noise.mix(skin, shell, ctx.tag("carapace"))
    colour = noise.mix(colour, [.44, .385, .28], ctx.tag("plastron"))
    colour = noise.mix(colour, [.47, .443, .355], ctx.tag("scale"))
    colour = noise.mix(colour, [.40, .344, .24], ctx.tag("claw"))
    colour = noise.mix(colour, [.145, .12, .083], ctx.tag("beak"))
    return noise.mix(colour, [.032, .026, .021], np.maximum(ctx.tag("eye"), ctx.tag("nostril")))


def _relief(ctx, scale):
    seam, rings = _scutes(ctx, scale)
    grain = noise.fbm(ctx.P / scale, 105, 3, seed=24)
    shell = .52 + .12 * (rings - .5) - .33 * seam + .10 * (grain - .5)
    return grain * (1 - ctx.tag("carapace")) + shell * ctx.tag("carapace")


def _clips(solver, scale):
    clips = []
    # The faster slot is another four-beat walk. The shell and girdles move as one.
    for name, duration, speed in [("walk", 2.5, .11), ("trot", 1.9, .17)]:
        duty = .76
        gait = quadruped.Gait(name, duration, speed * scale * duration * duty, duty,
                              quadruped.WALK, lift=.033 * scale, bob=.006 * scale,
                              hip_yaw=0, chest_yaw=0, sway=.010 * scale, roll=1.1,
                              heel_off=11, swing_pitch=14, head_nod=1.6, tail_sway=0)
        clips.append(quadruped.gait_clip(solver, gait))

    def still(name, phase, reach=0):
        wave = math.sin(math.tau * phase)
        pose = Pose()
        lowered = .012
        if name in ("eat", "drink"):
            lowered += .058 * reach
            pose.r("neck1", ry=61 * reach)
            pose.r("neck2", ry=28 * reach)
            pose.r("head", ry=-13 * reach)
            pose.r("jaw", ry=(6 if name == "eat" else 2) * (.5 + .5 * wave))
        elif name in ("rest", "sleep"):
            lowered = .095 if name == "rest" else .135
            pose.r("neck1", ry=-30, rz=9)
            pose.r("neck2", ry=62)
            pose.r("head", ry=12 + wave)
        elif name == "vocalize":
            pose.r("neck1", ry=-15 + 3 * wave)
            pose.r("jaw", ry=5 + 4 * wave)
        else:
            pose.r("neck2", rz=8 * wave)
            pose.r("head", ry=2 * math.cos(math.tau * phase))
        pose.move("hips", dz=-lowered * scale)
        for key in solver.rig.legs:
            pose.plant(key, solver.toe_base(key))
        return pose

    for name, duration in [("idle", 9), ("rest", 10), ("sleep", 12),
                           ("eat", 4.5), ("drink", 5.2), ("vocalize", 3)]:
        reach = 0
        if name in ("eat", "drink"):
            target = (.045 if name == "eat" else .023) * scale
            reach = quadruped.bisect_effort(
                lambda k: quadruped.muzzle(solver, still(name, .5, k))[2], target, name)
        clips.append(Clip(name, duration, partial(still, name, reach=reach)))
    return clips


def make_spec(juvenile=False, female=False):
    scale = .37 if juvenile else .80 if female else 1.
    dome = .87 if juvenile else 1.
    height = .32 + .424 * dome

    def point(xyz):
        return np.asarray(xyz, dtype=float) * scale

    bones = []

    def bone(name, parent, a, b, deform=True):
        bones.append(Bone(name, parent, point(a), point(b), deform))

    bone("root", None, (0, 0, 0), (.12, 0, 0), False)
    bone("hips", "root", (-.30, 0, .34), (.11, 0, .34))
    bone("chest", "hips", (.11, 0, .34), (.33, 0, .34))
    bone("neck1", "chest", (.32, 0, .34), (.47, 0, .365))
    bone("neck2", "neck1", (.47, 0, .365), (.61, 0, .41))
    bone("head", "neck2", (.61, 0, .41), (.76, 0, .417))
    bone("jaw", "head", (.63, 0, .397), (.772, 0, .378))
    tail_tip = (-.553, 0, .222) if juvenile or female else (-.615, 0, .182)
    bone("tail1", "hips", (-.44, 0, .28), tail_tip)
    parts = []

    def section(xyz, width, upper, lower=None):
        return R(point(xyz), width * scale, upper * scale,
                 (upper if lower is None else lower) * scale)

    def part(name, rings, joints, **options):
        if isinstance(joints, str):
            attachment = dict(weights={joints: 1.})
        else:
            attachment = dict(chain=[(b.name, b.head, b.tail) for b in bones if b.name in joints])
        sections = [section(*r) for r in rings]
        if juvenile and name in ("skull", "jaw", "beak", "eye.L", "nostril.L"):
            pivot = point((.61, 0, .41))
            for ring in sections:
                ring.c = pivot + 1.10 * (ring.c - pivot)
                ring.ry *= 1.16
                ring.up *= 1.16
                ring.dn *= 1.16
        parts.append(Part(name, sections, **attachment, **options))

    # Keep the broad, low rim and ventral plate rigid while the legs extend from underneath.
    part("carapace", [((x, 0, z), width, top * dome, base) for x, z, width, top, base in [
        (-.582, .325, .070, .060, .025), (-.53, .325, .214, .197, .035),
        (-.37, .325, .342, .34, .043), (-.13, .32, .401, .424, .046),
        (.065, .32, .403, .407, .045), (.265, .328, .353, .333, .044),
        (.426, .345, .266, .204, .039), (.517, .353, .154, .068, .025)]],
         "hips", tag="carapace", segments=52, subdiv=5, fuse=False, cap_scale=.48)
    part("plastron", [((x, 0, z), w, .063, d) for x, z, w, d in [
        (-.50, .286, .17, .033), (-.35, .29, .295, .06), (.02, .29, .32, .064),
        (.29, .29, .275, .052), (.44, .302, .16, .038)]],
         "hips", tag="plastron", segments=32, fuse=False)
    part("body", [((-.42, 0, .29), .24, .116), ((-.15, 0, .325), .30, .12),
                  ((.19, 0, .32), .29, .13), ((.35, 0, .31), .23, .11)], "hips")
    part("neck", [((.27, 0, .325), .13, .105), ((.40, 0, .337), .095, .085),
                  ((.53, 0, .373), .078, .078), ((.642, 0, .411), .063, .061)],
         ["chest", "neck1", "neck2", "head"], segments=24)
    part("skull", [((.607, 0, .414), .049, .057), ((.654, 0, .434), .085, .074),
                   ((.709, 0, .435), .079, .060), ((.754, 0, .416), .047, .038)], "head")
    part("beak", [((.710, 0, .414), .066, .039), ((.752, 0, .405), .048, .03),
                  ((.777, 0, .388), .026, .022)], "head", tag="beak", fuse=False)
    part("jaw", [((.641, 0, .384), .055, .036), ((.702, 0, .38), .067, .03),
                 ((.762, 0, .38), .037, .02)], "jaw", cap_scale=.45)
    part("eye.L", [((.699, .076, .455), .012, .012)], "head", tag="eye", mirror=True,
         side_ref=(1, 0, 0), fuse=False, segments=16, cap_scale=.7)
    part("nostril.L", [((.753, .041, .430), .007, .004)], "head", tag="nostril",
         side_ref=(1, 0, 0), mirror=True, fuse=False, segments=10, cap_scale=.5)
    part("tail", [((-.42, 0, .279), .062, .05), ((-.531, 0, .222), .038, .035),
                  (tail_tip, .008, .008)], "tail1", cap_end="point")

    legs = {}
    for end, front in (("fore", True), ("hind", False)):
        x = .28 if front else -.32
        hip = (x, .221, .346)
        knee = (x + (-.075 if front else .105), .325, .202)
        ankle = (x + (.05 if front else -.014), .343, .070)
        toe = (ankle[0] + .047, .35, .041)
        tip = (toe[0] + .071, .35, .025)
        limb = [end + s + ".L" for s in ("_upper", "_lower", "_foot", "_toes")]
        for i, (a, b) in enumerate(zip((hip, knee, ankle, toe), (knee, ankle, toe, tip))):
            bone(limb[i], ("chest" if front else "hips") if i == 0 else limb[i - 1], a, b)
        part(end + "_leg.L", [(hip, .105, .105), (knee, .093, .100),
                            (ankle, .075, .077), (toe, .071, .050)], limb, mirror=True)
        part(end + "_foot.L", [(ankle, .073, .056), (toe, .082, .041), (tip, .055, .024)],
             limb[2:], mirror=True, cap_scale=.35)
        claws = 5 if front else 4
        for i in range(claws):
            y = tip[1] + (i - (claws - 1) * .5) * (.023 if front else .027)
            part(f"{end}_claw_{i}.L",
                 [((tip[0] - .025, y, .038), .012, .015), ((tip[0] + .017, y, .019), .006, .006)],
                 limb[-1], mirror=True, fuse=False, tag="claw", segments=8, cap_end="point")
        if front:
            for row in range(3):
                for i in range(3):
                    t = .22 + .18 * row
                    at = point(knee) * (1 - t) + point(ankle) * t
                    at /= scale
                    at[0] += .060 - .011 * abs(i - 1)
                    at[1] += .031 * (i - 1)
                    part(f"fore_scale_{row}_{i}.L", [(at, .026, .028)], limb[1],
                         mirror=True, fuse=False, tag="scale", segments=10, cap_scale=.32)
        for side in ("L", "R"):
            legs[("F" if front else "H") + side] = Leg(*[n[:-1] + side for n in limb])
    rig = Rig(body="hips", chest="chest", neck=["neck1", "neck2"], head="head",
              jaw="jaw", tail=["tail1"], legs=legs)
    meta = dict(common_name="Aldabra giant tortoise", scientific_name="Aldabrachelys gigantea",
                shoulder_height_m=height * scale, body_length_m=1.40 * scale, roughness=.9)
    variant = "juvenile" if juvenile else "female" if female else ""
    if variant:
        meta.update(variant_of="aldabra_tortoise", variant=variant, relative_height=height * scale / .744)
    return Spec(name="aldabra_tortoise" + ("_" + variant if variant else ""),
                bones=bones, parts=parts, rig=rig, clips=partial(_clips, scale=scale),
                paint=partial(_colour, scale=scale), surface_height=partial(_relief, scale=scale),
                voxel=.009 * scale, target_tris=14500, texture_size=1024, meta=meta)
