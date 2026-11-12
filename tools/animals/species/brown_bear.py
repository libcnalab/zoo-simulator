# SPDX-License-Identifier: MIT
"""Brown bear, medium adult. Shoulder height 1.20 m; head and body length 2.00 m."""
from zoogen.wild_quadruped import make_wild_quadruped


def make_spec():
    return make_wild_quadruped({
        "name": "brown_bear",
        "kind": "brown_bear",
        "common_name": "Brown bear",
        "scientific_name": "Ursus arctos",
        "height": 1.2,
        "length": 2.0,
        "walk_speed": 1.0,
        "trot_speed": 2.5,
    })
