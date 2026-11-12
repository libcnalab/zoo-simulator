# SPDX-License-Identifier: MIT
"""Polar bear, adult. Shoulder height 1.50 m; head and body length 2.40 m."""
from zoogen.wild_quadruped import make_wild_quadruped


def make_spec():
    return make_wild_quadruped({
        "name": "polar_bear",
        "kind": "polar_bear",
        "common_name": "Polar bear",
        "scientific_name": "Ursus maritimus",
        "height": 1.5,
        "length": 2.4,
        "walk_speed": 1.0,
        "trot_speed": 2.5,
        "validate_mesh": True,
    })
