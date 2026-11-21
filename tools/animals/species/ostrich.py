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
JUVENILE_SCALE = .52
FEMALE_RELATIVE_HEIGHT = .90


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
    parts = [Part("feathered_trunk", [
        # A rump-heavy ratite trunk rather than a symmetric egg: the back rises toward the
        # breast while the ventral contour stays deep before tucking into the neck root.
        R((-1.03, 0, 1.30), .08, .10, .08), R((-.83, 0, 1.30), .25, .24, .21),
        R((-.57, 0, 1.31), .37, .31, .27), R((-.29, 0, 1.31), .42, .35, .32),
        R((.00, 0, 1.32), .39, .34, .36), R((.25, 0, 1.36), .30, .28, .38),
        R((.44, 0, 1.41), .16, .18, .24),
    ], chain=biped.chain(bones, "hips", "chest"), segments=36, rough=(.008, 22), cap_scale=.4),
        Part("bare_neck", [R(NECK[0], .105, .13), R(NECK[1], .061, .068),
              R(NECK[2], .048, .052), R(NECK[3], .045, .048), R(NECK[4], .049, .053)],
             chain=biped.chain(bones, "neck1", "neck2", "neck3", "neck4", "head"),
             segments=24, tag="bare", cap_start="flat", cap_end="flat"),
        Part("skull", [R((.59, 0, 2.325), .039, .047, .043),
            R((.65, 0, 2.337), .061, .063, .057), R((.72, 0, 2.337), .076, .071, .059),
            R((.79, 0, 2.315), .055, .040, .035)],
             weights={"head": 1}, segments=24, tag="bare", cap_scale=.3),
        Part("broad_bill", [R((.77, 0, 2.31), .057, .019),
             R((.88, 0, 2.31), .060, .016), R((.98, 0, 2.305), .032, .012)],
             weights={"head": 1}, segments=20, tag="beak", cap_scale=.25),
        biped.eye_part((.724, .064, 2.350), .022),
        Part("nostril.L", [R((.817, .052, 2.322), .008, .006),
            R((.839, .054, 2.321), .007, .005)], weights={"head": 1}, mirror=True,
             fuse=False, segments=10, tag="nostril", cap_scale=.35),
        Part("folded_wing.L", [R((.20, .25, 1.39), .040, .075),
            R((-.10, .37, 1.28), .045, .13), R((-.45, .37, 1.22), .045, .13),
            R((-.72, .31, 1.22), .025, .08)], weights={"wing.L": 1},
             mirror=True, fuse=False, segments=28, tag="wing", rough=(.009, 28), cap_scale=.4),
        Part("tail_plume", [R((-.80, 0, 1.37), .15, .13), R((-1.02, 0, 1.42), .24, .16),
             R((-1.23, 0, 1.38), .22, .13), R((-1.40, 0, 1.31), .085, .055)],
             weights={"tail1": 1}, segments=24,
             tag="plume", rough=(.025, 18), cap_scale=.4),
        Part("thigh_plumage.L", [R((-.18, .17, 1.23), .15, .20),
            R((-.22, .17, 1.11), .145, .18), R((-.29, .17, .98), .12, .145),
            R((-.37, .17, .83), .085, .10), R((-.43, .17, .72), .052, .062)],
             chain=biped.chain(bones, "thigh.L", "shin.L"), mirror=True,
             segments=20, tag="body", cap_start="flat", cap_end="round"),
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
    # Separate overlapping feathers break up the single smooth wing and tail masses. They are
    # rigid to the existing wing/tail bones, so every established clip keeps its deformation.
    for index, (root_x, end_x, y, z) in enumerate((
            (.12, -.38, .34, 1.32), (.04, -.52, .38, 1.29),
            (-.05, -.65, .40, 1.25), (-.14, -.74, .39, 1.21))):
        parts.append(Part(f"wing_feather.{index}.L", [
            R((root_x, y, z + .05), .035, .055),
            R(((root_x + end_x) * .5, y + .018, z), .045, .048),
            R((end_x, y, z - .025), .012, .018)],
            weights={"wing.L": 1}, mirror=True, fuse=False, segments=12, tag="wing",
            rough=(.006, 34), cap_start="flat", cap_end="point"))
    for index, (end_x, y, z) in enumerate((
            (-1.34, -.12, 1.48), (-1.43, -.06, 1.42), (-1.47, 0, 1.34),
            (-1.43, .06, 1.27), (-1.34, .12, 1.22))):
        parts.append(Part(f"tail_feather.{index}", [
            R((-.79, 0, 1.37), .060, .075), R((-1.06, y * .5, (1.37 + z) * .5), .075, .060),
            R((end_x, y, z), .016, .020)], weights={"tail1": 1}, fuse=False,
            segments=12, tag="plume", rough=(.010, 28), cap_start="flat", cap_end="point"))
    return parts


def paint(ctx, juvenile=False, female=False, scale=1.0):
    if juvenile:
        # Chicks are cryptic tan rather than miniature black-and-white adult males. Work in
        # adult coordinates so the down's stripes keep their visual width after scaling.
        P = ctx.P / scale
        grain = nz.fbm(P * [1, 1, 3], 46, 3, seed=64)
        tan = np.array([.48, .37, .23])
        cream = np.array([.72, .61, .42])
        brown = np.array([.27, .19, .12])
        color = tan * (.86 + .24 * grain)[..., None]
        feather = np.maximum(ctx.tag("body"), np.maximum(ctx.tag("wing"), ctx.tag("plume")))
        irregular = nz.fbm(P, 7.0, 3, seed=65)
        stripes = .5 + .5 * np.sin((P[..., 0] + .13 * P[..., 2]) * 31.0 + 2.2 * irregular)
        stripes = nz.smoothstep(.80, .96, stripes) * feather
        color = nz.mix(color, brown * (.9 + .2 * grain)[..., None], stripes * .52)
        color = nz.mix(color, cream * (.92 + .12 * grain)[..., None],
                       feather * nz.smoothstep(.15, -.75, ctx.N[..., 2]) * .45)
        bare = np.array([.52, .43, .31]) * (.91 + .13 * grain)[..., None]
        color = nz.mix(color, bare, np.maximum(ctx.tag("bare"), ctx.tag("digit")))
        claw = ctx.tag("digit") * nz.smoothstep(.21, .25, P[..., 0])
        color = nz.mix(color, np.array([.15, .11, .08]), claw)
        color = nz.mix(color, np.array([.47, .39, .28]), ctx.tag("beak"))
        color = nz.mix(color, np.array([.045, .025, .012]),
                       np.maximum(ctx.tag("eye"), ctx.tag("nostril")))
        return color
    if female:
        # Adult hens are brown rather than black-and-white. Fine mottling keeps the layered
        # feather masses readable while the bare neck and legs remain a quiet grey-brown.
        P = ctx.P / scale
        grain = nz.fbm(P * [1, 1, 4], 40, 3, seed=68)
        brown = np.array([.34, .27, .20])
        pale_brown = np.array([.53, .44, .33])
        dark_brown = np.array([.22, .17, .13])
        feather = np.maximum(ctx.tag("body"), np.maximum(ctx.tag("wing"), ctx.tag("plume")))
        color = brown * (.82 + .38 * grain)[..., None]
        mottling = nz.fbm(P, 8.0, 3, seed=69)
        color = nz.mix(color, pale_brown * (.90 + .16 * grain)[..., None],
                       feather * nz.smoothstep(.58, .86, mottling) * .54)
        color = nz.mix(color, dark_brown, ctx.tag("wing") * .16 + ctx.tag("plume") * .12)
        bare = np.array([.45, .40, .34]) * (.92 + .12 * grain)[..., None]
        color = nz.mix(color, bare, np.maximum(ctx.tag("bare"), ctx.tag("digit")))
        claw = ctx.tag("digit") * nz.smoothstep(.21, .25, P[..., 0])
        color = nz.mix(color, np.array([.13, .10, .075]), claw)
        color = nz.mix(color, np.array([.49, .43, .35]), ctx.tag("beak"))
        color = nz.mix(color, np.array([.045, .025, .012]),
                       np.maximum(ctx.tag("eye"), ctx.tag("nostril")))
        return color
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
    color = nz.mix(color, np.array([.045, .025, .012]),
                   np.maximum(ctx.tag("eye"), ctx.tag("nostril")))
    return color


def surface_height(ctx, scale=1.0):
    """Layered feather relief on plumage while bare skin, bill, eyes and feet stay smooth."""
    P = ctx.P / scale
    directional = nz.fbm(P * np.array([.38, 1.15, 2.10]), 46.0, 4, seed=66)
    barbs = nz.fbm(P * np.array([.30, 1.35, 2.50]), 105.0, 2, seed=67)
    feather = np.maximum(ctx.tag("body"), np.maximum(ctx.tag("wing"), ctx.tag("plume")))
    relief = .72 * directional + .28 * barbs
    return .5 + (relief - .5) * feather


def _feeding(solver, effort, scale=1.0):
    pose = biped.standing(solver)
    drop = .44 if scale < 1.0 else .32
    pose.move("hips", dz=-drop * scale * effort)
    pose.r("hips", ry=25 * effort)
    angles = [60 - 122 * effort, 100 - 168 * effort, 80 - 158 * effort, 60 - 130 * effort]
    biped.sagittal_profile(solver, pose, solver.rig.neck, angles)
    biped.sagittal_profile(solver, pose, ["head"], [-5])
    return pose


def _sitting(solver, asleep, scale=1.0):
    pose = biped.standing(solver)
    pose.move("hips", dz=-.72 * scale)
    for key in solver.rig.legs:
        target = solver.toe_base(key)
        target[0] = .18 * scale
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


def _dust_bathe(solver, sitting, phase, juvenile=False):
    """`dust_bathe`: sitting in a dry hollow, the bird rocks from side to side, lays its neck
    along the ground and sweeps it to throw sand over its flank, and shakes its loosened wings."""
    angle = math.tau * phase
    yaw, low, roll, lift, shake = activity.held(phase, DUST_BATHE)
    pose = biped.copy_pose(sitting)
    pose.r("hips", rx=-roll)
    if juvenile:
        neutral = [math.degrees(math.atan2(solver.tail[name][2] - solver.head[name][2],
                                           solver.tail[name][0] - solver.head[name][0]))
                   for name in solver.rig.neck]
    else:
        neutral = [math.degrees(math.atan2(b[2] - a[2], b[0] - a[0]))
                   for a, b in zip(NECK, NECK[1:])]
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


def _clips(solver, juvenile=False, scale=1.0):
    if juvenile:
        clips = [biped.gait_clip(solver, biped.Stride("walk", .75, .58, .64, .075,
                       bob=.006, nod=1.2)),
                 biped.gait_clip(solver, biped.Stride("trot", 2.0, .38, .40, .12,
                       bob=.018, lean=4, wing=12))]
    else:
        clips = [biped.gait_clip(solver, biped.Stride("walk", 1.2, .8, .64, .13,
                       bob=.012, nod=1.2)),
                 biped.gait_clip(solver, biped.Stride("trot", 3.2, .5, .40, .24,
                       bob=.035, lean=4, wing=16))]
    feeding = {}
    targets = (("eat", .040), ("drink", .022)) if juvenile else (("eat", .075), ("drink", .035))
    for name, height in targets:
        effort = bisect_effort(lambda value: muzzle(solver, _feeding(solver, value, scale))[2],
                               height, name)
        feeding[name] = _feeding(solver, effort, scale)
    postures = {"rest": _sitting(solver, False, scale),
                "sleep": _sitting(solver, True, scale), **feeding}
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
    clips.append(Clip("dust_bathe", 10,
                      partial(_dust_bathe, solver, postures["rest"], juvenile=juvenile)))
    return clips


def _juvenile_shape(spec):
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

    # A chick has a shorter neck and face, a large round head and eye, downy wings and no
    # adult male's extravagant white tail and wing plumes.
    neck_base = np.asarray(NECK[0], dtype=float) * scale

    def shorten_neck(point):
        point[:] = neck_base + .78 * (point - neck_base)

    for bone in spec.bones:
        if bone.name.startswith("neck") or bone.name == "head":
            shorten_neck(bone.head)
            shorten_neck(bone.tail)
    for part in spec.parts:
        if part.name in {"bare_neck", "skull", "broad_bill", "eye.L", "nostril.L"}:
            for ring in part.rings:
                shorten_neck(ring.c)
        if part.name == "skull":
            for ring in part.rings:
                ring.ry *= 1.34
                ring.up *= 1.30
                ring.dn *= 1.30
        elif part.name == "eye.L":
            for ring in part.rings:
                ring.c[1] *= 1.36
                ring.ry *= 1.48
                ring.up *= 1.48
                ring.dn *= 1.48
        elif part.name == "nostril.L":
            for ring in part.rings:
                ring.c[1] *= 1.12
        elif part.name == "broad_bill":
            for ring in part.rings:
                ring.ry *= 1.12
                ring.up *= 1.10
                ring.dn *= 1.10
        elif part.name == "feathered_trunk":
            for ring in part.rings:
                ring.ry *= 1.08
                ring.up *= 1.08
                ring.dn *= 1.08
        elif part.name == "folded_wing.L" or part.name.startswith("wing_feather."):
            wing_root = part.rings[0].c[0]
            for ring in part.rings:
                ring.c[0] = wing_root + .55 * (ring.c[0] - wing_root)
                ring.ry *= .90
                ring.up *= .90
                ring.dn *= .90
        elif part.name == "tail_plume" or part.name.startswith("tail_feather."):
            tail_root = part.rings[0].c[0]
            for ring in part.rings:
                ring.c[0] = tail_root + .35 * (ring.c[0] - tail_root)
                ring.ry *= .62
                ring.up *= .62
                ring.dn *= .62
        elif part.name in {"thigh_plumage.L", "leg.L", "ankle.L"} \
                or part.name.startswith("digit"):
            for ring in part.rings:
                ring.ry *= 1.12
                ring.up *= 1.08
                ring.dn *= 1.08

    bones = {bone.name: bone for bone in spec.bones}
    for part in spec.parts:
        if part.chain is not None:
            part.chain = [(name, bones[name].head.copy(), bones[name].tail.copy())
                          for name, _, _ in part.chain]
    muzzle_bone, muzzle_point = spec.rig.extra["muzzle"]
    muzzle_point = np.asarray(muzzle_point, dtype=float) * scale
    shorten_neck(muzzle_point)
    spec.rig.extra["muzzle"] = (muzzle_bone, muzzle_point)

    spec.name = "ostrich_juvenile"
    spec.paint = partial(paint, juvenile=True, scale=scale)
    spec.surface_height = partial(surface_height, scale=scale)
    spec.voxel = .005
    spec.target_tris = 12000
    spec.meta.update({"shoulder_height_m": 1.13, "body_length_m": 1.17,
                      "variant_of": "ostrich", "variant": "juvenile",
                      "relative_height": 1.13 / 2.4,
                      "age_shape": "Chick: shorter neck and face, larger head, eye and feet",
                      "sex": "juvenile"})


def _female_shape(spec):
    """Give the hen a lighter adult build without disturbing the shared adult rig."""
    for part in spec.parts:
        if part.name == "feathered_trunk":
            for ring in part.rings:
                ring.ry *= .94
                ring.up *= .96
                ring.dn *= .96
        elif part.name == "thigh_plumage.L":
            for ring in part.rings:
                ring.ry *= .95
                ring.up *= .96
                ring.dn *= .96
        elif part.name in {"folded_wing.L", "tail_plume"} \
                or part.name.startswith(("wing_feather.", "tail_feather.")):
            for ring in part.rings:
                ring.ry *= .92
                ring.up *= .92
                ring.dn *= .92

    spec.name = "ostrich_female"
    spec.paint = partial(paint, female=True)
    spec.target_tris = 14000
    spec.meta.update({"common_name": "Common ostrich hen", "sex": "female",
                      "variant_of": "ostrich", "variant": "female",
                      "relative_height": FEMALE_RELATIVE_HEIGHT,
                      "coat": "Mottled brown adult plumage without the cock's white plumes",
                      "age_shape": "Slightly shorter and lighter adult hen"})


def make_spec(juvenile=False, female=False):
    if juvenile and female:
        raise ValueError("juvenile and female ostrich variants are mutually exclusive")
    bones, rig = _skeleton()
    spec = Spec("ostrich", bones, _parts(bones), paint=paint,
                surface_height=surface_height, surface_strength=1.15, voxel=.009,
                target_tris=14000, smooth_iters=4, rig=rig,
                clips=partial(_clips, juvenile=True, scale=JUVENILE_SCALE) if juvenile else _clips,
                key_all_locations=True,
                meta={"common_name": "Common ostrich", "scientific_name": "Struthio camelus",
                      "shoulder_height_m": 2.4, "body_length_m": 2.1, "sex": "male", "roughness": .93,
                      "surface_detail": "layered feather normal map",
                      "anatomy": "third-pass ratite trunk, muscular thighs, large eyes and nostrils",
                      # The game's clip for a still activity (AnimalRenderer).
                      "activity_clips": {"dust_bathe": "dust_bathe"}})
    if juvenile:
        _juvenile_shape(spec)
    elif female:
        _female_shape(spec)
    return spec
