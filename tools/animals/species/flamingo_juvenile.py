# SPDX-License-Identifier: MIT
"""First-year greater flamingo with juvenile plumage and scale."""
from species.flamingo import make_spec as flamingo_spec


def make_spec():
    return flamingo_spec(juvenile=True)
