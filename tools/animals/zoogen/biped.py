# SPDX-License-Identifier: MIT
"""Two-legged gaits and continuous articulated profiles, independent of Blender.

Ground contacts are authored in animal space. During stance they travel backwards
at exactly the manifest speed; body rocking never changes the contact position.
"""
import math
from dataclasses import dataclass

import numpy as np

from .anatomy import chain
from .geom import Bone, Part, R
from .quadruped import Gait, fit_drop, foot_path
from .rig import Clip, Leg, Pose


def add_chain(bones, prefix, parent, points):
    """Add connected bones and return their names, in proximal-to-distal order."""
    names = []
    for index, (head, tail) in enumerate(zip(points, points[1:])):
        name = f"{prefix}{index + 1}"
        bones.append(Bone(name, parent, head, tail))
        names.append(name)
        parent = name
    return names


def add_leg(bones, parent, points):
    """A mirrored thigh/hock/shank/foot chain; the last bone carries the digits."""
    names = ("thigh.L", "shin.L", "foot.L", "toes.L")
    for name, head, tail in zip(names, points, points[1:]):
        bones.append(Bone(name, parent, head, tail))
        parent = name
    return {side: Leg(*(name[:-1] + side for name in names)) for side in ("L", "R")}


def webbed_foot(x, y, length, width, height, bone="toes.L"):
    """Broad, low triangular web rather than a mammalian paw pad."""
    return Part("web.L", [
        R((x - length * .25, y, height), width * .38, height * .8),
        R((x + length * .30, y, height), width, height * .75, n=3),
        R((x + length * .55, y, height * .90), width * .85, height * .65, n=3),
    ], weights={bone: 1.0}, mirror=True, fuse=False, segments=18, subdiv=3,
        cap_scale=.12, tag="foot")


def eye_part(center, radius, bone="head"):
    point = np.asarray(center, dtype=float)
    return Part("eye.L", [R(point - [radius * .3, 0, 0], radius, radius),
                           R(point + [radius * .3, 0, 0], radius, radius)],
                weights={bone: 1.0}, mirror=True, fuse=False, segments=12, subdiv=2,
                cap_scale=.4, tag="eye")


@dataclass
class Stride:
    name: str
    speed: float
    period: float
    duty: float
    lift: float
    bob: float = .005
    roll: float = 0.0
    lean: float = 0.0
    wing: float = 0.0
    nod: float = 0.0
    hop: bool = False
    flight_height: float = 0.0
    drop: float = None


def gait_pose(solver, stride, phase, drop):
    rig = solver.rig
    angle = math.tau * phase
    pose = Pose()
    if stride.hop:
        swing = max(0.0, (phase - stride.duty) / (1.0 - stride.duty))
        rise = stride.flight_height * math.sin(math.pi * swing) ** 2
        bob = rise - stride.bob * math.cos(angle)
    else:
        bob = stride.bob * math.cos(2.0 * angle)
    pose.move(rig.body, dz=bob - drop)
    pose.r(rig.body, rx=stride.roll * math.sin(angle), ry=stride.lean)
    if rig.neck:
        pose.r(rig.neck[0], ry=stride.nod * math.cos(2.0 * angle))
    for side in ("L", "R"):
        sign = 1.0 if side == "L" else -1.0
        for index, bone in enumerate(rig.extra.get("wings", {}).get(side, [])):
            pose.r(bone, rx=sign * stride.wing / (index + 1))
    for index, bone in enumerate(rig.tail):
        pose.r(bone, ry=(4.0 * math.sin(angle) if stride.hop else 0.0) / (index + 1))
    path = Gait(stride.name, stride.period, stride.speed * stride.period * stride.duty,
                stride.duty, {}, lift=stride.lift, heel_off=0, land_pitch=0,
                swing_pitch=0, toe_follow=0)
    for key in rig.legs:
        offset = 0.0 if stride.hop or key == "L" else .5
        dx, dz, _, _ = foot_path(path, (phase - offset) % 1.0)
        pose.plant(key, solver.toe_base(key) + [dx, 0, dz])
    return pose


def gait_clip(solver, stride):
    drop = stride.drop
    if drop is None:
        def trial(phase, amount):
            return gait_pose(solver, stride, phase, amount)
        drop = fit_drop(solver, trial, .975, samples=96, most=.7)
    def pose(phase):
        return gait_pose(solver, stride, phase, drop)
    events = [(0.0 if stride.hop or key == "L" else .5, "step") for key in solver.rig.legs]
    result = Clip(stride.name, stride.period, pose, speed=stride.speed, events=events)
    result.drop = drop
    return result


def standing(solver):
    pose = Pose()
    for key in solver.rig.legs:
        pose.plant(key, solver.toe_base(key))
    return pose


def aim_chain(solver, pose, names, points):
    """Aim each segment at an authored smooth profile, preserving every bone length.

    Targets describe directions rather than translations, so the exported skeleton
    cannot stretch. Profiles must begin at the chain's actual attachment point.
    """
    from .rig import frame
    for index, name in enumerate(names):
        world = solver.solve(pose)
        parent = solver.parent[name]
        parent_delta = world[parent][:3, :3] @ solver.rest[parent][:3, :3].T
        rest_direction = solver.tail[name] - solver.head[name]
        desired = np.asarray(points[index + 1]) - np.asarray(points[index])
        absolute = frame(desired, (0, 1, 0)) @ frame(rest_direction, (0, 1, 0)).T
        pose.rot[name] = parent_delta.T @ absolute
    return pose


def copy_pose(pose):
    """Copy a prepared posture before adding periodic breathing or looking motion."""
    result = Pose()
    result.rot = dict(pose.rot)
    result.offset = dict(pose.offset)
    result.feet = dict(pose.feet)
    return result


def sagittal_profile(solver, pose, names, angles):
    """Orient a chain by absolute angles from forward (+90 degrees is vertical)."""
    points = [np.zeros(3)]
    for angle in angles:
        radians = math.radians(angle)
        points.append(points[-1] + [math.cos(radians), 0, math.sin(radians)])
    return aim_chain(solver, pose, names, points)


def target_chain(solver, pose, names, target):
    """Aim a planar support tail along a smooth arc ending at a ground contact.

    A single curvature parameter has a unique continuous solution as the contact
    travels backwards. This avoids redundant-joint branch changes in iterative IK.
    Segment lengths and the support point are both preserved exactly.
    """
    world = solver.solve(pose)
    root = world[names[0]][:3, 3].copy()
    target = np.asarray(target, dtype=float)
    difference = target - root
    if abs(difference[1]) > 1e-8:
        raise ValueError("support tail targets must remain in the sagittal plane")
    distance = float(np.linalg.norm(difference))
    lengths = np.array([np.linalg.norm(solver.tail[name] - solver.head[name]) for name in names])
    if distance >= float(lengths.sum()) * .995:
        raise ValueError("support tail target exceeds its available length")
    parameters = np.linspace(-.5, .5, len(names))

    def resultant(curvature):
        angles = curvature * parameters
        return np.array([np.sum(lengths * np.cos(angles)), np.sum(lengths * np.sin(angles))])

    low, high = 0.0, math.radians(300)
    if np.linalg.norm(resultant(high)) > distance:
        raise ValueError("support tail contact is too close for a smooth arc")
    for _ in range(36):
        middle = (low + high) * .5
        if np.linalg.norm(resultant(middle)) > distance:
            low = middle
        else:
            high = middle
    curvature = (low + high) * .5
    vector = resultant(curvature)
    direction = math.atan2(difference[2], difference[0]) - math.atan2(vector[1], vector[0])
    points = [root]
    for length, parameter in zip(lengths, parameters):
        angle = direction + curvature * parameter
        points.append(points[-1] + length * np.array([math.cos(angle), 0, math.sin(angle)]))
    return aim_chain(solver, pose, names, points)
