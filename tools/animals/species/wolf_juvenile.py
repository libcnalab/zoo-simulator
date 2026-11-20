# SPDX-License-Identifier: MIT
"""Older grey wolf pup with juvenile head, muzzle and paw proportions."""
from species.wolf import make_spec as wolf_spec


def make_spec():
    return wolf_spec(juvenile=True)
