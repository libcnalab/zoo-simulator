# SPDX-License-Identifier: MIT
"""Dhole older pup."""
from species.dhole import make_spec as _make


def make_spec():
    return _make(juvenile=True)
