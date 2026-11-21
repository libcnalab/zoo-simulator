# SPDX-License-Identifier: MIT
"""Adult female polar bear: smaller and less massively necked than the male."""
from zoogen.bear import make_bear

EDITS = {
    'hips': {'scale': [1.16, .90, 1]},
    'neck1': {'scale': [.96, .86, .90]},
    'neck2': {'scale': [1.02, .90, 1]},
    'chest': {'scale': [1, .94, 1]},
    'head': {'scale': [.78, .83, .90]},
    'ear': {'scale': [.72, .72, .72]},
    'hand': {'scale': [1.15, 1.18, 1]},
    'foot': {'scale': [1.12, 1.20, 1]},
}


def make_spec():
    return make_bear('polar_bear_female', True, False, 1.335, EDITS,
                     {'variant_of': 'polar_bear', 'variant': 'female',
                      'relative_height': .89, 'sex': 'female'})
