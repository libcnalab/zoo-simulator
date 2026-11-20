# SPDX-License-Identifier: MIT
"""Common ostrich chick with juvenile plumage and body proportions."""
from species.ostrich import make_spec as ostrich_spec


def make_spec():
    return ostrich_spec(juvenile=True)
