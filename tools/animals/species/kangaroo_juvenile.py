# SPDX-License-Identifier: MIT
"""Young-at-foot red kangaroo with joey proportions and scaled locomotion."""
from species.kangaroo import make_spec as kangaroo_spec


def make_spec():
    return kangaroo_spec(juvenile=True)
