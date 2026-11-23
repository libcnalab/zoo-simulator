# SPDX-License-Identifier: MIT
"""African wild dog older pup."""
from species.african_wild_dog import make_spec as _make


def make_spec():
    return _make(juvenile=True)
