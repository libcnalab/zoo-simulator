# SPDX-License-Identifier: MIT
"""Shorter brown-plumaged adult common ostrich hen."""
from species.ostrich import make_spec as ostrich_spec


def make_spec():
    return ostrich_spec(female=True)
