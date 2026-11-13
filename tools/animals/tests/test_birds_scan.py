# SPDX-License-Identifier: MIT
"""Behaviour and export contracts for scan-derived birds; no Blender required."""
import json
from pathlib import Path
import sys
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from zoogen import geom
from test_clips import make_solver
from test_pachyderms import glb, accessor

ROOT = Path(__file__).resolve().parents[3]
BIRDS = ('penguin', 'penguin_juvenile', 'flamingo')
STANDARD = {'idle', 'walk', 'trot', 'eat', 'drink', 'rest', 'sleep', 'vocalize'}


class BirdScanTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = {}
        for name in BIRDS:
            spec, solver = make_solver(name)
            cls.cases[name] = spec, solver, {c.name: c for c in spec.clips(solver)}

    def test_portable_sources_weights_and_texture_limits(self):
        for kind in ('penguin', 'flamingo'):
            directory = ROOT / 'tools/animals/sources' / kind
            self.assertLessEqual(sum(p.stat().st_size for p in directory.iterdir() if p.is_file()), 10_000_000)
            with np.load(directory / 'mesh.npz', allow_pickle=False) as data:
                weights = data['weights']
                self.assertTrue(np.isfinite(weights).all())
                self.assertTrue((weights >= 0).all())
                self.assertTrue(((weights > 0).sum(axis=1) <= 4).all())
                self.assertTrue(np.allclose(weights.sum(axis=1), 1, atol=1e-6))
                self.assertEqual(data['uv'].shape, (len(data['triangles']), 3, 2))
                self.assertGreaterEqual(data['triangles'].min(), 0)
                self.assertLess(data['triangles'].max(), len(data['vertices']))
                self.assertTrue(np.isfinite(data['vertices']).all())
            for path in directory.glob('*.png'):
                import struct
                width, height = struct.unpack_from('>II', path.read_bytes(), 16)
                self.assertLessEqual(max(width, height), 2048)
            self.assertIn('CC BY 4.0', (directory / 'README.md').read_text())

    def test_heights_and_rig_landmarks(self):
        for name, (spec, _, _) in self.cases.items():
            height = spec.meta['shoulder_height_m']
            self.assertAlmostEqual(spec.sculpt['vertices'][:, 2].max(), height, delta=height*.03)
            self.assertLessEqual(len(geom.expand_bones(spec.bones)), 60)
            if name.startswith('penguin'):
                for side in ('L', 'R'):
                    self.assertEqual(len(spec.rig.extra['wings'][side]), 3)
            else:
                self.assertGreaterEqual(len(spec.rig.neck), 5)
                self.assertGreater(spec.meta['neck_centerline_m'], .65)

    def test_manifest_clip_water_and_variant_contract(self):
        for name, (spec, _, clips) in self.cases.items():
            manifest = json.loads((ROOT / 'assets/animals' / (name + '.json')).read_text())
            limit = 10000 if name.endswith('juvenile') else 14000
            self.assertLessEqual(manifest['triangles'], limit)
            self.assertEqual(manifest['license'], 'CC-BY-4.0')
            self.assertEqual(manifest['meta'], spec.meta)
            expected = STANDARD | {'swim', 'float'} if name.startswith('penguin') else STANDARD
            self.assertEqual(set(clips), expected)
            exported = {clip['name']: clip for clip in manifest['clips']}
            self.assertEqual(set(exported), expected)
            if name.startswith('penguin'):
                self.assertEqual(manifest['meta']['source_species'], 'Spheniscus magellanicus')
                for water in ('swim', 'float'):
                    metadata = manifest['meta'][water]
                    self.assertEqual(metadata['clip'], water)
                    self.assertGreater(metadata['depth_m'], 0)
                    self.assertLess(metadata['depth_m'], spec.meta['shoulder_height_m'])
                    self.assertAlmostEqual(metadata['speed'], exported[water]['speed'], places=4)
            if name.endswith('juvenile'):
                self.assertEqual(manifest['meta']['variant_of'], 'penguin')
                self.assertEqual(manifest['meta']['variant'], 'juvenile')
                self.assertLess(manifest['meta']['relative_height'], 1)

    def test_swim_is_horizontal_flipper_flight_and_not_a_walk(self):
        for name in ('penguin', 'penguin_juvenile'):
            spec, solver, clips = self.cases[name]
            clip = clips['swim']
            self.assertAlmostEqual(1 / clip.duration, 2)
            self.assertEqual(clip.contact_keys, [])
            flipper = []
            for phase in np.arange(24) / 24:
                world = solver.solve(clip.pose(float(phase)))
                axis = world['neck1'][:3, 3] - world['hips'][:3, 3]
                angle = np.degrees(np.arcsin(abs(axis[2]) / np.linalg.norm(axis)))
                self.assertLessEqual(angle, 25, name)
                flipper.append(solver.point(world, 'flipper3.L', solver.tail['flipper3.L']))
            self.assertGreater(np.ptp(np.array(flipper)[:, 1]), .06 * spec.rig.extra['scale'])
            self.assertGreater(np.ptp(np.array(flipper)[:, 2]), .06 * spec.rig.extra['scale'])

    def test_standing_bones_clear_ground(self):
        for name, (spec, solver, clips) in self.cases.items():
            for clip_name in ('idle', 'walk', 'trot', 'vocalize', 'eat', 'drink'):
                for phase in np.arange(12) / 12:
                    world = solver.solve(clips[clip_name].pose(float(phase)))
                    for bone in geom.expand_bones(spec.bones):
                        if bone.deform:
                            for point in (bone.head, bone.tail):
                                self.assertGreaterEqual(solver.point(world, bone.name, point)[2], -.02,
                                                        f'{name}/{clip_name}/{bone.name}')

    def test_flamingo_rest_has_exactly_one_ground_contact(self):
        _, solver, clips = self.cases['flamingo']
        for name in ('rest', 'sleep'):
            for phase in np.arange(12) / 12:
                world = solver.solve(clips[name].pose(float(phase)))
                contact = [abs(world[leg.toes][2, 3]-solver.toe_base(key)[2]) < .002
                           for key, leg in solver.rig.legs.items()]
                self.assertEqual(sum(contact), 1, name)
                self.assertGreater(world['toes.R'][2, 3], .60)
                self.assertLess(world['head'][2, 3], 1.15)

    def test_walk_stance_feet_do_not_slide(self):
        for name, (_, solver, clips) in self.cases.items():
            for clip_name in ('walk', 'trot'):
                clip = clips[clip_name]
                for side, leg in solver.rig.legs.items():
                    contacts = 0
                    for index in range(48):
                        a = solver.solve(clip.pose(index/48))[leg.toes][:3, 3]
                        b = solver.solve(clip.pose(((index+1)/48) % 1))[leg.toes][:3, 3]
                        if max(a[2], b[2]) > solver.toe_base(side)[2] + 1e-6:
                            continue
                        contacts += 1
                        speed = (b[0]-a[0]) / (clip.duration/48)
                        self.assertAlmostEqual(speed, -clip.speed, delta=.02*clip.speed,
                                               msg=f'{name}/{clip_name}/{side}')
                    self.assertGreater(contacts, 9)

    def test_penguin_paddles_are_skinned_through_all_three_joints(self):
        for name in ('penguin', 'penguin_juvenile'):
            spec, _, _ = self.cases[name]
            data = spec.sculpt
            for side in ('L', 'R'):
                for segment in (1, 2, 3):
                    index = data['bone_names'].tolist().index(f'flipper{segment}.{side}')
                    self.assertGreater(np.count_nonzero(data['weights'][:, index] > .1), 5)

    def test_penguin_outer_paddles_do_not_stretch_when_raised(self):
        for name in ('penguin', 'penguin_juvenile'):
            spec, solver, clips = self.cases[name]
            data = spec.sculpt
            vertices = data['vertices']
            scale = spec.rig.extra['scale']
            outer = ((np.abs(vertices[:, 1]) > .135 * scale)
                     & (vertices[:, 2] > .13 * scale) & (vertices[:, 2] < .29 * scale))
            triangles = data['triangles'][np.all(outer[data['triangles']], axis=1)]
            self.assertGreater(len(triangles), 10)
            edges = np.concatenate([triangles[:, [0, 1]], triangles[:, [1, 2]], triangles[:, [2, 0]]])
            original = np.linalg.norm(vertices[edges[:, 0]] - vertices[edges[:, 1]], axis=1)
            for clip_name in ('vocalize', 'swim'):
                world = solver.solve(clips[clip_name].pose(.25))
                posed = np.zeros_like(vertices)
                for index, bone in enumerate(data['bone_names']):
                    matrix = world[bone]
                    posed += data['weights'][:, index, None] * (
                        (vertices-solver.head[bone]) @ matrix[:3, :3].T + matrix[:3, 3])
                stretched = np.linalg.norm(posed[edges[:, 0]] - posed[edges[:, 1]], axis=1)
                valid = original > 1e-5 * scale
                self.assertLess(float(np.max(stretched[valid] / original[valid])), 1.2,
                                name + '/' + clip_name)

    def test_penguin_belly_rest_clears_and_touches_the_floor(self):
        for name in ('penguin', 'penguin_juvenile'):
            spec, solver, clips = self.cases[name]
            data = spec.sculpt
            for clip_name in ('rest', 'sleep'):
                for phase in (0, .25, .5, .75):
                    world = solver.solve(clips[clip_name].pose(phase))
                    posed = np.zeros_like(data['vertices'])
                    for index, bone in enumerate(data['bone_names']):
                        matrix = world[bone]
                        posed += data['weights'][:, index, None] * (
                            (data['vertices']-solver.head[bone]) @ matrix[:3, :3].T + matrix[:3, 3])
                    self.assertGreaterEqual(posed[:, 2].min(), -.002, name + '/' + clip_name)
                    self.assertLess(posed[:, 2].min(), .01)
                    self.assertLess(world['chest'][2, 3], .3 * spec.meta['shoulder_height_m'])

    def test_exported_geometry_weights_and_animation_names(self):
        for name, (_, _, clips) in self.cases.items():
            document, binary = glb(ROOT / 'assets/animals' / (name + '.glb'))
            manifest = json.loads((ROOT / 'assets/animals' / (name + '.json')).read_text())
            count = 0
            for mesh in document['meshes']:
                for primitive in mesh['primitives']:
                    count += len(accessor(document, binary, primitive['indices'])) // 3
                    weights = accessor(document, binary, primitive['attributes']['WEIGHTS_0'])
                    self.assertTrue(np.allclose(weights.sum(axis=1), 1, atol=1e-5))
                    self.assertTrue((weights >= 0).all())
                    self.assertEqual(weights.shape[1], 4)
            self.assertEqual(count, manifest['triangles'])
            self.assertEqual({a['name'] for a in document['animations']}, set(clips))
            self.assertLess((ROOT / 'assets/animals' / (name + '.glb')).stat().st_size, 7_000_000)


if __name__ == '__main__':
    unittest.main()
