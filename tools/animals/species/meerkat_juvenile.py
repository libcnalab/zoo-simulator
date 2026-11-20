# SPDX-License-Identifier: MIT
"""Meerkat pup with juvenile head, face, eye, ear and paw proportions."""
from species.meerkat import make_spec as meerkat_spec


def make_spec():
    return meerkat_spec(juvenile=True)
