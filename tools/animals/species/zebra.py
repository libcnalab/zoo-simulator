# SPDX-License-Identifier: MIT
"""Plains zebra, adult. Shoulder height 1.35 m; head and body length 2.30 m."""
from zoogen.wild_quadruped import make_wild_quadruped


def make_spec():
    return make_wild_quadruped({
        "name": "zebra",
        "kind": "zebra",
        "common_name": "Plains zebra",
        "scientific_name": "Equus quagga",
        "height": 1.35,
        "length": 2.3,
        "walk_speed": 1.5,
        "trot_speed": 3.0,
    })
