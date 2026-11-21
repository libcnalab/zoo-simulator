# SPDX-License-Identifier: MIT
"""The capybara's four/three toes, surfaced senses and distinct water movements."""
import unittest

import numpy as np

from test_clips import make_solver


class CapybaraTests(unittest.TestCase):
    def test_webbed_feet_and_head_above_water_at_both_ages(self):
        for name in ("capybara", "capybara_juvenile"):
            spec, solver = make_solver(name)
            parts = {part.name: part for part in spec.parts}
            self.assertNotIn("tail", parts)
            for end, count in [("front", 4), ("hind", 3)]:
                toes = [part for key, part in parts.items() if key.startswith(end + "_digit_")]
                self.assertEqual(len(toes), count)
                for toe in toes:
                    self.assertTrue(toe.mirror)
            clips = {clip.name: clip for clip in spec.clips(solver)}
            height = spec.meta["shoulder_height_m"]
            for motion in ("swim", "float"):
                clip = clips[motion]
                self.assertEqual(clip.contact_keys, [])
                depth = spec.meta[motion]["depth_m"]
                poses = [clip.pose(phase) for phase in np.linspace(0, 1, 17)]
                for pose in poses:
                    world = solver.solve(pose)
                    for part in ("eye.L", "ear.L", "nostril.L"):
                        ring = parts[part].rings[-1]
                        bone = next(iter(parts[part].weights))
                        pos = solver.point(world, bone, ring.c)
                        self.assertGreater(pos[2], depth, (name, motion, part))
                for foot in solver.rig.legs:
                    travel = np.array([p.feet[foot].pos for p in poses])
                    np.testing.assert_allclose(travel[0], travel[-1], atol=1e-8)
                    extent = np.ptp(travel[:, 0]) / height
                    if motion == "swim":
                        self.assertGreater(extent, .28)
                    else:
                        self.assertLess(extent, .10)
                self.assertEqual(clip.speed > 0, motion == "swim")
