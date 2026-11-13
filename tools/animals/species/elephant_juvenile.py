# SPDX-License-Identifier: MIT
"""One to two year African elephant calf with no tusks and a shorter, thinner trunk."""
from zoogen.pachyderm import make_pachyderm

PROPORTIONS = {'hips': {'scale': [.88, .95, 1]}, 'head': {'scale': [1.16, 1.1, 1.1]},
               'trunk1': {'scale': [.85, .8, .85]}}
SHAPE = {'ear_scale': [1.2, 2.1, 2.3], 'ear_sweep': .10, 'saddle': .20, 'dome': .12,
         'tusk_length': .05, 'tusk_width': .2, 'hide_tusks': True, 'normal_strength': .5,
         'color_saturation': .12, 'color_gain': 1.45}


def make_spec():
    return make_pachyderm('elephant_juvenile', 'elephant', 1.2, PROPORTIONS, SHAPE, .9, 2.2,
                          {'common_name': 'African bush elephant calf', 'scientific_name': 'Loxodonta africana',
                           'variant_of': 'elephant', 'variant': 'juvenile'})
