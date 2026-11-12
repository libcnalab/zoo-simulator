# SPDX-License-Identifier: MIT
"""Bactrian camel: 2.1 m withers, about 3.0 m nose-to-rump excluding tail.

The humps rise above a level back; a five-bone U-shaped neck carries the head
above them. The limbs end in broad paired pads rather than carnivore paws.
"""
import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.anatomy import chain, leg_bones, set_bone, split_foot
from zoogen.geom import Bone, Part, R
from zoogen.wild_quadruped import make_wild_quadruped


def _anatomy(spec):
    points = [(.51, 0, 1.86), (.85, 0, 1.48), (1.17, 0, 1.44),
              (1.35, 0, 1.85), (1.33, 0, 2.30), (1.36, 0, 2.77)]
    for i in range(2):
        set_bone(spec.bones, f"neck{i + 1}", points[i], points[i + 1])
    for i in range(2, 5):
        spec.bones.append(Bone(f"neck{i + 1}", f"neck{i}", points[i], points[i + 1]))
    head = next(bone for bone in spec.bones if bone.name == "head")
    head.parent = "neck5"
    set_bone(spec.bones, "head", points[-1], (1.90, 0, 2.68))
    set_bone(spec.bones, "jaw", (1.48, 0, 2.62), (1.94, 0, 2.57))
    set_bone(spec.bones, "ear.L", (1.37, .12, 2.84), (1.31, .19, 2.93))
    spec.rig.neck = [f"neck{i}" for i in range(1, 6)]
    spec.rig.extra["muzzle"] = ("head", (1.96, 0, 2.66))
    spec.parts = [Part("torso", [
        R((-1.00, 0, 1.68), .12, .17, .22), R((-.85, 0, 1.68), .34, .37, .30),
        R((-.52, 0, 1.68), .43, .39, .34), R((-.12, 0, 1.68), .47, .41, .43),
        R((.29, 0, 1.68), .44, .42, .51), R((.57, 0, 1.68), .33, .39, .49),
        R((.74, 0, 1.68), .20, .28, .32)],
        chain=chain(spec.bones, "hips", "spine1", "spine2", "chest"), segments=28, cap_scale=.35),
        Part("neck", [R(points[0], .24, .28), R(points[1], .20, .24),
                      R(points[2], .17, .21), R(points[3], .16, .20),
                      R(points[4], .14, .18), R(points[5], .13, .16)],
             chain=chain(spec.bones, *spec.rig.neck), segments=24, cap_start="flat", cap_end="flat"),
        Part("head", [R((1.29, 0, 2.79), .12, .16, .13),
                      R((1.47, 0, 2.80), .16, .18, .16),
                      R((1.67, 0, 2.72), .125, .11, .12),
                      R((1.89, 0, 2.67), .095, .08, .09, n=2.7)],
             weights={"head": 1.0}, segments=22, cap_scale=.30, tag="head"),
        Part("jaw", [R((1.48, 0, 2.62), .12, .045, .065),
                     R((1.87, 0, 2.57), .085, .04, .045)],
             weights={"jaw": 1.0}, segments=16, tag="jaw"),
        Part("ear.L", [R((1.37, .12, 2.84), .055, .05), R((1.31, .19, 2.92), .04, .055)],
             weights={"ear.L": 1.0}, mirror=True, segments=14, tag="ear"),
        Part("eye.L", [R((1.55, .141, 2.81), .023, .023), R((1.57, .143, 2.81), .022, .022)],
             weights={"head": 1.0}, mirror=True, fuse=False, segments=12, tag="eye"),
    ]
    # Upright closed lofts keep each hump rounded without bridging their saddle.
    for label, x, bone in (("rear", -.62, "spine1"), ("front", .20, "chest")):
        spec.parts.append(Part("hump." + label, [
            R((x, 0, 1.93), .31, .35), R((x, 0, 2.17), .32, .34),
            R((x, 0, 2.44), .28, .29), R((x, 0, 2.63), .16, .17)],
            weights={bone: 1.0}, segments=28, cap_scale=.65, tag="hump"))
    for front, x, y, top in ((True, .46, .28, 1.90), (False, -.75, .28, 1.83)):
        points = ([(x, y, top + .10), (x, y, top), (x - .16, y, .94),
                   (x + .04, y, .30), (x + .06, y, .09), (x + .20, y, .05)] if front else
                  [(x, y, top), (x + .27, y, 1.03), (x - .11, y, .40),
                   (x + .02, y, .09), (x + .16, y, .05)])
        limb_chain = leg_bones(spec.bones, front, points)
        label = "front" if front else "hind"
        knee = (x - .16, .94) if front else (x + .27, 1.03)
        spec.parts.append(Part(label + "_leg.L", [
            R((x, .14, top), .11, .15), R((x, .25, 1.60), .13, .16),
            R((x, y, 1.45), .12, .15),
            R((knee[0], y, knee[1]), .11, .125),
            R((x - .05, y, .53), .066, .08), R((x + .06, y, .045), .085, .085)],
            chain=limb_chain, mirror=True, segments=18, tag="leg", cap_start="flat", cap_end="flat"))
        spec.parts.extend(split_foot(label + "_pad", x + .09, y, .16, .32, .10,
                                    "fingers.L" if front else "toes.L", tag="pad", exponent=2.3))
    tail_points = [(-.96, 0, 1.77), (-1.15, 0, 1.50), (-1.20, 0, 1.24),
                   (-1.23, 0, 1.02), (-1.26, 0, .87)]
    for i in range(4):
        set_bone(spec.bones, f"tail{i + 1}", tail_points[i], tail_points[i + 1])
    spec.parts.append(Part("tail", [R(point, .045 - .005 * i, .045 - .005 * i)
        for i, point in enumerate(tail_points)],
        chain=chain(spec.bones, "tail1", "tail2", "tail3", "tail4"), segments=12, tag="tail"))


def _paint(ctx):
    sand = np.array([.56, .37, .20])
    pale = np.array([.77, .63, .43])
    grain = nz.fbm(ctx.P, 32.0, 3, seed=21)
    under = nz.smoothstep(.15, -.70, ctx.N[..., 2])
    coat = nz.mix(sand, pale, under * .40) * (.90 + .16 * grain)[..., None]
    coat = nz.mix(coat, np.array([.16, .10, .055]), ctx.tag("eye"))
    coat = nz.mix(coat, np.array([.32, .22, .13]), ctx.tag("pad") * .7)
    # A fine vertical philtrum suggests the divided mobile upper lip.
    lip = nz.smoothstep(1.82, 1.93, ctx.x) * nz.smoothstep(.018, .004, np.abs(ctx.y))
    coat = nz.mix(coat, np.array([.23, .14, .075]), lip * ctx.tag("head"))
    return coat


def make_spec():
    spec = make_wild_quadruped({
        "name": "camel", "kind": "camel", "common_name": "Bactrian camel",
        "scientific_name": "Camelus bactrianus", "height": 2.1, "length": 3.0,
        "walk_speed": 1.1, "trot_speed": 2.4,
        "walk_pattern": "pace", "fast_pattern": "pace",
        "head_down": quad.HeadDown(body_pitch=12, crouch=.24, chest=8,
                                   neck=(8, 18, 32, 40, 45), head=15),
        "sleep_neck": (15, 20, 30, 35, 40), "sleep_head": 10,
        "lie_drop": .90,
    })
    _anatomy(spec)
    spec.paint = _paint
    spec.voxel = .014
    spec.smooth_iters = 5
    spec.meta["roughness"] = .94
    return spec
