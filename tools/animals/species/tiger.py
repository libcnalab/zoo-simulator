# SPDX-License-Identifier: MIT
"""Bengal tiger, adult male. Shoulder height 1.00 m; head and body length 2.70 m."""
from zoogen.wild_quadruped import make_wild_quadruped


def make_spec():
    return make_wild_quadruped({
        "name": "tiger",
        "kind": "tiger",
        "common_name": "Bengal tiger",
        "scientific_name": "Panthera tigris tigris",
        "height": 1.0,
        "length": 2.7,
        "walk_speed": 1.0,
        "trot_speed": 2.5,
    })
