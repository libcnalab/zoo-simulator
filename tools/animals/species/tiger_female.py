# SPDX-License-Identifier: MIT
"""Adult Bengal tigress: a slightly smaller, lighter-built tiger."""
from zoogen.bigcat import make_cat

PROPORTIONS = {
    'hips': {'scale': [1.23, 1.04, 1]},
    'spine1': {'offset': [.10, 0, 0]},
    'neck1': {'scale': [.78, 1.12, 1], 'offset': [0, 0, -.035]},
    'scapula': {'scale': [1, 1, .98]},
    'chest': {'scale': [1, 1.03, 1.02]},
    'head': {'scale': [.93, 1.22, 1.18]},
    'ear': {'scale': [.70, .70, .48]},
    'forearm': {'scale': [1.03, 1.08, 1]},
    'fingers': {'scale': [1.14, 1.18, 1]},
}


def make_spec():
    return make_cat('tiger_female', 'tiger', .92, PROPORTIONS, 1.0, 2.5,
                    {'variant_of': 'tiger', 'variant': 'female',
                     'relative_height': .92, 'sex': 'female'})
