# SPDX-License-Identifier: MIT
"""Portable static-scan, equid contact and exported zebra family contracts."""
import json
from pathlib import Path
import unittest

import numpy as np

from test_clips import make_solver
from test_pachyderms import accessor, glb
from zoogen import quadruped

ROOT = Path(__file__).resolve().parents[3]
SPECIES = ('zebra', 'zebra_female', 'zebra_juvenile')
CLIPS = {'idle', 'walk', 'trot', 'eat', 'drink', 'rest', 'sleep', 'vocalize', 'sentry'}


class ZebraTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = []
        for name in SPECIES:
            spec, solver = make_solver(name)
            cls.cases.append((name, spec, solver, {clip.name: clip for clip in spec.clips(solver)}))

    def test_derivative_weights_and_budget(self):
        directory = ROOT / 'tools/animals/sources/zebra'
        self.assertLessEqual(sum(path.stat().st_size for path in directory.iterdir()), 14_000_000)
        with np.load(directory / 'scan.npz', allow_pickle=False) as data:
            self.assertLessEqual(len(data['triangles']), 30000)
            self.assertLessEqual(len(data['bone_names']), 60)
            self.assertGreater(int(data['heat_weighted_vertices']), 0)
            self.assertTrue(np.isfinite(data['vertices']).all())
            self.assertTrue(np.isfinite(data['weights']).all())
            self.assertTrue((data['weights'] >= 0).all())
            self.assertTrue(np.allclose(data['weights'].sum(axis=1), 1, atol=1e-6))
            self.assertTrue(((data['weights'] > 0).sum(axis=1) <= 4).all())
            self.assertEqual(data['uv'].shape, (len(data['triangles']), 3, 2))

    def test_exports_and_variants(self):
        for name, spec, _, clips in self.cases:
            with self.subTest(name=name):
                path = ROOT / 'assets/animals' / (name + '.glb')
                manifest = json.loads(path.with_suffix('.json').read_text())
                self.assertEqual(manifest['license'], 'CC BY 4.0')
                doc, binary = glb(path)
                self.assertEqual(set(clips), CLIPS)
                self.assertEqual({clip['name'] for clip in doc['animations']}, CLIPS)
                self.assertEqual({clip['name'] for clip in manifest['clips']}, CLIPS)
                budget = 20000 if name.endswith('juvenile') else 30000
                triangles = sum(doc['accessors'][primitive['indices']]['count'] // 3
                                for mesh in doc['meshes'] for primitive in mesh['primitives'])
                self.assertEqual(triangles, manifest['triangles'])
                self.assertLessEqual(triangles, budget)
                self.assertLessEqual(len(doc['skins'][0]['joints']), 60)
                self.assertLessEqual(path.stat().st_size, 7_000_000)
                self.assertAlmostEqual(manifest['meta']['shoulder_height_m'], spec.meta['shoulder_height_m'])
                if name != 'zebra':
                    self.assertEqual(manifest['meta']['variant_of'], 'zebra')
                    self.assertEqual(manifest['meta']['variant'], name.split('_')[1])
                if name.endswith('female'):
                    self.assertAlmostEqual(manifest['meta']['relative_height'], .96)
                for mesh in doc['meshes']:
                    for primitive in mesh['primitives']:
                        self.assertIn('TANGENT', primitive['attributes'])
                        weights = accessor(doc, binary, primitive['attributes']['WEIGHTS_0'])
                        self.assertEqual(weights.shape[1], 4)
                        self.assertTrue(np.allclose(weights.sum(axis=1), 1, atol=1e-6))
                        self.assertNotIn('WEIGHTS_1', primitive['attributes'])
                self.assertIn('baseColorTexture', doc['materials'][0]['pbrMetallicRoughness'])

    def test_lying_bones_clear_ground_and_chest_is_low(self):
        for name, spec, solver, clips in self.cases:
            for clip_name in ('rest', 'sleep'):
                for phase in np.linspace(0, 1, 25):
                    world = solver.solve(clips[clip_name].pose(float(phase)))
                    for bone in solver.order:
                        if bone == 'root':
                            continue
                        with self.subTest(name=name, clip=clip_name, phase=phase, bone=bone):
                            self.assertGreaterEqual(world[bone][2, 3], -.02)
                            self.assertGreaterEqual(solver.point(world, bone, solver.tail[bone])[2], -.02)
                    self.assertLess(world['chest'][2, 3], .45 * spec.meta['shoulder_height_m'])

    def test_walk_lateral_sequence_and_trot_diagonals(self):
        for name, _, solver, clips in self.cases:
            walk = clips['walk']
            for key, touchdown in quadruped.WALK.items():
                toe = solver.rig.legs[key].toes
                before = solver.solve(walk.pose((touchdown - .05) % 1))[toe][2, 3]
                after = solver.solve(walk.pose((touchdown + .05) % 1))[toe][2, 3]
                self.assertGreater(before, after + .00001, (name, key))
            airborne = False
            for phase in np.linspace(0, 1, 60, endpoint=False):
                world = solver.solve(clips['trot'].pose(float(phase)))
                heights = {key: world[leg.toes][2, 3] - solver.toe_base(key)[2]
                           for key, leg in solver.rig.legs.items()}
                self.assertAlmostEqual(heights['FL'], heights['HR'], places=6)
                self.assertAlmostEqual(heights['FR'], heights['HL'], places=6)
                airborne |= all(value > .001 for value in heights.values())
            self.assertTrue(airborne, name)

    def test_stance_sliding_matches_existing_bound(self):
        for name, _, solver, clips in self.cases:
            for clip_name in ('walk', 'trot'):
                clip = clips[clip_name]
                count = 96
                for key, leg in solver.rig.legs.items():
                    rest = solver.toe_base(key)
                    contacts = 0
                    for index in range(count):
                        a = solver.solve(clip.pose(index / count))[leg.toes][:3, 3]
                        b = solver.solve(clip.pose(((index + 1) / count) % 1))[leg.toes][:3, 3]
                        if a[2] > rest[2] + 1e-6 or b[2] > rest[2] + 1e-6:
                            continue
                        contacts += 1
                        velocity = (b[0] - a[0]) / (clip.duration / count)
                        self.assertAlmostEqual(velocity, -clip.speed, delta=.02 * clip.speed + 1e-6,
                                               msg=f'{name}/{clip_name}/{key}')
                        self.assertAlmostEqual(a[1], rest[1], places=6)
                    self.assertGreater(contacts, .2 * count)

    def test_food_and_water_reach(self):
        for name, _, solver, clips in self.cases:
            for clip_name, maximum in [('eat', .12), ('drink', .06)]:
                height = quadruped.muzzle(solver, clips[clip_name].pose(0))[2]
                self.assertGreater(height, 0, name)
                self.assertLess(height, maximum, name)

    def test_lying_skin_clears_ground(self):
        for name, spec, solver, clips in self.cases:
            data = spec.sculpt
            for clip_name in ('rest', 'sleep'):
                for phase in (0., .3, .7):
                    world = solver.solve(clips[clip_name].pose(phase))
                    vertices = data['vertices']
                    skinned = np.zeros_like(vertices)
                    for column, bone in enumerate(data['bone_names']):
                        transform = world[bone] @ np.linalg.inv(solver.rest[bone])
                        skinned += data['weights'][:, column, None] * (
                            vertices @ transform[:3, :3].T + transform[:3, 3])
                    self.assertGreaterEqual(float(skinned[:, 2].min()), -.02,
                                            f'{name}/{clip_name}/{phase}')

    def test_exported_body_postures_match_definitions(self):
        from test_pachyderms import world_translation
        for name, _, solver, clips in self.cases:
            doc, binary = glb(ROOT / 'assets/animals' / (name + '.glb'))
            body = next(index for index, node in enumerate(doc['nodes']) if node.get('name') == 'hips')
            for animation in doc['animations']:
                channel = next(item for item in animation['channels']
                               if item['target'] == {'node': body, 'path': 'translation'})
                sampler = animation['samplers'][channel['sampler']]
                translation = accessor(doc, binary, sampler['output'])[0]
                actual = world_translation(doc, body, translation)
                expected = solver.solve(clips[animation['name']].pose(0))['hips'][:3, 3]
                self.assertTrue(np.allclose(actual, expected[[1, 2, 0]], atol=2e-5),
                                f'{name}/{animation["name"]}')

    def test_feeding_skin_clears_ground(self):
        from zoogen.scan import skin_floor
        for name, _, solver, clips in self.cases:
            for clip_name in ('eat', 'drink'):
                for phase in np.linspace(0, 1, 13):
                    self.assertGreaterEqual(skin_floor(solver, clips[clip_name].pose(float(phase))), -.02,
                                            f'{name}/{clip_name}/{phase}')
