# SPDX-License-Identifier: MIT
"""Greater capybara pup."""
from species.capybara import make_spec as capybara_spec


def make_spec():
    return capybara_spec(juvenile=True)
