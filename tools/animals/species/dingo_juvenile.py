# SPDX-License-Identifier: MIT
"""Dingo older pup."""
from species.dingo import make_spec as _make


def make_spec():
    return _make(juvenile=True)
