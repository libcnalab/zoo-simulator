# SPDX-License-Identifier: MIT
"""Six-month brown bear cub with a pale collar and shortened juvenile proportions."""
from zoogen.bear import make_bear

EDITS = {
    'hips': {'scale': [.94, 1.04, 1]},
    'neck1': {'scale': [.85, 1.08, .93]},
    'head': {'scale': [.86, 1.22, 1.18]},
    'ear': {'scale': [1.15, 1.15, 1.25]},
    'upper_arm': {'scale': [1, 1.12, .82]},
    'thigh': {'scale': [1, 1.12, .82]},
    'hand': {'scale': [1.14, 1.15, 1.1]},
    'foot': {'scale': [1.14, 1.15, 1.1]},
}


def make_spec():
    return make_bear('brown_bear_juvenile', False, True, .45, EDITS)
