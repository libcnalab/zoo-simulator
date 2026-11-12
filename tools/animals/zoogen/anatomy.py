# SPDX-License-Identifier: MIT
"""Optional anatomical helpers for species with ungulate or pillar limbs.

These helpers are called explicitly by species definitions; existing builders and
lofts keep their original defaults.
"""
from .geom import Part, R


def chain(bones, *names):
    by_name = {bone.name: bone for bone in bones}
    return [(name, by_name[name].head, by_name[name].tail) for name in names]


def set_bone(bones, name, head, tail):
    bone = next(bone for bone in bones if bone.name == name)
    bone.head = head
    bone.tail = tail


def leg_bones(bones, front, points):
    """Set contiguous joints without changing the standard rig's bone names."""
    names = (("scapula.L", "upper_arm.L", "forearm.L", "hand.L", "fingers.L") if front else
             ("thigh.L", "shin.L", "foot.L", "toes.L"))
    for index, name in enumerate(names):
        set_bone(bones, name, points[index], points[index + 1])
    return chain(bones, *names)


def round_foot(name, x, y, radius, height, bone, tag="foot"):
    """A flat sole and upright side wall, rather than a forward-projecting paw."""
    return Part(name, [R((x, y, height), radius * .86, radius * .86),
                       R((x, y, height * .50), radius, radius),
                       R((x, y, .012), radius, radius)],
                weights={bone: 1.0}, mirror=True, segments=20, tag=tag,
                cap_start="flat", cap_end="flat")


def split_foot(name, x, y, width, length, height, bone, tag="hoof", exponent=3.0):
    """Separate left and right digits with a visible cleft at the toe."""
    parts = []
    for digit in (-1, 1):
        offset = digit * width * .52
        parts.append(Part(f"{name}.{digit}.L", [
            R((x - length * .36, y + offset, height * .55), width * .45, height * .44, n=exponent),
            R((x + length * .30, y + offset, height * .50), width * .46, height * .45, n=exponent),
        ], weights={bone: 1.0}, mirror=True, fuse=False, segments=12, tag=tag,
            cap_scale=.20))
    return parts
