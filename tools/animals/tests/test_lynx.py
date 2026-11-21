# SPDX-License-Identifier: MIT
"""Species-specific anatomy and production-export checks for the Eurasian lynx."""
import importlib
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))


class LynxTests(unittest.TestCase):
    def setUp(self):
        self.adult = importlib.import_module("species.lynx").make_spec()
        self.kitten = importlib.import_module("species.lynx_juvenile").make_spec()

    def test_has_bobtail_ear_pencils_ruff_and_wide_snow_paws(self):
        for spec in (self.adult, self.kitten):
            parts = {part.name: part for part in spec.parts}
            height = spec.meta["shoulder_height_m"]
            length = spec.meta["body_length_m"]
            tail = parts["tail"].rings
            self.assertLess(tail[0].c[0] - tail[-1].c[0], .24 * length)
            self.assertIn("ear_pencil.L", parts)
            self.assertIn("cheek_ruff.L", parts)
            self.assertGreater(parts["front_paw.L"].rings[-1].ry / height, .10)
            self.assertGreater(parts["hind_paw.L"].rings[-1].ry / height, .10)
            self.assertIsNotNone(spec.surface_height)
            self.assertIn("bobtail", spec.meta["anatomy"])

    def test_kitten_has_juvenile_metadata_and_broader_head(self):
        adult = {part.name: part for part in self.adult.parts}
        kitten = {part.name: part for part in self.kitten.parts}
        adult_head = adult["skull"].rings[1].ry / self.adult.meta["shoulder_height_m"]
        kitten_head = kitten["skull"].rings[1].ry / self.kitten.meta["shoulder_height_m"]
        self.assertGreater(kitten_head, 1.08 * adult_head)
        self.assertEqual(self.kitten.meta["variant_of"], "lynx")
        self.assertEqual(self.kitten.meta["variant"], "juvenile")
        self.assertAlmostEqual(self.kitten.meta["relative_height"], .40 / .65)

    def test_production_exports_match_the_generator_contract(self):
        for name, triangles in (("lynx", 19360), ("lynx_juvenile", 17360)):
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
