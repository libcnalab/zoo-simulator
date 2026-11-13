# SPDX-License-Identifier: MIT
"""Bengal tiger: 1.0 m shoulders, elongated trunk, broad forepaws and cheek ruff."""
from zoogen.bigcat import make_cat

PROPORTIONS = {
    'hips': {'scale': [1.26, 1.1, 1]},
    'spine1': {'offset': [.10, 0, 0]},
    'neck1': {'scale': [.75, 1.12, 1]},
    # Chest depth must not lengthen the forelegs and lift the hind paws off the ground.
    'scapula': {'scale': [1, 1, 0.970873786407767]},
    'chest': {'scale': [1, 1.08, 1.03]},
    'head': {'scale': [.82, 1.24, 1.25]},
    'ear': {'scale': [1, 1, .80]},
    'fingers': {'scale': [1.12, 1.16, 1]},
}


def make_spec():
    return make_cat('tiger', 'tiger', 1.0, PROPORTIONS, 1.0, 2.5)
