# SPDX-License-Identifier: MIT
"""African bush elephant cow with a lighter head and short slender tusks."""
from zoogen.pachyderm import make_pachyderm

PROPORTIONS = {'hips': {'scale': [1.13, .87, 1.10]}, 'head': {'scale': [.91, .93, .95],
                'offset': [0, 0, .1]}, 'neck1': {'scale': [1, .94, 1]}}
SHAPE = {'ear_scale': [1.3, 2.2, 2.2], 'ear_sweep': .12, 'saddle': .30, 'dome': .08,
         'tusk_length': .85, 'tusk_width': .65, 'normal_strength': 1.05,
         'color_saturation': .12, 'color_gain': 1.4}


def make_spec():
    return make_pachyderm('elephant_female', 'elephant', 2.6, PROPORTIONS, SHAPE, 1.2, 2.8,
                          {'common_name': 'African bush elephant cow', 'scientific_name': 'Loxodonta africana',
                           'variant_of': 'elephant', 'variant': 'female', 'relative_height': .81})
