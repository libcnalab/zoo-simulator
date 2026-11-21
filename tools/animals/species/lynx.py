# SPDX-License-Identifier: MIT
"""Eurasian lynx adult and kitten with a short tail, snow paws and tufted ears."""
import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Part, R
from zoogen.wild_quadruped import make_wild_quadruped


def _shape(spec, juvenile):
    height = spec.meta["shoulder_height_m"]
    length = spec.meta["body_length_m"]
    head_z = .91 * height
    nose_x = .50 * length
    parts = {part.name: part for part in spec.parts}

    # A lynx is a high-legged, square cat. The deep chest gives way to a modest abdominal tuck;
    # the rounded skull and very short muzzle do not read as the long generic canid head.
    parts["torso"].rings = [
        R((-.47 * length, 0, .78 * height), .10 * height, .12 * height),
        R((-.31 * length, 0, .71 * height), .21 * height, .20 * height),
        R((-.13 * length, 0, .68 * height), .18 * height, .17 * height),
        R(( .04 * length, 0, .70 * height), .19 * height, .22 * height),
        R(( .19 * length, 0, .73 * height), .17 * height, .23 * height),
        R(( .25 * length, 0, .75 * height), .13 * height, .17 * height),
    ]
    parts["neck"].rings = [
        R((.16 * length, 0, .72 * height), .14 * height, .18 * height),
        R((.25 * length, 0, .80 * height), .12 * height, .16 * height),
        R((.30 * length, 0, .88 * height), .10 * height, .13 * height),
    ]
    skull_scale = 1.10 if juvenile else 1.0
    parts["skull"].rings = [
        R((nose_x - .22 * length, 0, head_z), .105 * height * skull_scale,
          .115 * height * skull_scale),
        R((nose_x - .14 * length, 0, head_z + .005 * height), .145 * height * skull_scale,
          .135 * height * skull_scale, n=2.3),
        R((nose_x - .085 * length, 0, head_z - .020 * height), .115 * height,
          .095 * height, n=2.4),
        R((nose_x - .035 * length, 0, head_z - .050 * height), .072 * height,
          .055 * height, n=2.5),
    ]
    parts["jaw"].rings = [
        R((nose_x - .105 * length, 0, head_z - .075 * height), .090 * height, .038 * height),
        R((nose_x - .040 * length, 0, head_z - .095 * height), .062 * height, .025 * height),
    ]
    parts["nose"].rings = [
        R((nose_x - .035 * length, 0, head_z - .040 * height), .047 * height, .031 * height),
        R((nose_x + .005 * length, 0, head_z - .052 * height), .038 * height, .025 * height),
    ]
    eye = (nose_x - .145 * length + .033 * height, (.135 if juvenile else .123) * height,
           head_z + .053 * height)
    eye_size = (.027 if juvenile else .021) * height
    parts["eye.L"].rings = [R(eye, eye_size, .78 * eye_size)]

    # Broad triangular ears carry separate dark hair pencils above their rounded tips.
    ear_x = nose_x - .19 * length
    parts["ear.L"].rings = [
        R((ear_x, .105 * height, head_z + .025 * height), .062 * height, .045 * height),
        R((ear_x - .012 * length, .125 * height, head_z + .125 * height),
          .048 * height, .038 * height),
        R((ear_x - .018 * length, .135 * height, head_z + .185 * height),
          .014 * height, .012 * height),
    ]
    spec.parts.append(Part(
        "ear_pencil.L",
        [R((ear_x - .018 * length, .135 * height, head_z + .175 * height), .012 * height,
           .012 * height),
         R((ear_x - .028 * length, .140 * height, head_z + .285 * height), .003 * height,
           .003 * height)],
        weights={"ear.L": 1.0}, tag="ear_pencil", segments=8, subdiv=2, fuse=False,
        mirror=True, cap_start="flat", cap_end="point"))
    spec.parts.append(Part(
        "cheek_ruff.L",
        [R((nose_x - .20 * length, .11 * height, head_z - .015 * height), .065 * height,
           .065 * height),
         R((nose_x - .145 * length, .145 * height, head_z - .075 * height), .085 * height,
           .075 * height),
         R((nose_x - .11 * length, .13 * height, head_z - .13 * height), .025 * height,
           .025 * height)],
        weights={"head": 1.0}, tag="ruff", segments=12, rough=(.006, 22.0), fuse=False,
        mirror=True, cap_end="point"))

    # Long legs end in outsize furred paws that spread the load on snow.
    for name in ("front_leg.L", "hind_leg.L"):
        for ring in parts[name].rings:
            ring.ry *= .82
            ring.up *= .86
            ring.dn *= .86
    for name in ("front_paw.L", "hind_paw.L"):
        for ring in parts[name].rings:
            ring.ry *= 1.34
            ring.up *= 1.24
            ring.dn *= 1.24

    # The black-tipped bobtail is shorter than the rump is wide, unlike a canid brush.
    rear_x = -.50 * length
    body_z = .70 * height
    parts["tail"].rings = [
        R((rear_x + .07 * length, 0, body_z), .055 * height, .055 * height),
        R((rear_x - .015 * length, 0, body_z - .025 * height), .052 * height, .052 * height),
        R((rear_x - .075 * length, 0, body_z - .055 * height), .045 * height, .045 * height),
        R((rear_x - .125 * length, 0, body_z - .075 * height), .025 * height, .025 * height),
    ]
    spec.meta["anatomy"] = ("Species-specific lynx: square long-legged trunk, rounded short muzzle, "
                            "wide snow paws, facial ruff, ear pencils and black-tipped bobtail")


def _paint(ctx, length):
    p = ctx.P
    tawny = np.array([.62, .43, .24])
    pale = np.array([.86, .78, .64])
    dark = np.array([.10, .075, .055])
    grain = nz.fbm(p, 22.0, 4, seed=173)
    underside = nz.smoothstep(.18, -.58, ctx.N[..., 2])
    coat = nz.mix(tawny, pale, underside * .88)
    coat *= (.87 + .23 * grain)[..., None]

    # Irregular flank and leg spots, faded on the pale underside and face.
    cells = nz.fbm(p * np.array([1.0, 1.8, 1.5]), 8.5, 3, seed=179)
    spots = nz.smoothstep(.67, .76, cells)
    spots *= np.clip(ctx.tag("body") + .75 * ctx.tag("leg"), 0.0, 1.0)
    spots *= 1.0 - .86 * underside
    coat = nz.mix(coat, np.array([.14, .095, .065]), spots * .88)

    head = ctx.tag("head") + ctx.tag("jaw")
    cheek = nz.smoothstep(.04, .11, np.abs(p[..., 1])) * head
    coat = nz.mix(coat, pale, cheek * .55 + ctx.tag("ruff") * .75)
    coat = nz.mix(coat, dark, ctx.tag("nose") + ctx.tag("ear_pencil"))
    ear_back = ctx.tag("ear") * nz.smoothstep(.15, .75, -ctx.N[..., 0])
    coat = nz.mix(coat, dark, ear_back * .88)

    # Only the distal half of the geometrically short tail is black.
    tail_tip = ctx.tag("tail") * nz.smoothstep(-.48 * length, -.56 * length, p[..., 0])
    coat = nz.mix(coat, dark, tail_tip)
    coat = nz.mix(coat, np.array([.36, .24, .10]), np.clip(ctx.tag("eye") * 3.0, 0, 1))
    return coat


def _fur(ctx):
    fine = nz.fbm(ctx.P * np.array([.45, 1.3, 1.8]), 115.0, 3, seed=181)
    clumps = nz.fbm(ctx.P, 30.0, 3, seed=191)
    smooth = np.clip(ctx.tag("eye") * 4.0 + ctx.tag("nose") * 2.0, 0.0, 1.0)
    return .5 + ((.88 * fine + .12 * clumps) - .5) * (1.0 - smooth)


def make_spec(juvenile=False):
    height = .40 if juvenile else .65
    length = .75 if juvenile else 1.10
    spec = make_wild_quadruped({
        "name": "lynx_juvenile" if juvenile else "lynx",
        "kind": "lynx",
        "common_name": "Eurasian lynx",
        "scientific_name": "Lynx lynx",
        "height": height,
        "length": length,
        "walk_speed": .75 if juvenile else 1.1,
        "trot_speed": 1.5 if juvenile else 2.2,
        "walk_period": .72 if juvenile else .92,
        "trot_period": .48 if juvenile else .58,
        "walk_lift": .055 if juvenile else .075,
        "fast_lift": .075 if juvenile else .11,
        "lie_drop": .24 if juvenile else .40,
        "hind_tuck": .055 if juvenile else .09,
        "hind_out": .035 if juvenile else .06,
        "head_down": quad.HeadDown(body_pitch=22, crouch=.08 if juvenile else .09, chest=16,
                                     neck=(52, 43), head=23),
        "idle_shift": .006 if juvenile else .010,
        "idle_bob": .002 if juvenile else .0035,
        "skull_scale": 1.18 if juvenile else 1.05,
        "muzzle_length_scale": .65,
        "ear_scale": 1.18,
        "eye_scale": 1.22 if juvenile else 1.0,
        "paw_scale": 1.25,
        "target_tris": 16000 if juvenile else 18000,
        "voxel": .0075 if juvenile else .009,
        "meta": ({"variant_of": "lynx", "variant": "juvenile",
                  "relative_height": height / .65,
                  "age_shape": "Kitten: shorter body, broader skull, larger eyes and paws"}
                 if juvenile else {}),
    })
    _shape(spec, juvenile)
    spec.paint = lambda ctx: _paint(ctx, length)
    spec.surface_height = _fur
    spec.surface_strength = 1.25
    spec.meta["surface_detail"] = "anisotropic dense winter-coat normal map"
    return spec
