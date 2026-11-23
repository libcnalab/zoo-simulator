# SPDX-License-Identifier: MIT
"""Dingo adult."""
from species.canid_variant import make_canid


def make_spec(juvenile=False):
    return make_canid("dingo", juvenile)
