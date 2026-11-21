# SPDX-License-Identifier: MIT
"""Shell rigidity, ground reach and four-beat support for each Aldabra model."""
import unittest

import numpy as np

from test_clips import make_solver
from zoogen import geom


class TortoiseTests(unittest.TestCase):
    def test_shell_remains_rigid_while_the_head_grazes_and_feet_walk(self):
        for name in ("aldabra_tortoise", "aldabra_tortoise_female", "aldabra_tortoise_juvenile"):
            spec, solver = make_solver(name)
            parts = {part.name: part for part in spec.parts}
            for shell in ("carapace", "plastron"):
                self.assertEqual(parts[shell].weights, {"hips": 1.0})
                self.assertFalse(parts[shell].fuse)
            carapace = geom.loft(parts["carapace"])[0]
            height = spec.meta["shoulder_height_m"]
            self.assertAlmostEqual(carapace[:, 2].max(), height, delta=height * .04)
            clips = {clip.name: clip for clip in spec.clips(solver)}
            for motion in ("walk", "trot"):
                for phase in np.linspace(0, 1, 41):
                    world = solver.solve(clips[motion].pose(phase))
                    contacts = sum(world[leg.toes][2, 3] <= solver.toe_base(key)[2] + 1e-5
                                   for key, leg in solver.rig.legs.items())
                    self.assertGreaterEqual(contacts, 3, (name, motion, phase))
            beak = parts["beak"].rings[-1].c
            for phase in np.linspace(0, 1, 17):
                world = solver.solve(clips["eat"].pose(phase))
                z = solver.point(world, "head", beak)[2]
                self.assertGreater(z, 0, name)
                self.assertLess(z, .12 * height, name)
