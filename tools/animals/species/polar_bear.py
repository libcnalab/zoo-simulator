# SPDX-License-Identifier: MIT
"""Polar conversion: extended neck/body, narrow straight head, broad paws, cream coat."""
from zoogen.bear import make_bear

EDITS = {
    'hips': {'scale': [1.16, .94, 1]},
    'neck1': {'scale': [.96, .92, .90]},
    'neck2': {'scale': [1.02, .96, 1]},
    'head': {'scale': [.78, .83, .90]},
    'ear': {'scale': [.72, .72, .72]},
    'hand': {'scale': [1.15, 1.18, 1]},
    'foot': {'scale': [1.12, 1.20, 1]},
}


def make_spec():
    return make_bear('polar_bear', True, False, 1.5, EDITS)
