# SPDX-License-Identifier: MIT
"""Species-specific anatomy and production-export checks for the red panda."""
import importlib
import io
import json
import os
import sys
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

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
            self.assertTrue(parts["cheek_fan.L"].fuse)
            snout = parts["nose"].rings[-1].c[0] - parts["eye.L"].rings[0].c[0]
            self.assertLess(snout, .21 * height)
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
        for name, triangles in (("red_panda", 18528), ("red_panda_juvenile", 16528)):
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

    def test_pale_brows_and_red_brown_tear_marks_in_the_shipped_coats(self):
        from zoogen.pachyderm_metrics import accessor, glb

        for spec in (self.adult, self.cub):
            doc, binary = glb(Path(ROOT) / "assets/animals" / (spec.name + ".glb"))
            primitive = doc["meshes"][0]["primitives"][0]
            material = doc["materials"][primitive["material"]]["pbrMetallicRoughness"]
            texture = doc["textures"][material["baseColorTexture"]["index"]]
            view = doc["bufferViews"][doc["images"][texture["source"]]["bufferView"]]
            start = view.get("byteOffset", 0)
            pixels = np.asarray(Image.open(io.BytesIO(binary[start:start + view["byteLength"]])).convert("RGB"))
            attributes = primitive["attributes"]
            positions = accessor(doc, binary, attributes["POSITION"])[:, [2, 0, 1]]
            uv = accessor(doc, binary, attributes["TEXCOORD_0"])
            size = np.array([pixels.shape[1], pixels.shape[0]])
            ix, iy = np.clip((uv * size).astype(int), 0, size - 1).T
            eye = next(p.rings[0].c for p in spec.parts if p.name == "eye.L")
            height = spec.meta["shoulder_height_m"]
            forward, _, above = ((positions - eye) / height).T
            for side in (-1, 1):
                with self.subTest(model=spec.name, side=side):
                    cheek = positions[:, 1] * side > .04 * height
                    brow = cheek & (abs(forward + .02) < .05) & (abs(above - .075) < .035)
                    tear = cheek & (abs(forward - .10) < .025) & (abs(above + .08) < .020)
                    self.assertGreaterEqual(brow.sum(), 4)
                    self.assertGreaterEqual(tear.sum(), 4)
                    brow_rgb = np.median(pixels[iy[brow], ix[brow]], axis=0)
                    tear_rgb = np.median(pixels[iy[tear], ix[tear]], axis=0)
                    self.assertGreater(brow_rgb[1], 160)
                    self.assertGreater(brow_rgb[1] - tear_rgb[1], 85)
                    self.assertGreater(tear_rgb[0], 1.8 * tear_rgb[1])


if __name__ == "__main__":
    unittest.main()
