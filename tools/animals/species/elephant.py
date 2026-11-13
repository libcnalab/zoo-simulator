# SPDX-License-Identifier: MIT
"""African bush elephant bull: anatomical edits of exegetpro's Asian source."""
from zoogen.pachyderm import make_pachyderm

PROPORTIONS = {'hips': {'scale': [1.16, 1, 1.10]}, 'head': {'offset': [0, 0, .10]}}
SHAPE = {'ear_scale': [1.3, 2.2, 2.2], 'ear_sweep': .12, 'saddle': .32, 'dome': .13,
         'tusk_length': 1.7, 'tusk_width': 1.2, 'normal_strength': 1.15,
         'color_saturation': .12, 'color_gain': 1.4}


def make_spec():
    return make_pachyderm('elephant', 'elephant', 3.2, PROPORTIONS, SHAPE, 1.3, 2.8,
                          {'common_name': 'African bush elephant', 'scientific_name': 'Loxodonta africana'})
