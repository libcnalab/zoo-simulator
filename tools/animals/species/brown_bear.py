# SPDX-License-Identifier: MIT
"""Autumn Eurasian brown bear, derived from Pebegou's upright sculpt."""
from zoogen.bear import make_bear

EDITS = {
    'hips': {'scale': [1.12, 1, 1]},
    'neck1': {'scale': [.64, 1.04, 1]},
    'head': {'scale': [.94, 1.05, 1]},
    'ear': {'scale': [.85, .85, .85]},
}


def make_spec():
    return make_bear('brown_bear', False, False, 1.2, EDITS)
