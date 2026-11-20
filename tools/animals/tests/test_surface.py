# SPDX-License-Identifier: MIT
"""Tests for procedural tangent-space surface maps."""

import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from zoogen.surface import height_to_normal  # noqa: E402


class SurfaceTests(unittest.TestCase):
    def test_flat_height_faces_straight_out(self):
        normal = height_to_normal(np.ones((7, 9)), 3.0)
        self.assertTrue(np.allclose(normal, [0.5, 0.5, 1.0]))

    def test_ramps_tilt_in_the_opposite_tangent_direction_and_stay_unit_length(self):
        height = np.broadcast_to(np.linspace(0.0, 1.0, 9), (7, 9))
        encoded = height_to_normal(height, 2.5)
        decoded = encoded * 2.0 - 1.0
        self.assertTrue((encoded[..., 0] < 0.5).all())
        self.assertTrue(np.allclose(encoded[..., 1], 0.5))
        self.assertTrue(np.allclose(np.linalg.norm(decoded, axis=-1), 1.0))

    def test_invalid_height_shape_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "two-dimensional"):
            height_to_normal(np.zeros((2, 3, 4)))


if __name__ == "__main__":
    unittest.main()
