# SPDX-License-Identifier: MIT
"""Bear anatomy, gait contact, prepared source and exported material contracts."""
import io
import json
from pathlib import Path
import unittest

import numpy as np
from PIL import Image

from test_clips import make_solver
from test_pachyderms import accessor, glb
from zoogen import quadruped

ROOT = Path(__file__).resolve().parents[3]
SPECIES = ('brown_bear', 'brown_bear_juvenile', 'polar_bear', 'polar_bear_female',
           'polar_bear_juvenile')
STANDARD = {'idle', 'walk', 'trot', 'eat', 'drink', 'rest', 'sleep', 'vocalize'}


class BearTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = {}
        for name in SPECIES:
            spec, solver = make_solver(name)
            cls.cases[name] = spec, solver, {clip.name: clip for clip in spec.clips(solver)}

    def test_derivative_is_portable_and_weights_normalized(self):
        directory = ROOT / 'tools/animals/sources/bear'
        self.assertLessEqual(sum(p.stat().st_size for p in directory.iterdir() if p.is_file()), 14_000_000)
        self.assertTrue((directory / 'README.md').is_file())
        with np.load(directory / 'sculpt.npz', allow_pickle=False) as data:
            self.assertLessEqual(len(data['triangles']), 30000)
            self.assertLessEqual(len(data['bone_names']), 60)
            self.assertTrue(np.isfinite(data['vertices']).all())
            self.assertTrue((data['weights'] >= 0).all())
            self.assertTrue(np.allclose(data['weights'].sum(axis=1), 1, atol=1e-6))
            self.assertTrue(((data['weights'] > 0).sum(axis=1) <= 4).all())
        for path in directory.glob('*.png'):
            with Image.open(path) as image:
                self.assertLessEqual(max(image.size), 2048)

    def test_export_budgets_materials_clips_and_variants(self):
        for name, (spec, _, clips) in self.cases.items():
            with self.subTest(name=name):
                path = ROOT / 'assets/animals' / (name + '.glb')
                manifest = json.loads(path.with_suffix('.json').read_text())
                doc, binary = glb(path)
                expected = STANDARD | {'bathe', 'swim'}
                self.assertEqual(set(clips), expected)
                self.assertEqual({item['name'] for item in manifest['clips']}, expected)
                self.assertEqual({item['name'] for item in doc['animations']}, expected)
                for item in manifest['clips']:
                    self.assertAlmostEqual(item['duration'], clips[item['name']].duration, delta=.0001)
                total = 0
                for mesh in doc['meshes']:
                    for primitive in mesh['primitives']:
                        total += doc['accessors'][primitive['indices']]['count'] // 3
                        attributes = primitive['attributes']
                        self.assertIn('TANGENT', attributes)
                        weights = accessor(doc, binary, attributes['WEIGHTS_0'])
                        self.assertEqual(weights.shape[1], 4)
                        self.assertTrue(np.allclose(weights.sum(axis=1), 1, atol=1e-6))
                        self.assertNotIn('WEIGHTS_1', attributes)
                self.assertEqual(total, manifest['triangles'])
                self.assertLessEqual(total, 20000 if 'juvenile' in name else 30000)
                self.assertLessEqual(len(doc['skins'][0]['joints']), 60)
                self.assertIn('normalTexture', doc['materials'][0])
                self.assertEqual(manifest['license'], 'CC-BY-4.0')
                self.assertEqual(manifest['meta']['shoulder_height_m'], spec.meta['shoulder_height_m'])
                if 'juvenile' in name:
                    self.assertEqual(manifest['meta']['variant'], 'juvenile')
                    self.assertEqual(manifest['meta']['variant_of'], name.removesuffix('_juvenile'))
                if name == 'polar_bear_female':
                    self.assertEqual(manifest['meta']['variant'], 'female')
                    self.assertEqual(manifest['meta']['variant_of'], 'polar_bear')
                    self.assertAlmostEqual(manifest['meta']['relative_height'], .89)
                water = manifest['meta']['swim']
                self.assertEqual(water['clip'], 'swim')
                depth_share = .91 if name.startswith('polar') else .72
                self.assertAlmostEqual(water['depth_m'], depth_share * spec.meta['shoulder_height_m'])
                self.assertAlmostEqual(water['speed_mps'], clips['swim'].speed)
                self.assertEqual(clips['swim'].contact_keys, [])

    def test_rest_and_sleep_bones_clear_ground(self):
        for name, (spec, solver, clips) in self.cases.items():
            for clip_name in ('rest', 'sleep', 'bathe'):
                for phase in np.linspace(0, 1, 16):
                    world = solver.solve(clips[clip_name].pose(float(phase)))
                    for bone in solver.order:
                        if bone == 'root':
                            continue
                        self.assertGreaterEqual(world[bone][2, 3], -.02, (name, clip_name, bone))
                        self.assertGreaterEqual(solver.point(world, bone, solver.tail[bone])[2], -.02,
                                                (name, clip_name, bone))
                    limit = .60 if clip_name == 'sleep' else .55
                    self.assertLess(world['chest'][2, 3], limit * spec.meta['shoulder_height_m'], name)

    def test_walk_is_lateral_near_pace_and_lope_is_transverse(self):
        for name, (_, solver, clips) in self.cases.items():
            for clip_name, expected in [('walk', {'HL': 0, 'FL': .12, 'HR': .5, 'FR': .62}),
                                        ('trot', {'HL': 0, 'HR': .10, 'FL': .48, 'FR': .58})]:
                clip = clips[clip_name]
                self.assertEqual(clip.gait.offsets, expected)
                for key, touchdown in expected.items():
                    bone = solver.rig.legs[key].toes
                    before = solver.solve(clip.pose((touchdown - .04) % 1))[bone][2, 3]
                    after = solver.solve(clip.pose((touchdown + .04) % 1))[bone][2, 3]
                    self.assertGreater(before, after + 1e-6, (name, clip_name, key))

    def test_stance_sliding_and_plantigrade_heels(self):
        for name, (spec, solver, clips) in self.cases.items():
            height = spec.meta['shoulder_height_m']
            for clip_name in ('walk', 'trot'):
                clip = clips[clip_name]
                for key, leg in solver.rig.legs.items():
                    contacts = 0
                    heel_points = None
                    if key.startswith('H'):
                        data = spec.sculpt
                        points = data['vertices']
                        toe = solver.toe_base(key)
                        side = 1 if key == 'HL' else -1
                        heel = (points[:, 0] < toe[0] - .035 * height)
                        heel &= points[:, 0] > toe[0] - .30 * height
                        heel &= points[:, 1] * side > .06 * height
                        heel &= points[:, 2] < .012 * height
                        heel_points = points[heel]
                        heel_weights = data['weights'][heel]
                        self.assertGreater(len(heel_points), 3, (name, key))
                    for i in range(64):
                        world = solver.solve(clip.pose(i / 64))
                        next_world = solver.solve(clip.pose(((i + 1) / 64) % 1))
                        a, b = world[leg.toes][:3, 3], next_world[leg.toes][:3, 3]
                        level = solver.toe_base(key)[2]
                        if a[2] > level + 1e-6 or b[2] > level + 1e-6:
                            continue
                        contacts += 1
                        velocity = (b[0] - a[0]) / (clip.duration / 64)
                        self.assertAlmostEqual(velocity, -clip.speed, delta=.02 * clip.speed + 1e-6,
                                               msg=f'{name}/{clip_name}/{key}')
                        self.assertAlmostEqual(a[1], solver.toe_base(key)[1], places=6)
                        if key.startswith('H'):
                            # Ankle pivot is above the heel pad; the sole stays at
                            # its rest height and the metatarsal never tips up.
                            self.assertAlmostEqual(world[leg.foot][2, 3], solver.head[leg.foot][2], places=6)
                            self.assertLess(world[leg.foot][2, 3], .18 * height)
                            heights = np.zeros(len(heel_points))
                            for column, bone in enumerate(data['bone_names']):
                                if not np.any(heel_weights[:, column]):
                                    continue
                                matrix = world[bone] @ np.linalg.inv(solver.rest[bone])
                                heights += heel_weights[:, column] * (
                                    heel_points @ matrix[2, :3] + matrix[2, 3])
                            self.assertLess(float(np.abs(heights).max()), .025 * height + .003,
                                            (name, clip_name, key))
                    self.assertGreater(contacts, 12)

    def test_polar_anatomy_differs_from_brown(self):
        metrics = {}
        for name in ('brown_bear', 'polar_bear'):
            spec, _, _ = self.cases[name]
            data = spec.sculpt
            heads = dict(zip(data['bone_names'], data['heads']))
            body = np.linalg.norm(heads['chest'] - heads['hips'])
            neck = np.linalg.norm(heads['neck2'] - heads['neck1']) + np.linalg.norm(heads['head'] - heads['neck2'])
            mask = data['weights'][:, data['bone_names'].tolist().index('head')] > .6
            width = np.ptp(data['vertices'][mask, 1])
            metrics[name] = neck / body, width / body
            if name == 'polar_bear':
                vertices = data['vertices']
                dorsal = np.abs(vertices[:, 1]) < .12 * spec.meta['shoulder_height_m']
                shoulder = dorsal & (np.abs(vertices[:, 0] - heads['chest'][0]) < .12)
                rump = dorsal & (np.abs(vertices[:, 0] - heads['hips'][0]) < .18)
                self.assertLessEqual(vertices[shoulder, 2].max(), vertices[rump, 2].max() + .02)
        self.assertGreater(metrics['polar_bear'][0], metrics['brown_bear'][0] * 1.10)
        self.assertLess(metrics['polar_bear'][1], metrics['brown_bear'][1] * .90)

    def test_female_polar_bear_is_smaller_and_less_massively_necked(self):
        male, _, _ = self.cases['polar_bear']
        female, _, _ = self.cases['polar_bear_female']
        self.assertAlmostEqual(female.meta['shoulder_height_m'],
                               .89 * male.meta['shoulder_height_m'])
        self.assertEqual(female.meta['sex'], 'female')

        def width(spec, bones):
            data = spec.sculpt
            names = data['bone_names'].tolist()
            influence = data['weights'][:, [names.index(bone) for bone in bones]].sum(axis=1)
            points = data['vertices'][influence > .5]
            return np.ptp(points[:, 1]) / spec.meta['shoulder_height_m']

        self.assertLess(width(female, ('neck1', 'neck2')),
                        width(male, ('neck1', 'neck2')) * .96)
        self.assertLess(width(female, ('chest',)), width(male, ('chest',)) * .97)

    def test_second_pass_closes_mouth_smooths_shoulders_and_shortens_outline_fur(self):
        for name, (spec, _, _) in self.cases.items():
            data = spec.sculpt
            height = spec.meta['shoulder_height_m']
            juvenile = 'juvenile' in name
            fur = [part for part in spec.parts if part.name.startswith('fur_')]
            self.assertEqual(len(fur), 120 if juvenile else 105, name)
            longest = max(np.linalg.norm(part.rings[-1].c - part.rings[0].c) for part in fur)
            self.assertLess(longest / height, .075 if juvenile else .055, name)
            self.assertEqual(spec.meta['rest_mouth_closure_deg'], 7.0)
            self.assertEqual(spec.meta['anatomy_revision'],
                             'Smoothed reposed shoulders and closed resting mouth')

            vertices, triangles = data['vertices'], data['triangles']
            edges = np.concatenate((triangles[:, [0, 1]], triangles[:, [1, 2]],
                                    triangles[:, [2, 0]]))
            edges = np.unique(np.sort(edges, axis=1), axis=0)
            degree = np.zeros(len(vertices), dtype=float)
            neighbours = np.zeros_like(vertices)
            np.add.at(degree, edges[:, 0], 1)
            np.add.at(degree, edges[:, 1], 1)
            np.add.at(neighbours, edges[:, 0], vertices[edges[:, 1]])
            np.add.at(neighbours, edges[:, 1], vertices[edges[:, 0]])
            names = data['bone_names'].tolist()
            arm = data['weights'][:, [names.index(bone) for bone in
                                      ('scapula.L', 'scapula.R',
                                       'upper_arm.L', 'upper_arm.R')]].sum(axis=1)
            p = vertices / height
            scapula = data['heads'][names.index('scapula.L')] / height
            zone = np.exp(-((p[:, 0] - scapula[0]) / .28) ** 2
                          - ((p[:, 2] - scapula[2]) / .34) ** 2)
            side = np.clip((np.abs(p[:, 1]) - .06) / .18, 0, 1)
            shoulder = (arm > .15) & (zone > .3) & (side > .2)
            residual = np.linalg.norm(vertices - neighbours / np.maximum(degree, 1)[:, None],
                                      axis=1) / height
            self.assertGreater(int(shoulder.sum()), 800, name)
            self.assertLess(float(residual[shoulder].mean()), .003, name)
            self.assertLess(float(residual[shoulder].max()), .030, name)

    def test_side_sleep_is_independent_of_bone_rest_axes(self):
        from zoogen.geom import expand_bones
        from zoogen.rig import Solver, euler
        spec, original, clips = self.cases['brown_bear']
        rest = {}
        for index, (name, matrix) in enumerate(original.rest.items()):
            rest[name] = matrix.copy()
            rest[name][:3, :3] = euler(13 * index, 7 * index, 11 * index)
        rotated = Solver(expand_bones(spec.bones), rest, spec.rig)
        alternate = next(clip for clip in spec.clips(rotated) if clip.name == 'sleep')
        for phase in (0, .25, .6):
            left = original.solve(clips['sleep'].pose(phase))
            right = rotated.solve(alternate.pose(phase))
            for bone in original.order:
                self.assertTrue(np.allclose(left[bone][:3, 3], right[bone][:3, 3], atol=1e-6), bone)
                self.assertTrue(np.allclose(original.point(left, bone, original.tail[bone]),
                                            rotated.point(right, bone, rotated.tail[bone]), atol=1e-6), bone)

    def test_exported_coat_statistics(self):
        for name in SPECIES:
            doc, binary = glb(ROOT / 'assets/animals' / (name + '.glb'))
            material = doc['materials'][0]['pbrMetallicRoughness']
            texture = doc['textures'][material['baseColorTexture']['index']]
            image = doc['images'][texture['source']]
            view = doc['bufferViews'][image['bufferView']]
            begin = view.get('byteOffset', 0)
            with Image.open(io.BytesIO(binary[begin:begin + view['byteLength']])) as decoded:
                rgb = np.asarray(decoded.convert('RGB'), dtype=float) / 255
            covered = rgb.max(axis=-1) > .1
            color = rgb[covered].mean(axis=0)
            luminance = float(color @ [.2126, .7152, .0722])
            if name.startswith('polar'):
                self.assertGreater(luminance, .65, name)
                self.assertLess(luminance, .96, name)
                self.assertGreater(color[0], color[2] + .025, name)
                self.assertLess(float((rgb[covered].min(axis=-1) > .98).mean()), .01)
            else:
                self.assertGreater(color[0], color[1] * 1.10, name)
                self.assertGreater(color[1], color[2] * 1.12, name)
                self.assertLess(luminance, .65, name)


if __name__ == '__main__':
    unittest.main()
