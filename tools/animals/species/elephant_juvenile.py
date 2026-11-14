# SPDX-License-Identifier: MIT
"""One to two year African elephant calf with no tusks and a shorter, thinner trunk."""
from zoogen.pachyderm import make_pachyderm

PROPORTIONS = {'hips': {'scale': [.88, .95, 1]}, 'head': {'scale': [1.25, 1.14, 1.16]},
               'trunk1': {'scale': [.72, .76, .76]}}
SHAPE = {'ear_scale': [1.2, 2.1, 1.9], 'ear_sweep': .10, 'saddle': -.04, 'dome': .12,
         'tusk_length': .05, 'tusk_width': .2, 'hide_tusks': True, 'normal_strength': .45,
         'calf_skin': True, 'skin_contrast': .8, 'skin_floor': .14}


def make_spec():
    return make_pachyderm('elephant_juvenile', 'elephant', 1.2, PROPORTIONS, SHAPE, .9, 2.2,
                          {'common_name': 'African bush elephant calf', 'scientific_name': 'Loxodonta africana',
                           'variant_of': 'elephant', 'variant': 'juvenile'})
