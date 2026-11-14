# SPDX-License-Identifier: MIT
"""Contracts for the S7 hair skin and complete recumbent surface."""
import unittest

import numpy as np

import test_clips as common
from zoogen import geom
from zoogen.bigcat_hair import cards, geometry
from zoogen.pachyderm_motion import skin_points


def complete_skin(spec):
    data = spec.sculpt
    names = data['bone_names'].tolist()
    points, influences = [data['vertices']], [data['weights']]
    extras = []
    for part in geom.expand_parts(spec.parts):
        vertices, _, weights = geom.loft(part)
        extras.append((vertices, weights))
    vertices, _, _, _, weights = geometry(data)
    extras.append((vertices, weights))
    for vertices, weights in extras:
        matrix = np.zeros((len(vertices), len(names)))
        for index, dictionary in enumerate(weights):
            for name, value in dictionary.items():
                matrix[index, names.index(name)] = value
        points.append(vertices)
        influences.append(matrix)
    return dict(data, vertices=np.concatenate(points), weights=np.concatenate(influences))


class BigCatMaterialTests(unittest.TestCase):
    def test_hair_influences(self):
        for species in common.CATS:
            spec, _ = common.make_solver(species)
            for part in geom.expand_parts(spec.parts):
                if part.tag != 'mane':
                    continue
                _, _, core_weights = geom.loft(part)
                for weights in core_weights:
                    self.assertTrue(set(weights) <= {'head', 'neck1', 'neck2', 'chest'})
                    self.assertLessEqual(len(weights), 4)
                    self.assertAlmostEqual(sum(weights.values()), 1)
            for root, direction, normal, width, weights, _ in cards(spec.sculpt):
                self.assertTrue(np.isfinite(root).all())
                self.assertGreater(np.linalg.norm(direction), 0)
                self.assertAlmostEqual(np.linalg.norm(normal), 1)
                self.assertGreater(width, 0)
                self.assertLessEqual(len(weights), 4)
                self.assertTrue(all(weight >= 0 for weight in weights.values()))
                self.assertAlmostEqual(sum(weights.values()), 1)
                allowed = {'head', 'neck1', 'neck2', 'chest', 'tail4'}
                self.assertTrue(set(weights) <= allowed)
                if species == 'lion' and root[0] > 0:
                    self.assertTrue(set(weights) <= allowed - {'tail4'})

    def test_complete_recumbent_surface(self):
        for species in common.CATS:
            spec, solver = common.make_solver(species)
            data = complete_skin(spec)
            for clip in spec.clips(solver):
                if clip.name not in ('rest', 'sleep'):
                    continue
                for phase in np.linspace(0, 1, 17):
                    points = skin_points(solver, clip.pose(float(phase)), data)
                    self.assertGreaterEqual(points[:, 2].min(), -.02,
                                            f'{species}/{clip.name}/{phase}: complete skin penetrates floor')


def node_matrix(node):
    if 'matrix' in node:
        return np.array(node['matrix']).reshape(4, 4).T
    quaternion = np.array(node.get('rotation', [0, 0, 0, 1]), dtype=float)
    quaternion /= np.linalg.norm(quaternion)
    x, y, z, w = quaternion
    rotation = np.array([[1 - 2 * (y*y + z*z), 2 * (x*y - z*w), 2 * (x*z + y*w)],
                         [2 * (x*y + z*w), 1 - 2 * (x*x + z*z), 2 * (y*z - x*w)],
                         [2 * (x*z - y*w), 2 * (y*z + x*w), 1 - 2 * (x*x + y*y)]])
    matrix = np.eye(4)
    matrix[:3, :3] = rotation @ np.diag(node.get('scale', [1, 1, 1]))
    matrix[:3, 3] = node.get('translation', [0, 0, 0])
    return matrix


def animated_world(document, binary, animation, phase):
    from test_pachyderms import accessor
    nodes = [dict(node) for node in document['nodes']]
    for channel in animation['channels']:
        sampler = animation['samplers'][channel['sampler']]
        times = accessor(document, binary, sampler['input'])[:, 0]
        values = accessor(document, binary, sampler['output'])
        time = times[0] + phase * (times[-1] - times[0])
        high = min(int(np.searchsorted(times, time, side='right')), len(times) - 1)
        low = max(0, high - 1)
        fraction = (time - times[low]) / max(float(times[high] - times[low]), 1e-9)
        target = channel['target']
        first, second = values[low].astype(float), values[high].astype(float)
        if target['path'] == 'rotation' and np.dot(first, second) < 0:
            second = -second
        value = first * (1 - fraction) + second * fraction
        nodes[target['node']][target['path']] = value
    parents = {child: index for index, node in enumerate(nodes) for child in node.get('children', [])}
    world = {}

    def resolve(index):
        if index not in world:
            local = node_matrix(nodes[index])
            world[index] = resolve(parents[index]) @ local if index in parents else local
        return world[index]

    for index in range(len(nodes)):
        resolve(index)
    return world


class ExportedCatSurfaceTests(unittest.TestCase):
    def test_exported_rest_and_sleep_clear_floor(self):
        from test_pachyderms import ROOT, accessor, glb
        for species in common.CATS:
            document, binary = glb(ROOT / 'assets/animals' / (species + '.glb'))
            for animation in document['animations']:
                if animation['name'] not in ('rest', 'sleep'):
                    continue
                for phase in np.linspace(0, 1, 17):
                    world = animated_world(document, binary, animation, float(phase))
                    for node in document['nodes']:
                        if 'mesh' not in node or 'skin' not in node:
                            continue
                        skin = document['skins'][node['skin']]
                        inverse = accessor(document, binary, skin['inverseBindMatrices']).reshape(-1, 4, 4)
                        matrices = np.array([world[joint] @ inverse[index].T
                                             for index, joint in enumerate(skin['joints'])])
                        for primitive in document['meshes'][node['mesh']]['primitives']:
                            attributes = primitive['attributes']
                            vertices = accessor(document, binary, attributes['POSITION'])
                            weights = accessor(document, binary, attributes['WEIGHTS_0'])
                            joints = accessor(document, binary, attributes['JOINTS_0'])
                            self.assertTrue(np.allclose(weights.sum(axis=1), 1, atol=1e-5))
                            points = np.concatenate([vertices, np.ones((len(vertices), 1))], axis=1)
                            posed = np.zeros((len(vertices), 4))
                            for influence in range(4):
                                transformed = np.einsum('nij,nj->ni', matrices[joints[:, influence]], points)
                                posed += weights[:, influence, None] * transformed
                            self.assertGreaterEqual(posed[:, 1].min(), -.02,
                                                    f'{species}/{animation["name"]}/{phase}: exported skin')

    def test_exported_mane_joint_contract(self):
        from test_pachyderms import ROOT, accessor, glb
        document, binary = glb(ROOT / 'assets/animals/lion.glb')
        for node in document['nodes']:
            if 'mesh' not in node or 'skin' not in node:
                continue
            skin = document['skins'][node['skin']]
            names = [document['nodes'][index]['name'] for index in skin['joints']]
            for primitive in document['meshes'][node['mesh']]['primitives']:
                material = document['materials'][primitive['material']]
                if material.get('alphaMode') != 'MASK':
                    continue
                weights = accessor(document, binary, primitive['attributes']['WEIGHTS_0'])
                joints = accessor(document, binary, primitive['attributes']['JOINTS_0'])
                positions = accessor(document, binary, primitive['attributes']['POSITION'])
                self.assertEqual(weights.shape[1], 4)
                self.assertTrue(np.allclose(weights.sum(axis=1), 1, atol=1e-5))
                for joint in np.unique(joints[weights > 0]):
                    self.assertIn(names[int(joint)], {'head', 'neck1', 'neck2', 'chest', 'tail4'})
                # Animal forward X is glTF Z; tail cards lie behind zero, the mane ahead of it.
                mane = positions[:, 2] > 0
                self.assertTrue(mane.any())
                for joint in np.unique(joints[mane][weights[mane] > 0]):
                    self.assertIn(names[int(joint)], {'head', 'neck1', 'neck2', 'chest'})


class CoatContext:
    def __init__(self, positions, normal, bone):
        self.P = np.asarray(positions).reshape(1, -1, 3)
        self.N = np.broadcast_to(normal, self.P.shape)
        self.bone = bone

    def bones(self, *prefixes):
        return np.full(self.P.shape[:2], float(any(self.bone.startswith(p) for p in prefixes)))

    def tag(self, name):
        return np.zeros(self.P.shape[:2])


class TigerCoatTests(unittest.TestCase):
    def test_asymmetric_flanks_and_unstriped_white_chest(self):
        from zoogen.bigcat_coat import coat
        spec, _ = common.make_solver('tiger')
        source = np.broadcast_to([.35, .25, .12], (1, 101, 3))
        positions = np.array([[x, .20, .75] for x in np.linspace(-.5, .5, 101)])
        left = coat(CoatContext(positions, [0, 1, 0], 'spine1'), source, spec.sculpt)
        positions[:, 1] *= -1
        right = coat(CoatContext(positions, [0, -1, 0], 'spine1'), source, spec.sculpt)
        self.assertFalse(np.allclose(left, right))
        self.assertGreater(float(left.max() - left.min()), .3)
        positions[:, 0], positions[:, 2] = .45, .55
        chest = coat(CoatContext(positions, [1, 0, 0], 'chest'), source, spec.sculpt)
        self.assertGreater(float(chest.min()), .6)
        self.assertTrue(np.isfinite(chest).all())
        positions[:, 0], positions[:, 2] = .5, np.linspace(.05, .43, 101)
        foreleg = coat(CoatContext(positions, [1, 0, 0], 'forearm.L'), source, spec.sculpt)
        self.assertTrue(np.allclose(foreleg, foreleg[:, :1]))
