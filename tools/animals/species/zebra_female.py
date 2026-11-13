# SPDX-License-Identifier: MIT
"""Adult mare: lighter neck and an independent stripe variation."""
from species.zebra import make_variant


def make_spec():
    return make_variant('zebra_female', 1.296, 'female',
                        {'neck1': {'scale': [1, .87, .98]},
                         'neck2': {'scale': [1, .95, 1]}})
