# SPDX-License-Identifier: MIT
"""Adult male lion: 1.15 m shoulders, broader skull, chest and neck than the lioness."""
from zoogen.bigcat import make_cat

PROPORTIONS = {
    'hips': {'scale': [1.08, 1, 1]},
    # Chest depth must not lengthen the forelegs and lift the hind paws off the ground.
    'scapula': {'scale': [1, 1, 0.9615384615384615]},
    'chest': {'scale': [1.02, 1.18, 1.04]},
    'neck1': {'scale': [1, 1.22, 1.08]},
    'jaw': {'scale': [1.04, 1.08, 1.04]},
    'head': {'scale': [1.10, 1.25, 1.13]},
}


def make_spec():
    return make_cat('lion', 'male', 1.15, PROPORTIONS, 1.1, 2.4, {'sex': 'male'})
