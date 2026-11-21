# SPDX-License-Identifier: MIT
"""Species-specific anatomy and production-export checks for the red panda."""
import importlib
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))


class RedPandaTests(unittest.TestCase):
    def setUp(self):
        self.adult = importlib.import_module("species.red_panda").make_spec()
        self.cub = importlib.import_module("species.red_panda_juvenile").make_spec()

    def test_has_long_tail_cheek_mask_and_wide_branch_paws(self):
        for spec in (self.adult, self.cub):
            parts = {part.name: part for part in spec.parts}
            height = spec.meta["shoulder_height_m"]
            length = spec.meta["body_length_m"]
            tail = parts["tail"].rings
            self.assertGreater(tail[0].c[0] - tail[-1].c[0], .85 * length)
            self.assertIn("cheek_fan.L", parts)
            self.assertGreater(parts["front_paw.L"].rings[-1].ry / height, .10)
            self.assertGreater(parts["hind_paw.L"].rings[-1].ry / height, .10)
            self.assertIsNotNone(spec.surface_height)
            self.assertIn("ringed tail", spec.meta["anatomy"])

    def test_cub_has_juvenile_metadata_and_broader_head(self):
        adult = {part.name: part for part in self.adult.parts}
        cub = {part.name: part for part in self.cub.parts}
        adult_head = adult["skull"].rings[1].ry / self.adult.meta["shoulder_height_m"]
        cub_head = cub["skull"].rings[1].ry / self.cub.meta["shoulder_height_m"]
        self.assertGreater(cub_head, 1.08 * adult_head)
        self.assertEqual(self.cub.meta["variant_of"], "red_panda")
        self.assertEqual(self.cub.meta["variant"], "juvenile")
        self.assertAlmostEqual(self.cub.meta["relative_height"], .24 / .32)

    def test_production_exports_match_the_generator_contract(self):
        for name, triangles in (("red_panda", 19200), ("red_panda_juvenile", 17200)):
            with open(os.path.join(ROOT, "assets", "animals", name + ".json")) as source:
                manifest = json.load(source)
            self.assertEqual(manifest["name"], name)
            self.assertEqual(manifest["triangles"], triangles)
            self.assertEqual(manifest["bones"], 33)
            self.assertEqual(manifest["texture"], 1024)
            self.assertEqual(len(manifest["clips"]), 8)
            self.assertIn("thick-coat normal map", manifest["meta"]["surface_detail"])
            self.assertGreater(os.path.getsize(os.path.join(ROOT, "assets", "animals", name + ".glb")),
                               2_000_000)


if __name__ == "__main__":
    unittest.main()
