# SPDX-License-Identifier: MIT
"""Bactrian camel calf with low humps and juvenile head and foot proportions."""
from species.camel import make_spec as camel_spec


def make_spec():
    return camel_spec(juvenile=True)
