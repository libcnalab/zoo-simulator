# SPDX-License-Identifier: MIT
"""Eurasian lynx kitten variant."""
from species.lynx import make_spec as lynx_spec


def make_spec():
    return lynx_spec(juvenile=True)
