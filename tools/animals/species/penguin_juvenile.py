# SPDX-License-Identifier: MIT
"""Juvenile penguin, 0.533 m standing height; retains the Magellanic scan's plumage."""
from species.penguin import make_spec as penguin_spec


def make_spec():
    return penguin_spec(juvenile=True)
