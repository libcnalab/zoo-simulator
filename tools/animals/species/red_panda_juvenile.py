# SPDX-License-Identifier: MIT
"""Red panda cub variant."""
from species.red_panda import make_spec as red_panda_spec


def make_spec():
    return red_panda_spec(juvenile=True)
