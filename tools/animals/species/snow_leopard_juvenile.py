# SPDX-License-Identifier: MIT
"""Snow leopard cub variant."""
from species.snow_leopard import make_spec as snow_leopard_spec


def make_spec():
    return snow_leopard_spec(juvenile=True)
