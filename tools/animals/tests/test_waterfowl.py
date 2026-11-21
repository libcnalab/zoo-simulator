# SPDX-License-Identifier: MIT
"""Pond and feeding poses of the two Mandarin duck models."""
import unittest

import numpy as np

from test_clips import make_solver
from zoogen.quadruped import muzzle


class MandarinDuckTests(unittest.TestCase):
    def test_only_the_drake_has_sail_feathers(self):
        for kind in ("mandarin_duck", "mandarin_duck_female"):
            spec, _ = make_solver(kind)
            sails = [part for part in spec.parts if part.tag == "sail"]
            if kind.endswith("_female"):
                self.assertFalse(sails)
            else:
                self.assertEqual(len(sails), 1)
                self.assertTrue(sails[0].mirror)

    def test_drinking_reaches_the_water_and_feeding_keeps_the_bill_above_ground(self):
        for kind in ("mandarin_duck", "mandarin_duck_female"):
            spec, rig = make_solver(kind)
            clips = {clip.name: clip for clip in spec.clips(rig)}
            for name in ("eat", "drink"):
                heights = [muzzle(rig, clips[name].pose(i / 32))[2] for i in range(32)]
                self.assertGreater(min(heights), 0)
                self.assertLess(max(heights), .075)
                self.assertGreater(max(heights) - min(heights), .012)

    def test_alternating_underwater_kicks_have_no_footsteps(self):
        spec, rig = make_solver("mandarin_duck")
        clips = {clip.name: clip for clip in spec.clips(rig)}
        self.assertEqual(len(clips["walk"].events), 2)
        self.assertFalse(clips["swim"].events)
        for phase in (.25, .75):
            one = rig.solve(clips["swim"].pose(phase))
            next_half = rig.solve(clips["swim"].pose((phase + .5) % 1))
            self.assertGreater(abs(one["toes.L"][0, 3] - one["toes.R"][0, 3]), .02)
            np.testing.assert_allclose(one["toes.L"][0, 3], next_half["toes.R"][0, 3], atol=1e-5)
