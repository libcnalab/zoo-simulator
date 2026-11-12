# SPDX-License-Identifier: MIT
"""Adult reticulated giraffe: 3.1 m at the withers, 5.2 m to the head top.

The simulation's 5.2 m height is the top of the head. The model metadata uses
that same measurement; the head-and-body length is about 4.3 m without the tail.
"""
import math

import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Bone, Part, R
from zoogen.anatomy import chain, leg_bones, set_bone, split_foot
from zoogen.wild_quadruped import make_wild_quadruped


def _extend_neck(spec):
    old_head = next(b for b in spec.bones if b.name == "head")
    points = [tuple(old_head.tail), (1.75, 0.0, 3.25), (1.97, 0.0, 3.81),
              (2.19, 0.0, 4.37), (2.41, 0.0, 4.93)]
    parent = "head"
    chain = []
    for i in range(4):
        name = f"neck{i + 3}"
        spec.bones.append(Bone(name, parent, points[i], points[i + 1]))
        chain.append((name, points[i], points[i + 1]))
        parent = name
    spec.bones.append(Bone("giraffe_head", parent, points[-1], (2.78, 0.0, 5.04)))
    spec.bones.append(Bone("giraffe_jaw", "giraffe_head", (2.63, 0.0, 4.93),
                           (3.01, 0.0, 4.91)))
    chain.insert(0, ("head", old_head.head, old_head.tail))
    rings = [R((.48, 0, 2.62), .32, .52, .40), R((.78, 0, 2.76), .30, .38, .34),
             R((1.10, 0, 2.88), .27, .30, .29), R((1.48, 0, 3.01), .24, .27, .25)]
    rings.extend(R(p, .22 - .01 * i, .25 - .01 * i, .23 - .01 * i)
                 for i, p in enumerate(points[1:]))
    spec.parts = [p for p in spec.parts if p.name not in
                  ("neck", "skull", "jaw", "ear.L", "eye.L", "nose")]
    spec.parts.append(Part("long_neck", rings, chain=chain, segments=22,
                           tag="neck", cap_start="flat", cap_end="flat"))
    spec.parts.append(Part("head", [R((2.54,0,4.98),.20,.23,.20),
        R((2.71,0,5.00),.18,.19,.17), R((2.92,0,4.93),.16,.13,.14,n=2.6)],
        weights={"giraffe_head":1.0}, segments=18, tag="head"))
    spec.parts.append(Part("muzzle", [R((2.85,0,4.90),.14,.10,.10,n=2.7),
        R((3.03,0,4.88),.12,.075,.08,n=2.8)],
        weights={"giraffe_head":1.0}, segments=16, tag="muzzle"))
    spec.parts.append(Part("jaw", [R((2.66,0,4.82),.13,.06,.07),
        R((2.97,0,4.81),.12,.05,.06)], weights={"giraffe_jaw":1.0},
        segments=14, tag="jaw"))
    for side in (-1.0, 1.0):
        suffix = "L" if side > 0 else "R"
        spec.parts.append(Part(f"ossicone.{suffix}", [R((2.63,side*.09,5.10),.07,.07),
            R((2.63,side*.09,5.27),.065,.065),R((2.63,side*.09,5.34),.085,.07)],
            weights={"giraffe_head":1.0}, fuse=False, segments=12, tag="ossicone"))
        spec.parts.append(Part(f"tuft.{suffix}", [R((2.63,side*.09,5.27),.08,.07),
            R((2.64,side*.09,5.34),.055,.05)], weights={"giraffe_head":1.0},
            fuse=False, segments=10, tag="tuft", rough=(.012,22.0)))
    # Follow the actual dorsal skin, with roots buried in it so the mane cannot float.
    mane = []
    for index, ring in enumerate(rings):
        before = rings[max(0, index - 1)].c
        after = rings[min(len(rings) - 1, index + 1)].c
        dorsal = np.cross(after - before, np.array([0.0, 1.0, 0.0]))
        dorsal /= np.linalg.norm(dorsal)
        mane.append(R(ring.c + dorsal * ring.up * .97, .027, .045, .02))
    spec.parts.append(Part("mane", mane, chain=chain, fuse=False,
                           segments=10, tag="mane", rough=(.004, 28.0)))
    spec.rig.neck.extend(["neck3", "neck4", "neck5", "neck6"])
    spec.rig.head = "giraffe_head"
    spec.rig.jaw = "giraffe_jaw"
    spec.rig.extra["muzzle"] = ("giraffe_head", (3.06,0,4.89))

def _body(spec):
    spec.parts = [part for part in spec.parts if part.name not in
                  ("torso", "front_leg.L", "hind_leg.L", "front_paw.L", "hind_paw.L", "tail")]
    spec.parts.append(Part("torso", [
        R((-1.18, 0, 2.12), .12, .18, .22),
        R((-1.02, 0, 2.12), .36, .35, .43),
        R((-.70, 0, 2.19), .48, .41, .55),
        R((-.35, 0, 2.36), .51, .43, .56),
        R((.16, 0, 2.44), .49, .50, .62),
        R((.57, 0, 2.53), .42, .57, .65),
        R((.83, 0, 2.53), .29, .43, .48),
    ], chain=chain(spec.bones, "hips", "spine1", "spine2", "chest"), segments=28, cap_scale=.40))
    # Thin long limbs begin within the skin; their roots cannot form balls on the back.
    for front, x, y, top in ((True, .60, .31, 2.65), (False, -.88, .30, 2.37)):
        points = ([(x, y, top + .10), (x, y, top), (x - .20, y, 1.37),
                   (x + .02, y, .35), (x + .02, y, .10), (x + .13, y, .055)] if front else
                  [(x, y, top), (x + .32, y, 1.46), (x - .10, y, .52),
                   (x + .02, y, .10), (x + .13, y, .055)])
        limb_chain = leg_bones(spec.bones, front, points)
        label = "front" if front else "hind"
        rings = ([R((x, .19, top), .12, .17), R((x - .04, y, 2.06), .105, .13),
                  R((x - .15, y, 1.40), .080, .095), R((x - .12, y, 1.26), .060, .07),
                  R((x, y, .65), .052, .06), R((x + .02, y, .095), .055, .065)] if front else
                 [R((x, .17, top), .14, .21), R((x + .16, y, 1.87), .15, .19),
                  R((x + .32, y, 1.46), .085, .11), R((x - .10, y, .52), .060, .07),
                  R((x + .02, y, .095), .052, .06)])
        spec.parts.append(Part(label + "_leg.L", rings, chain=limb_chain, mirror=True,
                               segments=18, tag="leg", cap_start="flat", cap_end="flat"))
        spec.parts.extend(split_foot(label + "_hoof", x + .035, y, .095, .23, .15,
                                    "fingers.L" if front else "toes.L"))
    points = [(-1.10, 0, 2.35), (-1.28, 0, 2.02), (-1.34, 0, 1.64),
              (-1.38, 0, 1.28), (-1.41, 0, .97)]
    for i in range(4):
        set_bone(spec.bones, f"tail{i + 1}", points[i], points[i + 1])
    spec.parts.append(Part("tail", [R(point, .033 - i * .004, .033 - i * .004)
        for i, point in enumerate(points)], chain=chain(spec.bones, "tail1", "tail2", "tail3", "tail4"),
        segments=10, tag="tail"))
    spec.parts.append(Part("tail_tuft", [R((-1.40, 0, 1.17), .065, .08),
        R((-1.43, 0, .94), .07, .08), R((-1.47, 0, .77), .015, .025)],
        weights={"tail4": 1.0}, segments=12, tag="tuft"))
    spec.parts.append(Part("giraffe_eye.L", [R((2.75, .17, 5.05), .027, .027),
        R((2.78, .17, 5.05), .025, .025)], weights={"giraffe_head": 1.0},
        mirror=True, fuse=False, segments=12, tag="eye"))
    for side in (-1, 1):
        spec.parts.append(Part(f"giraffe_ear.{side}", [
            R((2.49, side * .15, 5.07), .055, .07),
            R((2.41, side * .32, 5.12), .09, .10),
            R((2.37, side * .45, 5.16), .025, .035)],
            weights={"giraffe_head": 1.0}, segments=14, fuse=False, tag="ear"))

def _paint(ctx):
    P = ctx.P
    f1, f2, _ = nz.worley(P * np.array([1.0, 1.5, 1.0]), 3.1, seed=38, jitter=.92)
    edge = nz.smoothstep(.035, .11, f2 - f1)
    patch = np.array([.39, .12, .045]) * (.86 + .16 * nz.fbm(P, 22.0, 3, seed=12))[...,None]
    cream = np.array([.86, .73, .49])
    coat = nz.mix(cream, patch, edge)
    coat = nz.mix(coat, np.array([.035, .025, .018]), ctx.tag("eye"))
    face = ctx.tag("head") + ctx.tag("muzzle") + ctx.tag("jaw")
    coat = nz.mix(coat, np.array([.72,.45,.24]), np.clip(face*.55,0,1))
    coat = nz.mix(coat, np.array([.20,.09,.045]), ctx.tag("ossicone")*.55)
    coat = nz.mix(coat, np.array([.10,.065,.035]), ctx.tag("tuft"))
    coat = nz.mix(coat, np.array([.20,.10,.045]), ctx.tag("mane"))
    coat = nz.mix(coat, np.array([.15, .11, .075]), ctx.tag("hoof"))
    # The lower legs and face keep smaller, paler cells than the torso.
    fade = nz.smoothstep(.40, .95, ctx.z)
    coat = nz.mix(cream, coat, fade)
    coat = nz.mix(coat, np.array([.15, .11, .075]), ctx.tag("hoof"))
    return coat


def make_spec():
    spec = make_wild_quadruped({
        "name": "giraffe", "kind": "giraffe", "common_name": "Reticulated giraffe",
        "scientific_name": "Giraffa reticulata", "height": 3.1, "length": 3.3,
        "walk_speed": 1.2, "trot_speed": 2.7,
        "walk_pattern": "pace", "fast_pattern": "gallop",
        "head_down": quad.HeadDown(body_pitch=12,crouch=.18,chest=8,
            neck=(15,20,24,28,32,36),head=14,splay=.22),
        "sleep_neck": (30,30,30,30,30,30), "sleep_head": 30,
        "lie_drop": .62,
        "idle_shift": 0.0, "idle_bob": 0.0, "idle_roll": 0.0,
        "call_neck": -5.0, "call_head": -8.0,
        "validate_mesh": True,
    })
    _extend_neck(spec)
    _body(spec)
    spec.paint = _paint
    spec.voxel = .016
    spec.target_tris = 18000
    spec.smooth_iters = 4
    spec.meta["shoulder_height_m"] = 5.2
    spec.meta["withers_height_m"] = 3.1
    spec.meta["body_length_m"] = 4.3
    spec.meta["roughness"] = .92
    clips = spec.clips

    def pump_neck(solver):
        generated = clips(solver)
        for clip in generated:
            if clip.name != "trot":
                continue
            prior = clip.pose

            def pose(u, old=prior):
                p = old(u)
                pump = 3.0 * math.sin(2.0 * math.pi * u)
                for i in range(3, 7):
                    p.r(f"neck{i}", ry=pump * (i / 6.0))
                return p

            clip.pose = pose
        return generated

    spec.clips = pump_neck
    return spec
