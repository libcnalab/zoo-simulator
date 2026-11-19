# SPDX-License-Identifier: MIT
"""Shared pieces of the activity clips (sentry, dig, preen, dust bathing): the still
behaviours that the species routines give an animal, beyond the eight standard clips.

The game plays one of these where a species' manifest maps the activity to it
(`meta.activity_clips`, read by src/game/AnimalRenderer.cpp).
"""
import math

import numpy as np

from .rig import smooth


def held(u, keys):
    """A value that moves between held keys: `keys` is [(u, value), ...] in ascending u on
    [0, 1); the value holds at each key's own value until the next key's u - its `move`,
    and eases to it in that span. Cyclic: after the last key it returns to the first.

    Each key is (u, value) or (u, value, move); `move` defaults to 0.06 of the clip.
    Values may be numbers or numpy arrays."""
    count = len(keys)
    for index in range(count):
        start = keys[index]
        end = keys[(index + 1) % count]
        end_u = end[0] if index + 1 < count else end[0] + 1.0
        here = u if u >= start[0] else u + 1.0
        if start[0] <= here < end_u or count == 1:
            move = end[2] if len(end) > 2 else .06
            begin = end_u - move
            if here <= begin:
                return np.asarray(start[1], dtype=float) if isinstance(start[1], (list, tuple)) else start[1]
            s = smooth((here - begin) / move)
            a = np.asarray(start[1], dtype=float)
            b = np.asarray(end[1], dtype=float)
            value = a + (b - a) * s
            return value if value.ndim else float(value)
    return keys[0][1]


def nibble(u, rate, amount):
    """Small quick bill or muzzle movements (degrees) that close over the loop: `rate` must be
    a whole number of nibbles per clip."""
    return amount * math.sin(math.tau * rate * u) * (0.6 + 0.4 * math.sin(math.tau * 3 * u))


def turn_about_vertical(solver, pose, bone, degrees):
    """Turns `bone` and everything it carries about the upright through the bone's head, by
    `degrees` (+ to the left), whatever its parent's posture: a long neck laid out by a
    sagittal profile then swings sideways as one piece."""
    from .rig import rot_z
    parent = solver.parent[bone]
    world = solver.solve(pose)
    carried = world[parent][:3, :3] @ solver.rest[parent][:3, :3].T
    local = pose.rot.get(bone, np.eye(3))
    pose.rot[bone] = carried.T @ rot_z(degrees) @ carried @ local
    return pose
