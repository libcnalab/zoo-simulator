# SPDX-License-Identifier: MIT
"""Young Aldabra giant tortoise."""
from species.aldabra_tortoise import make_spec as tortoise_spec


def make_spec():
    return tortoise_spec(juvenile=True)
