# SPDX-License-Identifier: MIT
"""Giraffe anatomy, contact motion, portable source, and exported asset contracts."""
from collections import Counter
import io
import json
from pathlib import Path
import unittest

import numpy as np
from PIL import Image, ImageDraw

from test_clips import make_solver
from test_pachyderms import accessor, glb
from zoogen import geom, quadruped
from zoogen.giraffe2 import SOURCE, adult_space, scan_weights

ROOT = Path(__file__).resolve().parents[3]
IDS = ('giraffe','giraffe_female','giraffe_juvenile')


def body_texels(path, variant):
    """Read the exported colour map only where torso triangles actually use it."""
    doc, binary = glb(path)
    texture = doc['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index']
    source = doc['textures'][texture]['source']
    view = doc['bufferViews'][doc['images'][source]['bufferView']]
    offset = view.get('byteOffset', 0)
    image = Image.open(io.BytesIO(binary[offset:offset + view['byteLength']])).convert('RGB')
    mask = Image.new('1', image.size)
    draw = ImageDraw.Draw(mask)
    for mesh in doc['meshes']:
        for primitive in mesh['primitives']:
            vertices = accessor(doc, binary, primitive['attributes']['POSITION'])[:, [2, 0, 1]]
            vertices = adult_space(vertices, variant)
            uv = accessor(doc, binary, primitive['attributes']['TEXCOORD_0'])
            triangles = accessor(doc, binary, primitive['indices']).reshape(-1, 3)
            for tri in triangles:
                points = vertices[tri]
                if ((points[:, 2] > 1.9).all() and (points[:, 2] < 3.4).all()
                        and (points[:, 0] > -1.35).all()):
                    draw.polygon([tuple(v) for v in uv[tri] * image.size], fill=1)
    return np.asarray(image)[np.asarray(mask)] / 255


def coat_statistics(rgb):
    maximum = rgb.max(1)
    delta = maximum - rgb.min(1)
    saturation = delta / np.maximum(maximum, 1e-9)
    hue = np.zeros(len(rgb))
    for channel in range(3):
        selected = (rgb.argmax(1) == channel) & (delta > 0)
        hue[selected] = (60 * ((rgb[selected, (channel + 1) % 3]
                               - rgb[selected, (channel + 2) % 3]) / delta[selected]
                              + 2 * channel)) % 360
    linear = np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)
    luminance = linear @ [.2126, .7152, .0722]
    # Quantiles isolate patch interiors and cream lines in both the old olive
    # export and the new coat, without classifying the old dark cream as a patch.
    patch = (saturation > .4) & (maximum < .78) & (luminance <= np.quantile(luminance, .6))
    line = luminance >= np.quantile(luminance, .9)
    return {'texels': len(rgb),
            'green_texels': int(((hue >= 65) & (hue <= 170) & (saturation > .15)).sum()),
            'patch_texels': int(patch.sum()), 'patch_hue': float(hue[patch].mean()),
            'line_luminance': float(luminance[line].mean()),
            'line_saturation': float(saturation[line].mean())}


class GiraffeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = []
        for name in IDS:
            spec,solver = make_solver(name)
            cls.cases.append((name,spec,solver,{c.name:c for c in spec.clips(solver)}))

    def test_portable_derivative(self):
        with np.load(SOURCE,allow_pickle=False) as data:
            vertices,triangles,uv = data['vertices'],data['triangles'],data['uv']
            self.assertTrue(np.isfinite(vertices).all())
            self.assertGreaterEqual(triangles.min(),0)
            self.assertLess(triangles.max(),len(vertices))
            self.assertEqual(uv.shape,(len(triangles),3,2))
            self.assertLessEqual(len(triangles),14000)
        self.assertLess(sum(p.stat().st_size for p in SOURCE.parent.iterdir() if p.is_file()),14_000_000)

    def test_weights_and_height(self):
        for name,spec,_,_ in self.cases:
            bones = geom.expand_bones(spec.bones)
            self.assertLessEqual(len(bones),60,name)
            weights = scan_weights(spec.sculpt['vertices'],bones,spec.sculpt['variant'])
            self.assertTrue(np.isfinite(weights).all())
            self.assertTrue((weights>=0).all())
            self.assertTrue(((weights>0).sum(1)<=4).all())
            self.assertTrue(np.allclose(weights.sum(1),1))
            self.assertAlmostEqual(spec.sculpt['vertices'][:,2].max(),spec.meta['shoulder_height_m'],places=5)

    def test_no_bone_below_ground(self):
        for name,spec,solver,clips in self.cases:
            for clip in clips.values():
                solver.reset_stats()
                for phase in np.arange(24)/24:
                    world = solver.solve(clip.pose(phase))
                    for bone in geom.expand_bones(spec.bones):
                        if not bone.deform:
                            continue
                        for point in (bone.head,bone.tail):
                            self.assertGreaterEqual(solver.point(world,bone.name,point)[2],-.02,
                                                    f'{name}/{clip.name}/{bone.name}/{phase}')
                self.assertLess(solver.ik_error,1e-6,f'{name}/{clip.name}')
                self.assertLess(solver.extension,.99,f'{name}/{clip.name}')

    def test_lateral_walk_touchdown(self):
        for name,_,solver,clips in self.cases:
            contacts = {}
            walk = clips['walk']
            for key,leg in solver.rig.legs.items():
                previous = False
                for i in range(-1,120):
                    world = solver.solve(walk.pose((i/120)%1))
                    contact = abs(world[leg.toes][2,3]-solver.toe_base(key)[2])<1e-7
                    if i>=0 and contact and not previous:
                        contacts[key] = i/120
                    previous = contact
            self.assertEqual(len(contacts),4,name)
            for side in ('L','R'):
                distance = abs(contacts['F'+side]-contacts['H'+side])
                self.assertLess(min(distance,1-distance),.25,name)

    def test_stance_sliding(self):
        for name,_,solver,clips in self.cases:
            for clip in (clips['walk'],clips['trot']):
                for key,leg in solver.rig.legs.items():
                    count = 0
                    for i in range(48):
                        p0 = solver.solve(clip.pose(i/48))[leg.toes][:3,3]
                        p1 = solver.solve(clip.pose(((i+1)/48)%1))[leg.toes][:3,3]
                        if max(p0[2],p1[2])>solver.toe_base(key)[2]+1e-6:
                            continue
                        count += 1
                        speed = (p1[0]-p0[0])/(clip.duration/48)
                        self.assertAlmostEqual(speed,-clip.speed,delta=.02*clip.speed+1e-6,
                                               msg=f'{name}/{clip.name}/{key}')
                    self.assertGreater(count,.2*48)

    def test_high_browsing_and_ground_drinking(self):
        _,_,solver,clips = self.cases[0]
        for phase in np.arange(12)/12:
            drink = solver.solve(clips['drink'].pose(phase))
            eat = solver.solve(clips['eat'].pose(phase))
            self.assertLess(drink['head'][2,3],.3)
            self.assertGreater(eat['head'][2,3],4)
            self.assertLess(quadruped.muzzle(solver,clips['drink'].pose(phase))[2],.06)
            self.assertGreater(abs(drink['fingers.L'][1,3]),.8)

    def test_recumbent_postures(self):
        for name,spec,solver,clips in self.cases:
            for phase in (0,.25,.5,.75):
                rest = solver.solve(clips['rest'].pose(phase))
                sleep = solver.solve(clips['sleep'].pose(phase))
                self.assertLess(rest['chest'][2,3],.4*spec.meta['withers_height_m'],name)
                self.assertGreater(rest['head'][2,3],rest['chest'][2,3],name)
                self.assertLess(sleep['head'][0,3],sleep['chest'][0,3],name)
                self.assertGreater(sleep['head'][2,3],sleep['hips'][2,3]+.10,name)
                self.assertLess(sleep['head'][2,3],rest['head'][2,3]*.6,name)

    def test_recumbent_chest_reaches_ground(self):
        for name,spec,solver,clips in self.cases:
            underside = geom.loft(spec.parts[0])[0][:,2].min()
            for clip_name in ('rest','sleep'):
                world = solver.solve(clips[clip_name].pose(0))
                clearance = underside + world['hips'][2,3] - solver.head['hips'][2]
                self.assertGreaterEqual(clearance,-.01,name)
                self.assertLess(clearance,.025,name)

    def test_exported_head_matches_authored_postures(self):
        for name,_,solver,clips in self.cases:
            doc,binary = glb(ROOT/'assets/animals'/(name+'.glb'))
            target = next(i for i,node in enumerate(doc['nodes']) if node.get('name')=='head')
            parents = {child:i for i,node in enumerate(doc['nodes']) for child in node.get('children',[])}
            for animation in doc['animations']:
                if animation['name'] not in ('eat','drink','rest','sleep'):
                    continue
                nodes = [dict(node) for node in doc['nodes']]
                for channel in animation['channels']:
                    sampler = animation['samplers'][channel['sampler']]
                    value = accessor(doc,binary,sampler['output'])[0]
                    nodes[channel['target']['node']][channel['target']['path']] = value
                matrix = np.eye(4)
                index = target
                while True:
                    node = nodes[index]
                    x,y,z,w = node.get('rotation',[0,0,0,1])
                    rotation = np.array([
                        [1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                        [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                        [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
                    local = np.eye(4)
                    local[:3,:3] = rotation @ np.diag(node.get('scale',[1,1,1]))
                    local[:3,3] = node.get('translation',[0,0,0])
                    matrix = local @ matrix
                    if index not in parents:
                        break
                    index = parents[index]
                expected = solver.solve(clips[animation['name']].pose(0))['head'][:3,3]
                self.assertTrue(np.allclose(matrix[:3,3],expected[[1,2,0]],atol=2e-4),
                                f"{name}/{animation['name']}: stale or transformed clip")

    def test_exported_reticulated_coat_colours(self):
        for name, spec, _, _ in self.cases:
            rgb = body_texels(ROOT / 'assets/animals' / (name + '.glb'), spec.sculpt['variant'])
            stats = coat_statistics(rgb)
            self.assertGreater(stats['texels'], 10000, name)
            self.assertEqual(stats['green_texels'], 0, name)
            self.assertGreater(stats['patch_texels'], .25 * stats['texels'], name)
            self.assertGreaterEqual(stats['patch_hue'], 15, name)
            self.assertLessEqual(stats['patch_hue'], 35, name)
            self.assertGreater(stats['line_luminance'], .7, name)
            self.assertLess(stats['line_saturation'], .3, name)

    def test_exported_neck_graft_has_no_open_boundary(self):
        for name, spec, _, _ in self.cases:
            doc, binary = glb(ROOT / 'assets/animals' / (name + '.glb'))
            edges = Counter()
            for mesh in doc['meshes']:
                for primitive in mesh['primitives']:
                    vertices = accessor(doc, binary, primitive['attributes']['POSITION'])[:, [2, 0, 1]]
                    vertices = adult_space(vertices, spec.sculpt['variant'])
                    triangles = accessor(doc, binary, primitive['indices']).reshape(-1, 3)
                    for tri in triangles:
                        for i in range(3):
                            pair = vertices[[tri[i], tri[(i + 1) % 3]]]
                            # Enclose both ends of the transition, excluding the older
                            # scan's submillimetre surface fragments farther up the neck.
                            if (pair[:, 2] > 3.6).all() and (pair[:, 2] < 3.84).all():
                                ends = [tuple(point) for point in np.round(pair, 5)]
                                if ends[0] != ends[1]:
                                    edges[tuple(sorted(ends))] += 1
            self.assertGreater(len(edges), 50, name)
            exposed = [edge for edge, count in edges.items() if count == 1]
            self.assertEqual(len(exposed), 0, f'{name}: exposed neck edges {exposed[:3]}')

    def test_exported_hoof_dimensions_and_flat_soles(self):
        for name, spec, _, _ in self.cases:
            doc, binary = glb(ROOT / 'assets/animals' / (name + '.glb'))
            joint_names = [doc['nodes'][index]['name'] for index in doc['skins'][0]['joints']]
            scale = .46 if name.endswith('juvenile') else spec.meta['shoulder_height_m'] / 5.2
            for bone in ('fingers.L', 'fingers.R', 'toes.L', 'toes.R'):
                points = []
                joint = joint_names.index(bone)
                for mesh in doc['meshes']:
                    for primitive in mesh['primitives']:
                        attrs = primitive['attributes']
                        vertices = accessor(doc, binary, attrs['POSITION'])[:, [2, 0, 1]]
                        joints = accessor(doc, binary, attrs['JOINTS_0'])
                        weights = accessor(doc, binary, attrs['WEIGHTS_0'])
                        rigid = ((joints == joint) * weights).sum(1) > .999
                        points.extend(vertices[rigid & (vertices[:, 2] < .17 * scale)])
                points = np.asarray(points) / scale
                self.assertGreater(len(points), 40, f'{name}/{bone}')
                height = np.ptp(points[:, 2])
                length = np.ptp(points[:, 0])
                self.assertGreaterEqual(height, .13, f'{name}/{bone}: {height}')
                self.assertLessEqual(height, .17, f'{name}/{bone}: {height}')
                self.assertGreaterEqual(length, .22, f'{name}/{bone}: {length}')
                self.assertLessEqual(length, .28, f'{name}/{bone}: {length}')
                sole = points[points[:, 2] < points[:, 2].min() + .001]
                self.assertGreaterEqual(len(sole), 20, f'{name}/{bone}')
                self.assertLess(np.ptp(sole[:, 2]), .001, f'{name}/{bone}')
                self.assertGreaterEqual(points[:, 2].min(), -.001, f'{name}/{bone}')
                self.assertLess(points[:, 2].min(), .01, f'{name}/{bone}')

    def test_manifests_and_glb(self):
        for name,spec,_,clips in self.cases:
            asset = ROOT/'assets/animals'/name
            manifest = json.loads(asset.with_suffix('.json').read_text())
            self.assertEqual(manifest['meta']['shoulder_height_m'],spec.meta['shoulder_height_m'])
            self.assertEqual(manifest['license'],'CC-BY-4.0')
            self.assertEqual({c['name'] for c in manifest['clips']},set(clips))
            self.assertLessEqual(manifest['bones'],60)
            self.assertLessEqual(manifest['triangles'],20000 if name.endswith('juvenile') else 30000)
            self.assertLessEqual(manifest['texture'],2048)
            if name!='giraffe':
                self.assertEqual(manifest['meta']['variant_of'],'giraffe')
                self.assertAlmostEqual(manifest['meta']['relative_height'],spec.meta['shoulder_height_m']/5.2)
            doc,binary = glb(asset.with_suffix('.glb'))
            self.assertEqual(len(doc['images']),2)
            self.assertEqual(len(doc['materials']),1)
            self.assertIn('normalTexture',doc['materials'][0])
            self.assertIn('baseColorTexture',doc['materials'][0]['pbrMetallicRoughness'])
            triangles = 0
            for mesh in doc['meshes']:
                for primitive in mesh['primitives']:
                    self.assertIn('TANGENT',primitive['attributes'])
                    self.assertIn('WEIGHTS_0',primitive['attributes'])
                    weights = accessor(doc,binary,primitive['attributes']['WEIGHTS_0'])
                    self.assertTrue(np.isfinite(weights).all())
                    self.assertTrue((weights>=0).all())
                    self.assertTrue(np.allclose(weights.sum(1),1,atol=1e-5))
                    triangles += doc['accessors'][primitive['indices']]['count']//3
            self.assertEqual(triangles,manifest['triangles'])


if __name__ == '__main__':
    unittest.main()
