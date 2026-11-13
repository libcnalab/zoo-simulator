# SPDX-License-Identifier: MIT
"""Long-legged young foal with a shorter trunk, neck and muzzle, and brown stripes."""
from species.zebra import make_variant


def make_spec():
    return make_variant('zebra_juvenile', .90, 'juvenile',
                        {'hips': {'scale': [.90, .88, 1]},
                         'spine1': {'scale': [.97, 1, .97]},
                         'neck1': {'scale': [.93, .87, .93]},
                         'head': {'scale': [.88, 1, .94]},
                         'ear': {'scale': [1.08, 1.08, 1.08]}})
