# SPDX-License-Identifier: MIT
"""Red panda adult and cub with a masked face, low body and long ringed tail."""
import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Part, R
from zoogen.wild_quadruped import make_wild_quadruped


def _shape(spec, juvenile):
    height = spec.meta["shoulder_height_m"]
    length = spec.meta["body_length_m"]
    head_z = 1.02 * height
    nose_x = .49 * length
    parts = {part.name: part for part in spec.parts}

    # Compact mustelid-like trunk, low limbs and a short neck: deliberately unlike the high,
    # square lynx or long-faced generic canid produced by the common starting rig.
    parts["torso"].rings = [
        R((-.45 * length, 0, .66 * height), .15 * height, .16 * height),
        R((-.30 * length, 0, .65 * height), .25 * height, .23 * height),
        R((-.10 * length, 0, .64 * height), .27 * height, .24 * height),
        R(( .10 * length, 0, .67 * height), .25 * height, .23 * height),
        R(( .25 * length, 0, .72 * height), .20 * height, .20 * height),
    ]
    parts["neck"].rings = [
        R((.19 * length, 0, .72 * height), .17 * height, .18 * height),
        R((.28 * length, 0, .83 * height), .15 * height, .17 * height),
        R((.31 * length, 0, .94 * height), .13 * height, .15 * height),
    ]
    skull_scale = 1.12 if juvenile else 1.0
    parts["skull"].rings = [
        R((nose_x - .22 * length, 0, head_z), .14 * height * skull_scale,
          .14 * height * skull_scale),
        R((nose_x - .14 * length, 0, head_z + .01 * height), .18 * height * skull_scale,
          .17 * height * skull_scale, n=2.35),
        R((nose_x - .07 * length, 0, head_z - .03 * height), .13 * height,
          .105 * height, n=2.45),
        R((nose_x - .025 * length, 0, head_z - .07 * height), .075 * height,
          .06 * height, n=2.55),
    ]
    parts["jaw"].rings = [
        R((nose_x - .11 * length, 0, head_z - .10 * height), .10 * height, .04 * height),
        R((nose_x - .03 * length, 0, head_z - .12 * height), .065 * height, .028 * height),
    ]
    parts["nose"].rings = [
        R((nose_x - .025 * length, 0, head_z - .06 * height), .050 * height, .034 * height),
        R((nose_x + .015 * length, 0, head_z - .073 * height), .040 * height, .026 * height),
    ]
    eye = (nose_x - .145 * length, .115 * height, head_z + .015 * height)
    eye_size = (.032 if juvenile else .026) * height
    parts["eye.L"].rings = [R(eye, eye_size, .80 * eye_size)]

    # Upright white-edged ears and separate cheek fans make the facial mask legible in profile.
    ear_x = nose_x - .19 * length
    parts["ear.L"].rings = [
        R((ear_x, .13 * height, head_z + .04 * height), .075 * height, .055 * height),
        R((ear_x - .01 * length, .14 * height, head_z + .17 * height), .055 * height,
          .045 * height),
        R((ear_x - .018 * length, .145 * height, head_z + .25 * height), .012 * height,
          .010 * height),
    ]
    spec.parts.append(Part(
        "cheek_fan.L",
        [R((nose_x - .19 * length, .13 * height, head_z - .02 * height), .075 * height,
           .075 * height),
         R((nose_x - .12 * length, .17 * height, head_z - .09 * height), .095 * height,
           .080 * height),
         R((nose_x - .075 * length, .145 * height, head_z - .16 * height), .025 * height,
           .025 * height)],
        weights={"head": 1.0}, tag="mask", segments=12, rough=(.006, 29.0), fuse=False,
        mirror=True, cap_end="point"))

    # Short dark legs and large furred soles suit a branch-walking animal.
    for name in ("front_leg.L", "hind_leg.L"):
        for ring in parts[name].rings:
            ring.ry *= .78
            ring.up *= .77
            ring.dn *= .77
    # Seat the duplicated upper transition rings inside the trunk rather than leaving four
    # generic quadruped joint domes above this small animal's back line.
    for ring in parts["front_leg.L"].rings[:2]:
        ring.c[2] = .78 * height
    parts["hind_leg.L"].rings[0].c[2] = .76 * height
    for name in ("front_paw.L", "hind_paw.L"):
        for ring in parts[name].rings:
            ring.ry *= 1.20
            ring.up *= 1.16
            ring.dn *= 1.16

    # The cylindrical, fur-thick tail is nearly the body's length and tapers gently through its tip.
    rear_x = -.45 * length
    body_z = .66 * height
    parts["tail"].rings = [
        R((rear_x + .04 * length, 0, body_z), .095 * height, .095 * height),
        R((rear_x - .12 * length, 0, body_z - .01 * height), .105 * height, .105 * height),
        R((rear_x - .29 * length, 0, body_z - .02 * height), .105 * height, .105 * height),
        R((rear_x - .46 * length, 0, body_z - .04 * height), .095 * height, .095 * height),
        R((rear_x - .63 * length, 0, body_z - .06 * height), .082 * height, .082 * height),
        R((rear_x - .78 * length, 0, body_z - .08 * height), .062 * height, .062 * height),
        R((rear_x - .90 * length, 0, body_z - .10 * height), .020 * height, .020 * height),
    ]
    spec.meta["anatomy"] = ("Species-specific red panda: compact low trunk, masked rounded head, "
                            "pointed ears, broad branch paws and long gently tapered ringed tail")


def _paint(ctx, length, height, juvenile):
    p = ctx.P
    russet = np.array([.60, .20, .075]) if not juvenile else np.array([.54, .25, .13])
    cream = np.array([.90, .82, .68])
    dark = np.array([.095, .065, .055])
    grain = nz.fbm(p, 25.0, 4, seed=211)
    underside = nz.smoothstep(.22, -.55, ctx.N[..., 2])
    coat = nz.mix(russet, np.array([.28, .15, .09]), underside * .60)
    coat *= (.86 + .25 * grain)[..., None]

    # Cream muzzle, brows and cheeks surround dark tear tracks and eyes.
    head = np.clip(ctx.tag("head") + ctx.tag("jaw"), 0.0, 1.0)
    side = nz.smoothstep(.035, .09, np.abs(p[..., 1]))
    brow = head * side * nz.smoothstep(.88 * length, .55 * length, p[..., 0])
    coat = nz.mix(coat, cream, np.clip(.70 * brow + ctx.tag("mask") + .55 * ctx.tag("ear"), 0, 1))
    tear = head * side * nz.smoothstep(.38 * length, .31 * length, p[..., 0])
    coat = nz.mix(coat, dark, tear * .58)
    coat = nz.mix(coat, dark, np.clip(ctx.tag("nose") + 2.5 * ctx.tag("eye"), 0, 1))
    # The rig's leg tag also covers shoulder/hip transition pieces, so height-gate the dark socks;
    # otherwise four black joint caps sit visibly on the russet back.
    low_limb = nz.smoothstep(.58 * height, .36 * height, p[..., 2])
    socks = np.clip(ctx.tag("leg") * low_limb + .70 * ctx.tag("paw"), 0, 1)
    coat = nz.mix(coat, dark, socks * .88)

    # Seven pale annuli cross only the tail; the russet end remains darkened rather than white.
    phase = (-(p[..., 0] + .40 * length) / (.90 * length)) * 7.0
    rings = .5 + .5 * np.cos(2.0 * np.pi * phase)
    rings = nz.smoothstep(.35, .70, rings) * ctx.tag("tail")
    coat = nz.mix(coat, np.array([.78, .50, .28]), rings * .72)
    tail_end = ctx.tag("tail") * nz.smoothstep(-1.18 * length, -1.32 * length, p[..., 0])
    coat = nz.mix(coat, dark, tail_end * .45)
    return coat


def _fur(ctx):
    fine = nz.fbm(ctx.P * np.array([.42, 1.25, 1.8]), 122.0, 3, seed=223)
    clumps = nz.fbm(ctx.P, 31.0, 3, seed=227)
    smooth = np.clip(ctx.tag("eye") * 4.0 + ctx.tag("nose") * 2.0, 0.0, 1.0)
    return .5 + ((.84 * fine + .16 * clumps) - .5) * (1.0 - smooth)


def make_spec(juvenile=False):
    height = .24 if juvenile else .32
    length = .46 if juvenile else .60
    spec = make_wild_quadruped({
        "name": "red_panda_juvenile" if juvenile else "red_panda",
        "kind": "red_panda",
        "common_name": "Red panda",
        "scientific_name": "Ailurus fulgens",
        "height": height,
        "length": length,
        "walk_speed": .42 if juvenile else .72,
        "trot_speed": .85 if juvenile else 1.65,
        "walk_period": .52 if juvenile else .78,
        "trot_period": .36 if juvenile else .51,
        "walk_lift": .030 if juvenile else .048,
        "fast_lift": .040 if juvenile else .067,
        # The generic auto-fit's deliberately broad 0.4 m search bracket is larger than this
        # cub; measured opt-in drops keep all four short legs within reach without touching other
        # species' gait generation.
        "walk_drop": .010 if juvenile else None,
        "fast_drop": .030 if juvenile else .100,
        # Red pandas keep their plantigrade paws nearly level in a fast bound. The generic
        # digitigrade 54-degree toe-off makes this adult's very short shin snap between samples.
        "fast_swing_pitch": 54.0 if juvenile else 5.0,
        "lie_drop": .13 if juvenile else .18,
        "hind_tuck": .035 if juvenile else .05,
        "hind_out": .025 if juvenile else .035,
        "head_down": quad.HeadDown(body_pitch=20, crouch=.06 if juvenile else .075, chest=15,
                                     neck=(50, 40), head=22),
        "idle_shift": .004 if juvenile else .006,
        "idle_bob": .0015 if juvenile else .0025,
        "skull_scale": 1.20 if juvenile else 1.08,
        "muzzle_length_scale": .62,
        "ear_scale": 1.14,
        "eye_scale": 1.26 if juvenile else 1.05,
        "paw_scale": 1.20,
        "target_tris": 16000 if juvenile else 18000,
        "voxel": .0055 if juvenile else .0065,
        "meta": ({"variant_of": "red_panda", "variant": "juvenile",
                  "relative_height": height / .32,
                  "age_shape": "Cub: shorter body, broader skull, larger eyes, ears and paws"}
                 if juvenile else {}),
    })
    _shape(spec, juvenile)
    spec.paint = lambda ctx: _paint(ctx, length, height, juvenile)
    spec.surface_height = _fur
    spec.surface_strength = 1.20
    spec.meta["surface_detail"] = "anisotropic thick-coat normal map"
    return spec
