# SPDX-License-Identifier: MIT
"""Check that small facial features survive export and are visible outside the coat."""
import importlib
import unittest
from pathlib import Path

import numpy as np

from zoogen.geom import loft
from zoogen.pachyderm_metrics import accessor, glb


def exported_geometry(path):
    doc, binary = glb(path)
    points, triangles = [], []
    offset = 0
    for mesh in doc["meshes"]:
        for primitive in mesh["primitives"]:
            positions = accessor(doc, binary, primitive["attributes"]["POSITION"])
            points.append(positions[:, [2, 0, 1]])  # animal-space forward, left, up
            triangles.append(accessor(doc, binary, primitive["indices"]).reshape(-1, 3).astype(int) + offset)
            offset += len(positions)
    return np.concatenate(points), np.concatenate(triangles)


def coat_surface(triangles, x, z):
    """Intersect the outer coat with a lateral ray at the eye in animal space."""
    a, b, c = triangles.transpose(1, 0, 2)
    u, v = b - a, c - a
    det = u[:, 0] * v[:, 2] - v[:, 0] * u[:, 2]
    valid = abs(det) > 1e-10
    a, u, v, det = a[valid], u[valid], v[valid], det[valid]
    dx, dz = x - a[:, 0], z - a[:, 2]
    f = (dx * v[:, 2] - dz * v[:, 0]) / det
    g = (dz * u[:, 0] - dx * u[:, 2]) / det
    inside = (f >= 0) & (g >= 0) & (f + g <= 1)
    return (a[:, 1] + f * u[:, 1] + g * v[:, 1])[inside].max()


class FacialLandmarkTests(unittest.TestCase):
    def test_shipped_eyes_and_nostrils_retain_their_authored_surfaces(self):
        assets = Path(__file__).resolve().parents[3] / "assets/animals"
        for species in ("capybara", "lynx", "red_panda", "snow_leopard", "wolf"):
            variants = [species, species + "_juvenile"]
            if species == "wolf":
                variants.append("wolf_female")
            for name in variants:
                spec = importlib.import_module("species." + name).make_spec()
                vertices, triangles = exported_geometry(assets / (name + ".glb"))
                for part in (p for p in spec.parts if p.name in ("eye.L", "nostril.L")):
                    expected = loft(part)[0]
                    for side in (1, -1):
                        with self.subTest(name=name, part=part.name, side=side):
                            points = vertices * [1, side, 1]
                            within = ((points >= expected.min(axis=0) - 2e-5) &
                                      (points <= expected.max(axis=0) + 2e-5)).all(axis=1)
                            self.assertTrue(within.any())
                            distance = np.linalg.norm(expected[:, None] - points[None, within], axis=2)
                            self.assertLess(distance.min(axis=1).max(), 2e-5)
                            if part.name == "eye.L" and species != "capybara":
                                # Check both the exposed eye and the skin surrounding it. A
                                # perfectly formed eye inside the cheek is still invisible.
                                eye_vertices = np.zeros(len(points), dtype=bool)
                                eye_vertices[within] = distance.min(axis=0) < 2e-5
                                skin = points[triangles[~eye_vertices[triangles].any(axis=1)]]
                                ring = part.rings[0]
                                x, y, z = ring.c
                                buried = (coat_surface(skin, x, z) - y) / ring.ry
                                self.assertGreater(buried, .20)
                                self.assertLess(buried, .80)
