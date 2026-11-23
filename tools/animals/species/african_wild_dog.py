# SPDX-License-Identifier: MIT
"""African wild dog adult."""
from species.canid_variant import make_canid


def make_spec(juvenile=False):
    return make_canid("african_wild_dog", juvenile)
