# SPDX-License-Identifier: MIT
"""Portable skin, gait, posture and exported glTF contracts for all five pachyderms."""
import json
from pathlib import Path
import unittest

import numpy as np

import test_clips as common
from zoogen.pachyderm_motion import skin_points
from zoogen.pachyderm_metrics import accessor, glb, measure

ROOT = Path(__file__).resolve().parents[3]
SPECIES = ['rhino', 'rhino_juvenile', 'elephant', 'elephant_female', 'elephant_juvenile']
CLIPS = {'idle', 'walk', 'trot', 'eat', 'drink', 'rest', 'sleep', 'vocalize', 'swim'}


def clip_set(species):
    """The elephants also dust-bathe (an activity clip, tests/test_activity_clips.py)."""
    return CLIPS | ({'dust_bathe'} if species.startswith('elephant') else {'bathe'})


def world_translation(document, node, translation):
    """Joint channels are parent-local; include the exported root's rest rotation."""
    point = np.array(translation, dtype=float)
    parents = {child: i for i, item in enumerate(document['nodes']) for child in item.get('children', [])}
    while node in parents:
        node = parents[node]
        item = document['nodes'][node]
        point *= item.get('scale', [1, 1, 1])
        quaternion = np.array(item.get('rotation', [0, 0, 0, 1]))
        axis, scalar = quaternion[:3], quaternion[3]
        point += 2 * np.cross(axis, np.cross(axis, point) + scalar * point)
        point += item.get('translation', [0, 0, 0])
    return point


class PachydermTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = []
        cls.specs = {}
        for species in SPECIES:
            spec, solver = common.make_solver(species)
            cls.specs[species] = spec
            cls.cases.append((species, solver, spec.clips(solver)))

    def test_portable_prepared_sources(self):
        for kind in ('rhino', 'elephant'):
            directory = ROOT / 'tools/animals/sources' / kind
            self.assertLessEqual(sum(p.stat().st_size for p in directory.iterdir() if p.is_file()), 14_000_000)
            with np.load(directory / 'rigged.npz', allow_pickle=False) as data:
                self.assertTrue(np.allclose(data['weights'].sum(axis=1), 1, atol=1e-6))
                self.assertTrue(((data['weights'] > 0).sum(axis=1) <= 4).all())
                self.assertGreaterEqual(data['weights'].min(), 0)
                self.assertTrue(np.array_equal(data['walk_rotations'][0], data['walk_rotations'][-1]))
                self.assertLessEqual(len(data['bone_names']), 64)
                self.assertEqual(data['uv'].shape, (len(data['triangles']), 3, 2))

    def test_complete_clip_sets_and_speeds(self):
        for species, _, clips in self.cases:
            self.assertEqual({clip.name for clip in clips}, clip_set(species), species)
            for clip in clips:
                self.assertGreater(clip.duration, 0)
                self.assertEqual(clip.speed > 0, clip.name in ('walk', 'trot', 'swim'))
            by = {clip.name: clip for clip in clips}
            self.assertAlmostEqual(by['swim'].speed, .6 * by['walk'].speed)
            if species.endswith('juvenile'):
                self.assertAlmostEqual(by['walk'].speed, .8 if species.startswith('rhino') else .9)
                self.assertAlmostEqual(by['trot'].speed, 2.2)

    def test_feet_do_not_slide(self):
        # Use precisely the existing stance-foot velocity check, including its tolerance.
        common.ClipTests.test_gaits_do_not_slide(self)

    def test_legs_reach_without_locking(self):
        common.ClipTests.test_feet_reach_their_targets(self)

    def test_elephant_footfall_order_and_continuous_support(self):
        for species, solver, clips in self.cases:
            if not species.startswith('elephant'):
                continue
            for clip in clips:
                if clip.name not in ('walk', 'trot'):
                    continue
                previous, contacts = None, []
                for i in range(-1, 121):
                    world = solver.solve(clip.pose((i / 120) % 1))
                    current = {key: abs(world[leg.toes][2, 3] - solver.toe_base(key)[2]) < 1e-7
                               for key, leg in solver.rig.legs.items()}
                    self.assertTrue(any(current.values()), f'{species}/{clip.name}/{i}')
                    if previous is not None and i < 120:
                        contacts.extend(key for key in current if current[key] and not previous[key])
                    previous = current
                self.assertEqual(contacts, ['HL', 'FL', 'HR', 'FR'], f'{species}/{clip.name}')

    def test_rhino_trot_is_diagonal(self):
        for species, solver, clips in self.cases:
            if not species.startswith('rhino'):
                continue
            trot = next(c for c in clips if c.name == 'trot')
            for i in range(48):
                world = solver.solve(trot.pose(i / 48))
                contact = {key: abs(world[leg.toes][2, 3] - solver.toe_base(key)[2]) < 1e-7
                           for key, leg in solver.rig.legs.items()}
                self.assertEqual(contact['FL'], contact['HR'])
                self.assertEqual(contact['FR'], contact['HL'])

    def test_ground_and_rest_postures(self):
        for species, solver, clips in self.cases:
            spec = self.specs[species]
            for clip in clips:
                for phase in (0, .25, .5, .75):
                    pose = clip.pose(phase)
                    points = skin_points(solver, pose, spec.sculpt)
                    self.assertGreaterEqual(points[:, 2].min(), -.02, f'{species}/{clip.name}/{phase}')
                    if clip.name not in ('rest', 'sleep', 'bathe'):
                        continue
                    world = solver.solve(pose)
                    standing = species.startswith('elephant') and clip.name in ('rest', 'sleep')
                    if standing:
                        self.assertGreater(world['chest'][2, 3], .65 * spec.meta['shoulder_height_m'])
                    else:
                        self.assertLess(world['chest'][2, 3], .60 * spec.meta['shoulder_height_m'])
                        names = spec.sculpt['bone_names'].tolist()
                        core = [names.index(n) for n in ('hips', 'spine1', 'spine2', 'chest')]
                        body = spec.sculpt['weights'][:, core].sum(axis=1) > .5
                        self.assertLess(points[body, 2].min(), .04, f'{species}/{clip.name}: floating body')

    def test_elephants_doze_with_a_lower_head_and_one_relaxed_hind_foot(self):
        for species, solver, clips in self.cases:
            if not species.startswith('elephant'):
                continue
            by = {clip.name: clip for clip in clips}
            for phase in np.linspace(0, 1, 13):
                idle = solver.solve(by['idle'].pose(float(phase)))
                sleep = solver.solve(by['sleep'].pose(float(phase)))
                height = self.specs[species].meta['shoulder_height_m']
                self.assertLess(sleep['head'][2, 3], idle['head'][2, 3] - .015 * height)
                for key, leg in solver.rig.legs.items():
                    lift = sleep[leg.toes][2, 3] - solver.toe_base(key)[2]
                    if key == 'HR':
                        self.assertAlmostEqual(lift, .01 * height, places=6)
                    else:
                        self.assertAlmostEqual(lift, 0, places=6, msg=f'{species}/{key}/{phase}')

    def test_trunk_reaches_mouth_and_raises_above_head(self):
        for species, solver, clips in self.cases:
            if not species.startswith('elephant'):
                continue
            spec = self.specs[species]
            self.assertGreaterEqual(sum(n.startswith('trunk') for n in solver.order), 8)
            by = {c.name: c for c in clips}
            eat = solver.solve(by['eat'].pose(.5))
            tip = solver.point(eat, 'trunk10', solver.tail['trunk10'])
            mouth = solver.point(eat, 'head', solver.rig.extra['mouth'])
            self.assertLess(np.linalg.norm(tip-mouth), .12 * spec.meta['shoulder_height_m'], species)
            call = solver.solve(by['vocalize'].pose(.5))
            tip = solver.point(call, 'trunk10', solver.tail['trunk10'])
            self.assertGreater(tip[2], call['head'][2, 3], species)

    def test_idle_ears_move(self):
        for species, solver, clips in self.cases:
            idle = next(c for c in clips if c.name == 'idle')
            for ear in solver.rig.ears:
                frames = [solver.solve(idle.pose(i / 24))[ear][:3, :3] for i in range(24)]
                self.assertGreater(np.ptp(np.array(frames), axis=0).max(), .05, species)

    def test_exported_elephant_albedo_ao_and_tusks(self):
        reference = measure(ROOT / 'assets/animals/rhino.glb')['luminance']
        for species in ('elephant', 'elephant_female', 'elephant_juvenile'):
            with self.subTest(species=species):
                metrics = measure(ROOT / 'assets/animals' / (species + '.glb'))
                self.assertGreaterEqual(metrics['luminance'] / reference, .75)
                self.assertLessEqual(metrics['luminance'] / reference, .90)
                red, green, blue = metrics['linear_rgb']
                self.assertGreaterEqual(red, green)
                self.assertGreaterEqual(green, blue)
                self.assertLess(red / blue, 1.25)
                self.assertGreater(metrics['ao_mean'], .8)
                self.assertLess(metrics['ao_mean'], .99)
                self.assertLess(metrics['ao_p05'], .90, 'Real creases must retain occlusion')
                if species.endswith('juvenile'):
                    self.assertEqual(metrics['tusk_chord_m'], [])
                    self.assertEqual(metrics['tusk_arc_m'], [])
                else:
                    lower, upper = (.6, .9) if species == 'elephant' else (.3, .5)
                    self.assertEqual(len(metrics['tusk_arc_m']), 2)
                    for length in metrics['tusk_arc_m']:
                        self.assertGreaterEqual(length, lower)
                        self.assertLessEqual(length, upper)

    def test_elephant_ears_below_crown_and_rest_curl(self):
        for species, solver, clips in self.cases:
            if not species.startswith('elephant'):
                continue
            data = self.specs[species].sculpt
            names = data['bone_names'].tolist()
            ear = data['tags'][:, data['tag_names'].tolist().index('ear')] > .5
            crown = (data['weights'][:, names.index('head')] > .65) & ~ear
            self.assertLess(data['vertices'][ear, 2].max(), data['vertices'][crown, 2].max())
            if species.endswith('juvenile'):
                continue
            rest = next(clip for clip in clips if clip.name == 'rest')
            for phase in np.linspace(0, 1, 13):
                pose = rest.pose(float(phase))
                points = skin_points(solver, pose, data)
                self.assertLess(points[ear, 2].max(), points[crown, 2].max(), species)
                world = solver.solve(pose)
                tip = solver.point(world, 'trunk10', solver.tail['trunk10'])
                curl_base = world['trunk8'][:3, 3]
                self.assertGreater(tip[2], curl_base[2], 'Relaxed tip must turn upward')
                self.assertLess(tip[2], .4 * data['height'], 'Rest curl must remain low')

    def test_glb_material_skin_budgets_and_manifest(self):
        for species, _, clips in self.cases:
            path = ROOT / 'assets/animals' / (species + '.glb')
            juvenile = species.endswith('juvenile')
            self.assertLessEqual(path.stat().st_size, 4_000_000 if juvenile else 7_000_000)
            doc, binary = glb(path)
            manifest = json.loads(path.with_suffix('.json').read_text())
            self.assertEqual(manifest['name'], species)
            self.assertEqual(len(doc['materials']), 1)
            mat = doc['materials'][0]
            self.assertIn('baseColorTexture', mat['pbrMetallicRoughness'])
            self.assertIn('normalTexture', mat)
            self.assertIn('occlusionTexture', mat)
            self.assertEqual(mat['pbrMetallicRoughness'].get('metallicFactor'), 0)
            self.assertEqual({a['name'] for a in doc['animations']}, clip_set(species))
            self.assertLessEqual(len(doc['skins'][0]['joints']), 64)
            triangles = 0
            for mesh in doc['meshes']:
                for primitive in mesh['primitives']:
                    triangles += doc['accessors'][primitive['indices']]['count'] // 3
                    attrs = primitive['attributes']
                    self.assertIn('TANGENT', attrs)
                    self.assertNotIn('WEIGHTS_1', attrs)
                    weights = accessor(doc, binary, attrs['WEIGHTS_0'])
                    self.assertTrue(np.allclose(weights.sum(axis=1), 1, atol=1e-5))
            self.assertEqual(triangles, manifest['triangles'])
            self.assertLessEqual(triangles, 22000 if juvenile else 34000)
            self.assertEqual(manifest['bones'], len(doc['skins'][0]['joints']))
            for image in doc['images']:
                self.assertIn(image['mimeType'], ('image/png', 'image/jpeg'))
            meta = manifest['meta']
            self.assertAlmostEqual(meta['swim_depth_m'], .82 * meta['shoulder_height_m'])
            if '_' in species:
                base, variant = species.split('_')
                self.assertEqual(meta['variant_of'], base)
                self.assertEqual(meta['variant'], variant)
            if species == 'elephant_female':
                self.assertEqual(meta['relative_height'], .81)
            by = {c['name']: c for c in manifest['clips']}
            for clip in clips:
                self.assertAlmostEqual(by[clip.name]['speed'], clip.speed, places=4)
            spec, solver = common.make_solver(species)
            body = next(i for i, node in enumerate(doc['nodes']) if node.get('name') == 'hips')
            by_pose = {c.name: c for c in spec.clips(solver)}
            for animation in doc['animations']:
                channel = next(c for c in animation['channels']
                               if c['target'] == {'node': body, 'path': 'translation'})
                samples = accessor(doc, binary, animation['samplers'][channel['sampler']]['output'])
                expected = solver.solve(by_pose[animation['name']].pose(0))['hips'][:3, 3]
                exported = world_translation(doc, body, samples[0])
                self.assertTrue(np.allclose(exported, expected[[1, 2, 0]], atol=3e-5),
                                f'{species}/{animation["name"]}/export')


if __name__ == '__main__':
    unittest.main()
