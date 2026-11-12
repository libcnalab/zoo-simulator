# SPDX-License-Identifier: MIT
"""Grey wolf, adult. Shoulder height 0.80 m; head and body length 1.50 m."""
from zoogen.wild_quadruped import make_wild_quadruped


def make_spec():
    return make_wild_quadruped({
        "name": "wolf",
        "kind": "wolf",
        "common_name": "Grey wolf",
        "scientific_name": "Canis lupus",
        "height": 0.8,
        "length": 1.5,
        "walk_speed": 1.2,
        "trot_speed": 2.4,
    })
