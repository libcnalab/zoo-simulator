# SPDX-License-Identifier: MIT
"""Southern white rhinoceros, using LasquetiSpice's licensed skin and sculpt."""
from zoogen.pachyderm import make_pachyderm

PROPORTIONS = {'tail1': {'rotate': [0, -48, 0]}, 'neck2': {'rotate': [0, 6, 0]}}
SHAPE = {'normal_strength': .9, 'color_saturation': .45, 'color_gain': 1.05}


def make_spec():
    return make_pachyderm('rhino', 'rhino', 1.8, PROPORTIONS, SHAPE, 1.2, 3.0,
                          {'common_name': 'Southern white rhinoceros',
                           'scientific_name': 'Ceratotherium simum simum'})
