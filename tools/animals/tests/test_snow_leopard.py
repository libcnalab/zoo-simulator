# SPDX-License-Identifier: MIT
"""Species-specific anatomy and production-export checks for the snow leopard."""
import importlib
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))


class SnowLeopardTests(unittest.TestCase):
    def setUp(self):
        self.adult = importlib.import_module("species.snow_leopard").make_spec()
        self.cub = importlib.import_module("species.snow_leopard_juvenile").make_spec()

    def test_has_body_length_tail_small_ears_and_wide_snow_paws(self):
        for spec in (self.adult, self.cub):
            parts = {part.name: part for part in spec.parts}
            height = spec.meta["shoulder_height_m"]
            length = spec.meta["body_length_m"]
            tail = parts["tail"].rings
            self.assertGreater(tail[0].c[0] - tail[-1].c[0], .88 * length)
            self.assertLess(parts["ear.L"].rings[-1].c[2] - parts["ear.L"].rings[0].c[2],
                            .18 * height)
            self.assertGreater(parts["front_paw.L"].rings[-1].ry / height, .10)
            self.assertGreater(parts["hind_paw.L"].rings[-1].ry / height, .10)
            self.assertIsNotNone(spec.surface_height)
            self.assertIn("balancing tail", spec.meta["anatomy"])

    def test_cub_has_juvenile_metadata_and_broader_head(self):
        adult = {part.name: part for part in self.adult.parts}
        cub = {part.name: part for part in self.cub.parts}
        adult_head = adult["skull"].rings[1].ry / self.adult.meta["shoulder_height_m"]
        cub_head = cub["skull"].rings[1].ry / self.cub.meta["shoulder_height_m"]
        self.assertGreater(cub_head, 1.08 * adult_head)
        self.assertEqual(self.cub.meta["variant_of"], "snow_leopard")
        self.assertEqual(self.cub.meta["variant"], "juvenile")
        self.assertAlmostEqual(self.cub.meta["relative_height"], .39 / .60)

    def test_production_exports_match_the_generator_contract(self):
        for name, triangles in (("snow_leopard", 20028), ("snow_leopard_juvenile", 18028)):
            with open(os.path.join(ROOT, "assets", "animals", name + ".json")) as source:
                manifest = json.load(source)
            self.assertEqual(manifest["name"], name)
            self.assertEqual(manifest["triangles"], triangles)
            self.assertEqual(manifest["bones"], 33)
            self.assertEqual(manifest["texture"], 1024)
            self.assertEqual(len(manifest["clips"]), 8)
            self.assertIn("winter-coat normal map", manifest["meta"]["surface_detail"])
            self.assertGreater(os.path.getsize(os.path.join(ROOT, "assets", "animals", name + ".glb")),
                               2_000_000)


if __name__ == "__main__":
    unittest.main()
