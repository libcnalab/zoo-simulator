# SPDX-License-Identifier: MIT
"""Adult lioness: 1.05 m shoulder height, slim neck and the sculpt's proportions."""
from zoogen.bigcat import make_cat

PROPORTIONS = {'neck1': {'scale': [1, .96, 1]}}


def make_spec():
    return make_cat('lion_female', 'female', 1.05, PROPORTIONS, 1.1, 2.4,
                    {'variant_of': 'lion', 'variant': 'female', 'relative_height': .91, 'sex': 'female'})
