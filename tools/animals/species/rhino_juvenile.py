# SPDX-License-Identifier: MIT
"""Six to twelve month white-rhinoceros calf."""
from zoogen.pachyderm import make_pachyderm

PROPORTIONS = {'hips': {'scale': [.84, .92, 1]}, 'head': {'scale': [.75, 1.05, .90]},
               'ear': {'scale': [1.2, 1.2, 1.2]}, 'tail1': {'rotate': [0, -48, 0]}}
SHAPE = {'horn_scale': .22, 'hump_reduce': .18, 'normal_strength': .4,
         'color_saturation': .4, 'color_gain': 1.12}


def make_spec():
    return make_pachyderm('rhino_juvenile', 'rhino', .95, PROPORTIONS, SHAPE, .8, 2.2,
                          {'common_name': 'Southern white rhinoceros calf',
                           'scientific_name': 'Ceratotherium simum simum',
                           'variant_of': 'rhino', 'variant': 'juvenile'})
