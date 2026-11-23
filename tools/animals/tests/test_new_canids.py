# SPDX-License-Identifier: MIT
"""Dedicated anatomy, juvenile and production-export contracts for the new canids."""
import importlib
import json
from pathlib import Path
import unittest

from test_pachyderms import glb


ROOT = Path(__file__).resolve().parents[3]
SPECIES = ("dingo", "african_wild_dog", "dhole")
CLIPS = {"idle", "walk", "trot", "eat", "drink", "rest", "sleep", "vocalize"}


class NewCanidTests(unittest.TestCase):
    def test_each_species_has_distinct_anatomy_and_a_real_juvenile(self):
        anatomy = set()
        for name in SPECIES:
            adult = importlib.import_module("species." + name).make_spec()
            juvenile = importlib.import_module("species." + name + "_juvenile").make_spec()
            with self.subTest(name=name):
                self.assertEqual(adult.name, name)
                self.assertEqual(juvenile.meta["variant_of"], name)
                self.assertEqual(juvenile.meta["variant"], "juvenile")
                self.assertLess(juvenile.meta["relative_height"], 0.7)
                self.assertLess(juvenile.meta["shoulder_height_m"], adult.meta["shoulder_height_m"])
                self.assertNotEqual(adult.meta["anatomy"], "")
                anatomy.add(adult.meta["anatomy"])
        self.assertEqual(len(anatomy), len(SPECIES))

    def test_exports_match_manifests_and_animation_budget(self):
        embedded_images = set()
        for species in SPECIES:
            for name in (species, species + "_juvenile"):
                with self.subTest(name=name):
                    path = ROOT / "assets/animals" / (name + ".glb")
                    manifest = json.loads(path.with_suffix(".json").read_text())
                    document, binary = glb(path)
                    self.assertEqual(manifest["name"], name)
                    self.assertEqual(manifest["bones"], 33)
                    self.assertEqual(manifest["texture"], 1024)
                    self.assertEqual({clip["name"] for clip in manifest["clips"]}, CLIPS)
                    self.assertEqual({clip["name"] for clip in document["animations"]}, CLIPS)
                    self.assertLessEqual(manifest["triangles"], 18050)
                    self.assertLess(path.stat().st_size, 3_500_000)
                    self.assertIn("baseColorTexture", document["materials"][0]["pbrMetallicRoughness"])
                    image = document["images"][0]
                    view = document["bufferViews"][image["bufferView"]]
                    start = view.get("byteOffset", 0)
                    embedded_images.add(binary[start:start + view["byteLength"]])
        # Adult and juvenile textures are generated separately, and all three coat systems are
        # materially distinct rather than pointing back to the grey wolf texture.
        self.assertEqual(len(embedded_images), 6)


if __name__ == "__main__":
    unittest.main()
