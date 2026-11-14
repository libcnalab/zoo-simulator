# SPDX-License-Identifier: MIT
"""Bengal tiger: 1.0 m shoulders, elongated trunk, broad forepaws and cheek ruff."""
from zoogen.bigcat import make_cat

PROPORTIONS = {
    'hips': {'scale': [1.26, 1.1, 1]},
    'spine1': {'offset': [.10, 0, 0]},
    'neck1': {'scale': [.78, 1.17, 1], 'offset': [0, 0, -.035]},
    # Chest depth must not lengthen the forelegs and lift the hind paws off the ground.
    'scapula': {'scale': [1, 1, 0.970873786407767]},
    'chest': {'scale': [1, 1.08, 1.03]},
    'head': {'scale': [.94, 1.30, 1.26]},
    'ear': {'scale': [.70, .70, .48]},
    'forearm': {'scale': [1.04, 1.14, 1]},
    'fingers': {'scale': [1.17, 1.24, 1]},
}


def make_spec():
    return make_cat('tiger', 'tiger', 1.0, PROPORTIONS, 1.0, 2.5)
