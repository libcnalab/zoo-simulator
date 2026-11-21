# SPDX-License-Identifier: MIT
"""Southern white-rhinoceros cow: slightly smaller, with longer slender horns."""
from zoogen.pachyderm import make_pachyderm

PROPORTIONS = {'tail1': {'rotate': [0, -48, 0]}, 'neck2': {'rotate': [0, 6, 0]}}
SHAPE = {'normal_strength': .9, 'color_saturation': .45, 'color_gain': 1.05,
         'horn_length': 1.12, 'horn_width': .80}


def make_spec():
    return make_pachyderm('rhino_female', 'rhino', 1.746, PROPORTIONS, SHAPE, 1.2, 3.0,
                          {'common_name': 'Southern white rhinoceros cow',
                           'scientific_name': 'Ceratotherium simum simum',
                           'variant_of': 'rhino', 'variant': 'female',
                           'relative_height': .97})
