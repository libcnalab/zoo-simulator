# SPDX-License-Identifier: MIT
"""Dhole adult."""
from species.canid_variant import make_canid


def make_spec(juvenile=False):
    return make_canid("dhole", juvenile)
