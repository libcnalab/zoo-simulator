# SPDX-License-Identifier: MIT
"""Adult Bactrian camel cow with conservative sex-specific size and build."""
from species.camel import make_spec as camel_spec


def make_spec():
    return camel_spec(female=True)
