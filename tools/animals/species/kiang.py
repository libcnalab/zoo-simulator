# SPDX-License-Identifier: MIT
from zoogen.steppe_equid import make_equid


def make_spec():
    return make_equid('kiang', 'Kiang', 'Equus kiang', 1.40,
                      {'ear': {'scale': [1, 1.12, 1.50]},
                       'head': {'scale': [1.06, .95, 1]}}, chestnut=True)
