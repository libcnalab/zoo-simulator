# SPDX-License-Identifier: MIT
"""A ground snake with an articulated backbone from the tail to the snout."""
import math
from functools import partial

import numpy as np

from . import noise
from .geom import Bone, Part, R, Spec
from .rig import Clip, Pose, Rig

SEGMENTS = 24


def _width(t, python):
    # Tail tip, vent, broad trunk, neck, jaw and the rounded snout.
    stations = [0, .035, .10, .17, .30, .54, .72, .84, .90, .925, .948, .972, .992, 1]
    widths = ([.003, .010, .026, .042, .063, .068, .055, .038, .026, .028, .045, .040, .022, .015]
              if python else
              [.002, .005, .010, .016, .023, .026, .023, .018, .013, .013, .023, .021, .012, .008])
    return np.interp(t, stations, widths)


def _colour(ctx, length, python):
    u = ctx.x / length + .5
    theta = np.arctan2(ctx.N[..., 1], ctx.N[..., 2])
    wear = noise.fbm(ctx.P / length, 86, 2, seed=37)
    belly = noise.smoothstep(1.76, 2.15, np.abs(theta))
    if python:
        skin = noise.mix([.43, .28, .14], [.71, .56, .35], wear)
        # Dark, slightly ragged transverse bands stop where the pale belly begins.
        band = np.cos(math.tau * (u * 28 + .13 * np.sin(2 * theta + 23 * u) + .10 * wear))
        skin = noise.mix(skin, [.17, .11, .075], noise.smoothstep(-.17, .13, band))
        skin = noise.mix(skin, [.78, .66, .47], belly)
        skin = noise.mix(skin, [.055, .049, .043], noise.smoothstep(.874, .891, u))
    else:
        skin = noise.mix([.63, .31, .135], [.90, .60, .30], wear)
        phase = u * 26 + .1 * np.sin(u * 55) + .09 * np.cos(3 * theta)
        long = np.abs((phase + .5) % 1 - .5) / .36
        dorsal = (long ** 3 + (np.abs(theta) / 1.04) ** 3) ** (1 / 3)
        lateral = ((np.abs((phase % 1) - .5) / .32) ** 2 +
                   ((np.abs(theta) - 1.48) / .46) ** 2) ** .5
        edge = np.minimum(dorsal, lateral) + .08 * (wear - .5)
        blotch = noise.mix([.53, .15, .08], [.72, .23, .11], wear)
        marks = noise.mix([.17, .09, .055], blotch, 1 - noise.smoothstep(.77, .88, edge))
        skin = noise.mix(skin, marks, 1 - noise.smoothstep(.95, 1.02, edge))
        check = (np.floor(u * 58) + (theta > 0)) % 2
        underside = noise.mix([.84, .76, .60], [.15, .12, .10], check * .94)
        skin = noise.mix(skin, underside, belly)
    skin = noise.mix(skin, [.026, .021, .017], ctx.tag("eye"))
    return noise.mix(skin, [.25, .12, .11], ctx.tag("tongue"))


def _relief(ctx, length):
    u = ctx.x / length
    angle = np.arctan2(ctx.N[..., 1], ctx.N[..., 2])
    # Offset scale rows wrap around the dorsum. Broad transverse scutes cross the belly.
    row = angle * 22 / math.tau
    x = 110 * u + .5 * (np.floor(row) % 2)
    scales = np.maximum(0, 1 - (2 * ((x % 1) - .5)) ** 2 - (2 * ((row % 1) - .5)) ** 2)
    ventral = .5 + .5 * np.cos(math.tau * 92 * u)
    return .45 + .18 * noise.mix(scales[..., None], ventral[..., None],
                                  noise.smoothstep(1.8, 2.2, np.abs(angle)))[..., 0]


def _pose(joints, length, name, phase):
    pose = Pose()
    wave = math.tau * phase
    moving = name in ("walk", "trot")
    resting = name in ("rest", "sleep")
    previous = 0
    centre = np.zeros(2)
    spacing = length / SEGMENTS
    for i, joint in enumerate(joints):
        u = (i + .5) / SEGMENTS
        if moving:
            angle = (35 if name == "walk" else 44) * math.sin(math.tau * 1.22 * u + wave)
        elif resting:
            angle = -160 + 317 * u + 2.4 * u ** 4 * math.sin(wave)
        else:
            angle = 38 * math.sin(math.tau * 1.12 * u + .5) + 8 * u ** 3 * math.sin(wave)
        if name == "vocalize":
            angle += 13 * u ** 4 * math.sin(wave)
        pose.r(joint, rz=angle - previous)
        previous = angle
        if i < SEGMENTS // 2:
            rad = math.radians(angle)
            centre += spacing * np.array([math.cos(rad), math.sin(rad)])
    # Bending about the tail alone would shift the entire animal on the floor.
    pose.move("root", dx=length / 2 - centre[0], dy=-centre[1])
    if name in ("idle", "vocalize"):
        pose.r(joints[-3], ry=-6 - 2 * math.sin(wave))
    if name == "drink":
        # Small alternating lower-jaw movements at the edge of the water.
        pose.r("jaw", ry=1.3 + 1.2 * math.sin(2 * wave))
    if name == "eat":
        pose.r("jaw", ry=6 + 5 * math.sin(wave))
    if name in ("idle", "walk", "trot", "vocalize"):
        flick = max(0, math.sin(wave)) ** 8
        extension = .021 * length * flick
        heading = math.radians(previous)
        pose.move("tongue", dx=extension * math.cos(heading), dy=extension * math.sin(heading))
    return pose


def _clips(solver, joints, length, python):
    pace = .15 if python else .22
    return [Clip(name, duration, partial(_pose, joints, length, name), speed=speed)
            for name, duration, speed in [
                ("idle", 4.2, 0), ("walk", 2.2, pace), ("trot", 1.45, pace * 1.8),
                ("rest", 6, 0), ("sleep", 7, 0), ("eat", 3, 0),
                ("drink", 3.8, 0), ("vocalize", 2.2, 0)]]


def make_serpent(python):
    length = 1.90 if python else 1.32
    height = .113 if python else .045
    name = "black_headed_python" if python else "corn_snake"
    names = [f"spine_{i:02d}" for i in range(SEGMENTS)]
    names[SEGMENTS // 2] = "hips"
    names[-5], names[-3], names[-1] = "chest", "neck", "head"

    def point(u):
        width = _width(u, python)
        return np.array([(u - .5) * length, 0, width * .73 + .004])

    bones = [Bone("root", None, np.zeros(3), np.array([.02, 0, 0]), False)]
    for i, joint in enumerate(names):
        bones.append(Bone(joint, names[i - 1] if i else "root", point(i / SEGMENTS),
                          point((i + 1) / SEGMENTS)))
    head = point(.963)
    snout = point(1)
    bones.append(Bone("jaw", "head", head, snout))
    bones.append(Bone("tongue", "head", point(.986), snout + [.01, 0, 0]))
    chain = [(b.name, b.head, b.tail) for b in bones[1:1 + SEGMENTS]]
    rings = []
    for u in np.linspace(0, 1, 89):
        w = _width(u, python)
        rings.append(R(point(u), w, .88 * w, .73 * w, n=2.25))
    parts = [Part("body", rings, chain=chain, segments=24, subdiv=2, cap_scale=.42)]
    eye = point(.970)
    eye[1] = _width(.970, python) * .94
    eye[2] += _width(.970, python) * .32
    radius = .0057 if python else .0039
    parts.append(Part("eye.L", [R(eye, radius, radius)], weights={"head": 1}, mirror=True,
                      fuse=False, tag="eye", side_ref=(1, 0, 0), cap_scale=.65))
    parts.append(Part("lower_jaw", [R(head - [0, 0, radius], _width(.963, python) * .85, radius),
                                    R(snout - [.004, 0, radius], _width(1, python), radius * .7)],
                      weights={"jaw": 1}, fuse=False, tag="jaw"))
    # At rest the fork is withdrawn into the mouth. Only the tips divide.
    for side in (-1, 1):
        tip = snout - [.006 * length, -side * .003 * length, .002]
        parts.append(Part("tongue_" + str(side), [R(snout - [.020 * length, 0, .002], radius * .4, radius * .29),
                                                  R(tip, radius * .12, radius * .1)],
                          weights={"tongue": 1}, fuse=False, tag="tongue", segments=8, cap_end="point"))
    rig = Rig(body="hips", legs={}, chest="chest", neck=["neck"], head="head", jaw="jaw")
    meta = dict(common_name="Black-headed python" if python else "Eastern corn snake",
                scientific_name="Aspidites melanocephalus" if python else "Pantherophis guttatus",
                body_length_m=length, shoulder_height_m=height, roughness=.58)
    return Spec(name=name, bones=bones, parts=parts, rig=rig, clips=partial(_clips, joints=names, length=length,
                python=python), paint=partial(_colour, length=length, python=python),
                surface_height=partial(_relief, length=length), surface_strength=.75,
                voxel=.0045 if python else .0029, target_tris=9500, texture_size=1024, meta=meta)
