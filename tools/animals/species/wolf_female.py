# SPDX-License-Identifier: MIT
"""Adult grey-wolf female with modest sex-specific size and build."""
from species.wolf import make_spec as wolf_spec


def make_spec():
    return wolf_spec(female=True)
