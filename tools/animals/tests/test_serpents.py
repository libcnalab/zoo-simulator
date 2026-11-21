# SPDX-License-Identifier: MIT
"""Check lateral undulation and coiling in both pavilion residents."""
import unittest

import numpy as np

from test_clips import make_solver


class SerpentMotionTests(unittest.TestCase):
    def test_spine_keeps_its_length_and_makes_a_travelling_wave(self):
        for name in ("corn_snake", "black_headed_python"):
            with self.subTest(name=name):
                spec, solver = make_solver(name)
                spine = [bone.name for bone in spec.bones if bone.parent is not None
                         and bone.name not in ("jaw", "tongue")]
                clips = {clip.name: clip for clip in spec.clips(solver)}
                rest = np.array([solver.head[joint] for joint in spine])
                lengths = np.linalg.norm(np.diff(rest, axis=0), axis=1)
                waves = []
                for phase in (0.0, .25, .5, .75):
                    world = solver.solve(clips["walk"].pose(phase))
                    points = np.array([world[joint][:3, 3] for joint in spine])
                    np.testing.assert_allclose(np.linalg.norm(np.diff(points, axis=0), axis=1), lengths)
                    np.testing.assert_allclose(world["hips"][:2, 3], [0, 0], atol=1e-6)
                    self.assertGreater(points[:, 2].min(), 0)
                    directions = np.diff(points[:, 1])
                    self.assertGreaterEqual(np.count_nonzero(directions[1:] * directions[:-1] < 0), 2)
                    waves.append(points[:, 1])
                np.testing.assert_allclose(waves[0], -waves[2], atol=1e-6)
                self.assertGreater(np.max(np.abs(waves[0] - waves[1])), .07)
                self.assertFalse(clips["walk"].events)

    def test_sleep_coil_is_compact_and_does_not_cross_itself(self):
        for name in ("corn_snake", "black_headed_python"):
            with self.subTest(name=name):
                spec, solver = make_solver(name)
                clips = {clip.name: clip for clip in spec.clips(solver)}
                world = solver.solve(clips["sleep"].pose(0))
                spine = [world[bone.name][:2, 3] for bone in spec.bones[1:-2]]
                gap = np.linalg.norm(spine[-1] - spine[0])
                self.assertGreater(gap, .08)
                self.assertLess(gap, spec.meta["body_length_m"] * .28)
                self.assertLess(np.ptp(spine, axis=0).max(), spec.meta["body_length_m"] * .47)
                for i, a in enumerate(spine):
                    for b in spine[i + 5:]:
                        self.assertGreater(np.linalg.norm(a - b), .09)

    def test_fork_emerges_along_the_head_when_the_tongue_flicks(self):
        for name in ("corn_snake", "black_headed_python"):
            with self.subTest(name=name):
                spec, solver = make_solver(name)
                clip = next(c for c in spec.clips(solver) if c.name == "walk")
                pose = clip.pose(.25)
                offset = pose.offset.pop("tongue")
                world = solver.solve(pose)
                axis = world["head"][:3, 0]
                self.assertGreater(np.dot(axis, offset), spec.meta["body_length_m"] * .020)
