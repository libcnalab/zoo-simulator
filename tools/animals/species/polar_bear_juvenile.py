# SPDX-License-Identifier: MIT
"""Young polar bear: whiter fur, large ears/head/paws and shortened limbs."""
from zoogen.bear import make_bear

EDITS = {
    'hips': {'scale': [1.06, 1, 1]},
    'neck1': {'scale': [1.12, .98, .90]},
    'neck2': {'scale': [1.06, .98, 1]},
    'head': {'scale': [.78, 1.05, 1.10]},
    'ear': {'scale': [1.1, 1.1, 1.2]},
    'upper_arm': {'scale': [1, 1.1, .83]},
    'thigh': {'scale': [1, 1.1, .83]},
    'hand': {'scale': [1.2, 1.25, 1.1]},
    'foot': {'scale': [1.2, 1.25, 1.1]},
}


def make_spec():
    return make_bear('polar_bear_juvenile', True, True, .45, EDITS)
