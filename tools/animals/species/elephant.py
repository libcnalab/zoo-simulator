# SPDX-License-Identifier: MIT
"""African bush elephant bull: anatomical edits of exegetpro's Asian source."""
from zoogen.pachyderm import make_pachyderm

PROPORTIONS = {'hips': {'scale': [1.16, 1, 1.10]}, 'head': {'offset': [0, 0, .10]}}
SHAPE = {'ear_scale': [1.3, 2.2, 1.9], 'ear_sweep': .12, 'saddle': .32, 'dome': .13,
         'tusk_length': 0.83, 'tusk_width': 0.85, 'normal_strength': .8,
         'skin_floor': .105, 'skin_contrast': 1.45, 'forehead_recede': .10, 'crown_lower': .06}


def make_spec():
    return make_pachyderm('elephant', 'elephant', 3.2, PROPORTIONS, SHAPE, 1.3, 2.8,
                          {'common_name': 'African bush elephant', 'scientific_name': 'Loxodonta africana'})
