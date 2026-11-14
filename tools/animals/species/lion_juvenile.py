# SPDX-License-Identifier: MIT
"""Four-to-eight-month lion cub: 0.45 m shoulders, short legs and enlarged head/paws."""
from zoogen.bigcat import make_cat

PROPORTIONS = {
    'hips': {'scale': [.92, 1.12, .82]},
    'head': {'scale': [1.22, 1.45, 1.40], 'eye_scale': 1.35, 'muzzle_scale': .75},
    'ear': {'scale': [1.25, 1.20, .90]},
    'upper_arm': {'scale': [1.0, 1.0, 1.0]},
    'forearm': {'scale': [1.0, 1.10, .80]},
    'thigh': {'scale': [1.0, 1.0, 1.0]},
    'shin': {'scale': [1.0, 1.10, .80]},
    'fingers': {'scale': [1.25, 1.24, 1.12]},
    'toes': {'scale': [1.25, 1.24, 1.12]},
    'tail1': {'scale': [.78, .9, .78]},
}


def make_spec():
    return make_cat('lion_juvenile', 'juvenile', .45, PROPORTIONS, .6, 1.5,
                    {'variant_of': 'lion', 'variant': 'juvenile'})
