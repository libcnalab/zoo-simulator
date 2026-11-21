# SPDX-License-Identifier: MIT
"""Snow leopard adult and cub with alpine proportions, rosettes and a thick balancing tail."""
import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import R
from zoogen.wild_quadruped import make_wild_quadruped


def _shape(spec, juvenile):
    height = spec.meta["shoulder_height_m"]
    length = spec.meta["body_length_m"]
    head_z = .91 * height
    nose_x = .50 * length
    parts = {part.name: part for part in spec.parts}

    # A low, powerful alpine cat: deep shoulders, a flexible waist and longer hindquarters for
    # climbing and pouncing. The head stays small and round rather than becoming a canid wedge.
    parts["torso"].rings = [
        R((-.47 * length, 0, .68 * height), .13 * height, .17 * height, .13 * height),
        R((-.31 * length, 0, .68 * height), .23 * height, .23 * height, .22 * height),
        R((-.12 * length, 0, .67 * height), .22 * height, .24 * height, .20 * height),
        R(( .06 * length, 0, .68 * height), .22 * height, .24 * height, .22 * height),
        R(( .22 * length, 0, .70 * height), .19 * height, .22 * height, .18 * height),
        R(( .28 * length, 0, .74 * height), .14 * height, .17 * height, .14 * height),
    ]
    parts["neck"].rings = [
        R((.18 * length, 0, .72 * height), .16 * height, .19 * height),
        R((.27 * length, 0, .80 * height), .13 * height, .16 * height),
        R((.32 * length, 0, .87 * height), .11 * height, .13 * height),
    ]
    skull_scale = 1.18 if juvenile else 1.08
    parts["skull"].rings = [
        R((nose_x - .22 * length, 0, head_z), .115 * height * skull_scale,
          .12 * height * skull_scale),
        R((nose_x - .14 * length, 0, head_z + .005 * height), .15 * height * skull_scale,
          .14 * height * skull_scale, n=2.35),
        R((nose_x - .08 * length, 0, head_z - .025 * height), .115 * height,
          .095 * height, n=2.45),
        R((nose_x - .025 * length, 0, head_z - .055 * height), .068 * height,
          .052 * height, n=2.5),
    ]
    parts["jaw"].rings = [
        R((nose_x - .105 * length, 0, head_z - .08 * height), .09 * height, .04 * height),
        R((nose_x - .035 * length, 0, head_z - .10 * height), .06 * height, .026 * height),
    ]
    parts["nose"].rings = [
        R((nose_x - .03 * length, 0, head_z - .045 * height), .045 * height, .03 * height),
        R((nose_x + .008 * length, 0, head_z - .058 * height), .036 * height, .024 * height),
    ]
    eye = (nose_x - .14 * length + .033 * height, (.147 if juvenile else .137) * height,
           head_z + .055 * height)
    eye_size = (.028 if juvenile else .022) * height
    parts["eye.L"].rings = [R(eye, eye_size, .78 * eye_size)]

    # Small rounded ears lose little heat. Their tips are deliberately much shorter than a lynx's.
    ear_x = nose_x - .19 * length
    parts["ear.L"].rings = [
        R((ear_x, .105 * height, head_z + .02 * height), .06 * height, .045 * height),
        R((ear_x - .01 * length, .118 * height, head_z + .10 * height), .045 * height,
          .035 * height),
        R((ear_x - .016 * length, .12 * height, head_z + .15 * height), .012 * height,
          .010 * height),
    ]

    # Broad furred paws and slightly longer hind legs spread weight on snow and support climbing.
    for name in ("front_leg.L", "hind_leg.L"):
        for ring in parts[name].rings:
            ring.ry *= .86
            ring.up *= .88
            ring.dn *= .88
    for ring in parts["hind_leg.L"].rings[2:]:
        ring.c[2] *= .94
    for name in ("front_paw.L", "hind_paw.L"):
        for ring in parts[name].rings:
            ring.ry *= 1.38
            ring.up *= 1.28
            ring.dn *= 1.28

    # The fur-thick balancing tail approaches the body length and retains its diameter far out.
    rear_x = -.48 * length
    body_z = .69 * height
    parts["tail"].rings = [
        R((rear_x + .04 * length, 0, body_z), .105 * height, .105 * height),
        R((rear_x - .13 * length, 0, body_z - .01 * height), .115 * height, .115 * height),
        R((rear_x - .31 * length, 0, body_z - .03 * height), .11 * height, .11 * height),
        R((rear_x - .49 * length, 0, body_z - .06 * height), .10 * height, .10 * height),
        R((rear_x - .66 * length, 0, body_z - .09 * height), .085 * height, .085 * height),
        R((rear_x - .80 * length, 0, body_z - .12 * height), .06 * height, .06 * height),
        R((rear_x - .91 * length, 0, body_z - .14 * height), .018 * height, .018 * height),
    ]
    spec.meta["anatomy"] = ("Species-specific snow leopard: low muscular trunk, small rounded ears, "
                            "broad snow paws and body-length fur-thick balancing tail")


def _paint(ctx, length):
    p = ctx.P
    smoke = np.array([.63, .65, .63])
    pale = np.array([.86, .85, .79])
    dark = np.array([.11, .105, .10])
    grain = nz.fbm(p, 25.0, 4, seed=241)
    underside = nz.smoothstep(.20, -.58, ctx.N[..., 2])
    coat = nz.mix(smoke, pale, underside * .88)
    coat *= (.88 + .20 * grain)[..., None]

    # A narrow band of cellular noise produces open flank rosettes; head and distal legs carry
    # smaller solid spots. The pale underside stays largely clear.
    cells = nz.fbm(p * np.array([.85, 1.45, 1.25]), 7.5, 3, seed=251)
    outer = nz.smoothstep(.59, .69, cells)
    inner = nz.smoothstep(.71, .79, cells)
    rosettes = outer * (1.0 - inner) * np.clip(ctx.tag("body") + ctx.tag("tail"), 0, 1)
    rosettes *= 1.0 - .88 * underside
    freckles = nz.smoothstep(.68, .77, nz.fbm(p, 13.0, 3, seed=257))
    freckles *= np.clip(ctx.tag("head") + .75 * ctx.tag("leg"), 0, 1)
    freckles *= 1.0 - .75 * underside
    coat = nz.mix(coat, dark, np.clip(.94 * rosettes + .88 * freckles, 0, 1))
    coat = nz.mix(coat, dark, np.clip(ctx.tag("nose") + 2.8 * ctx.tag("eye"), 0, 1))
    ear_back = ctx.tag("ear") * nz.smoothstep(.15, .72, -ctx.N[..., 0])
    coat = nz.mix(coat, np.array([.22, .22, .21]), ear_back * .75)

    # Broken dark rings strengthen towards the tail tip without turning it into a striped tube.
    phase = (-(p[..., 0] + .43 * length) / (.92 * length)) * 7.0
    rings = nz.smoothstep(.50, .82, .5 + .5 * np.cos(2.0 * np.pi * phase))
    broken = .48 + .52 * nz.fbm(p * np.array([.35, 1.8, 1.8]), 9.0, 2, seed=271)
    coat = nz.mix(coat, dark, rings * broken * ctx.tag("tail") * .34)
    return coat


def _fur(ctx):
    fine = nz.fbm(ctx.P * np.array([.42, 1.3, 1.85]), 118.0, 3, seed=263)
    wool = nz.fbm(ctx.P, 28.0, 3, seed=269)
    smooth = np.clip(ctx.tag("eye") * 4.0 + ctx.tag("nose") * 2.0, 0.0, 1.0)
    return .5 + ((.82 * fine + .18 * wool) - .5) * (1.0 - smooth)


def make_spec(juvenile=False):
    height = .39 if juvenile else .60
    length = .74 if juvenile else 1.15
    spec = make_wild_quadruped({
        "name": "snow_leopard_juvenile" if juvenile else "snow_leopard",
        "kind": "snow_leopard",
        "common_name": "Snow leopard",
        "scientific_name": "Panthera uncia",
        "height": height,
        "length": length,
        "walk_speed": .68 if juvenile else 1.0,
        "trot_speed": 1.4 if juvenile else 2.2,
        "walk_period": .70 if juvenile else .90,
        "trot_period": .46 if juvenile else .58,
        "walk_lift": .052 if juvenile else .072,
        "fast_lift": .075 if juvenile else .105,
        "lie_drop": .23 if juvenile else .37,
        "hind_tuck": .055 if juvenile else .09,
        "hind_out": .035 if juvenile else .06,
        "head_down": quad.HeadDown(body_pitch=22, crouch=.08 if juvenile else .09, chest=16,
                                     neck=(52, 43), head=23),
        "idle_shift": .006 if juvenile else .010,
        "idle_bob": .002 if juvenile else .0035,
        "skull_scale": 1.18 if juvenile else 1.05,
        "muzzle_length_scale": .64,
        "ear_scale": 1.0,
        "eye_scale": 1.24 if juvenile else 1.0,
        "paw_scale": 1.28,
        "target_tris": 17500 if juvenile else 19500,
        "voxel": .0075 if juvenile else .0085,
        "meta": ({"variant_of": "snow_leopard", "variant": "juvenile",
                  "relative_height": height / .60,
                  "age_shape": "Cub: shorter body, broader skull, larger eyes and paws"}
                 if juvenile else {}),
    })
    _shape(spec, juvenile)
    spec.paint = lambda ctx: _paint(ctx, length)
    spec.surface_height = _fur
    spec.surface_strength = 1.35
    spec.meta["surface_detail"] = "anisotropic woolly winter-coat normal map"
    return spec
