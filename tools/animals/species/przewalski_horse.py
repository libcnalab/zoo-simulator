# SPDX-License-Identifier: MIT
from zoogen.steppe_equid import make_equid


def make_spec():
    return make_equid('przewalski_horse', "Przewalski's horse", 'Equus przewalskii', 1.35,
                      {'hips': {'scale': [1.06, 1.18, 1]},
                       'neck1': {'scale': [1, 1.12, 1]},
                       'ear': {'scale': [.96, .95, .78]}}, chestnut=False)
