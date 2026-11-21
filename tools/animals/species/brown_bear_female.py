# SPDX-License-Identifier: MIT
"""Adult female Eurasian brown bear: smaller and more lightly built than the male."""
from zoogen.bear import make_bear

EDITS = {
    'hips': {'scale': [1.12, .96, 1]},
    'neck1': {'scale': [.64, .98, 1]},
    'chest': {'scale': [1, .95, 1]},
    'head': {'scale': [.94, 1.03, 1]},
    'ear': {'scale': [.85, .85, .85]},
    'upper_arm': {'scale': [1, .96, 1]},
    'thigh': {'scale': [1, .97, 1]},
}


def make_spec():
    return make_bear('brown_bear_female', False, False, 1.104, EDITS,
                     {'variant_of': 'brown_bear', 'variant': 'female',
                      'relative_height': .92, 'sex': 'female'})
