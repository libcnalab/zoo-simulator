# SPDX-License-Identifier: MIT
"""Anatomically anchored coarse hair groups over an opaque shadow-casting core."""
import math

import numpy as np


def surface_normals(data):
    vertices, triangles = data['vertices'], data['triangles']
    faces = np.cross(vertices[triangles[:, 1]] - vertices[triangles[:, 0]],
                     vertices[triangles[:, 2]] - vertices[triangles[:, 0]])
    normals = np.zeros_like(vertices)
    for corner in range(3):
        np.add.at(normals, triangles[:, corner], faces)
    normals /= np.maximum(np.linalg.norm(normals, axis=1)[:, None], 1e-8)
    return normals


def mane_sections(data):
    names = data['bone_names'].tolist()
    head = data['heads'][names.index('head')]
    neck = data['heads'][names.index('neck1')]
    chest = data['heads'][names.index('chest')]
    height = data['height']
    centers = [chest + height * np.array([-.22, 0, .025]),
               chest + height * np.array([-.06, 0, .025]),
               neck, (neck + head) / 2, head - height * np.array([.045, 0, 0]),
               head + height * np.array([.07, 0, -.005])]
    return np.array(centers), height * np.array([.14, .25, .25, .23, .21, .185]), \
        height * np.array([.13, .20, .22, .20, .18, .15]), \
        height * np.array([.15, .24, .34, .32, .28, .22])


def cards(data):
    names = data['bone_names'].tolist()
    head = data['heads'][names.index('head')]
    neck = data['heads'][names.index('neck1')]
    height, kind = data['height'], data['kind']
    result = []
    if kind == 'male':
        from .geom import R, _resample
        centers, widths, tops, bottoms = mane_sections(data)
        rings = _resample([R(center, width, top, bottom)
                           for center, width, top, bottom in zip(centers, widths, tops, bottoms)], 2)
        centers = np.array([ring.c for ring in rings])
        widths = np.array([ring.ry for ring in rings])
        tops = np.array([ring.up for ring in rings])
        bottoms = np.array([ring.dn for ring in rings])
        for layer in range(22):
            for index in range(84):
                scatter = (math.sin(index * 19.37 + layer * 63.17) * 43758.5453) % 1
                t = float(np.clip((layer + 1.8 * scatter - .9) / 21, 0, 1))
                position = t * (len(rings) - 1)
                interval = min(len(rings) - 2, int(position))
                fraction = position - interval
                center = centers[interval] * (1 - fraction) + centers[interval + 1] * fraction
                width = np.interp(position, np.arange(len(rings)), widths)
                top = np.interp(position, np.arange(len(rings)), tops)
                bottom = np.interp(position, np.arange(len(rings)), bottoms)
                tangent = centers[interval + 1] - centers[interval]
                tangent /= np.linalg.norm(tangent)
                vertical = np.cross(tangent, [0, 1, 0])
                angle = 2 * math.pi * (index + .43 * layer + .10 * math.sin(index * 7.1)) / 84
                side, up = math.cos(angle), math.sin(angle)
                radial = np.array([0, side * width, 0]) + vertical * up * (top if up > 0 else bottom)
                normal = np.array([.75 * t ** 4, side, 0]) + vertical * up
                normal /= np.linalg.norm(normal)
                root = center + radial * 1.008
                length = height * (.060 + .018 * (1 - up)
                                   + .012 * (.5 + .5 * math.sin(index * 7.7 + layer)))
                if up > .4:
                    length *= .78
                second = math.sin(index * 11.91 + layer * 3.17)
                direction = np.array([-.74 + .10 * second,
                                      .16 * side + .08 * (scatter - .5),
                                      -.34 + .20 * up + .06 * second])
                if t > .85:
                    direction = np.array([-.08 + .05 * second,
                                          .50 * side + .08 * (scatter - .5),
                                          -.50 + .24 * up + .05 * second])
                direction /= np.linalg.norm(direction)
                if t > .75:
                    weights = {'head': (t - .75) / .25, 'neck2': 1 - (t - .75) / .25}
                elif t > .3:
                    weights = {'neck1': 1 - (t - .3) / .45, 'neck2': (t - .3) / .45}
                else:
                    weights = {'chest': 1 - t / .3, 'neck1': t / .3}
                palette = int(np.clip(round(13 * (1 - t ** 2) + .6 * scatter), 0, 13))
                lock_width = .018 * height * (.80 + .25 * math.sin(index * 5.3 + layer) ** 2)
                result.append((root, direction * length, normal, lock_width, weights, palette))
        # Radial locks cover the frontal cap; longitudinal cards alone are edge-on here.
        center = centers[-1].copy()
        center[0] += .006 * height
        for layer in range(4):
            for index in range(72):
                angle = 2 * math.pi * (index + .37 * layer) / 72
                side, up = math.cos(angle), math.sin(angle)
                radial = np.array([0, side * widths[-1], up * (tops[-1] if up > 0 else bottoms[-1])])
                root = center + radial * (.62 + .10 * layer)
                normal = np.array([1., .2 * side, .2 * up])
                normal /= np.linalg.norm(normal)
                direction = radial * (.18 + .025 * math.sin(index * 7.3))
                direction[0] = .004 * height
                result.append((root, direction, normal, .020 * height, {'head': 1}, 0))
        # Short forehead crest closes the bare gap between the ears without covering the muzzle.
        for row in range(3):
            for index in range(21):
                y = (index - 10) * .009 * height
                root = head + height * np.array([.072 - row * .032, y / height, .125])
                surface = data['vertices'][data['vertices'][:, 2] > head[2] + .05 * height]
                distances = np.sum((surface[:, :2] - root[:2]) ** 2, axis=1)
                root[2] = max(surface[np.argmin(distances), 2], head[2] + tops[-1]) + .006 * height
                normal = np.array([.5, y / height * 3, 1.])
                normal /= np.linalg.norm(normal)
                result.append((root, height * np.array([-.035, .010 * np.sign(y), .008]), normal,
                               .013 * height, {'head': 1}, 0))
    if kind == 'tiger':
        face = data['weights'][:, [names.index('head'), names.index('jaw')]].sum(axis=1) > .8
        normals = surface_normals(data)
        for side in (-1, 1):
            indices = np.flatnonzero(face & (data['vertices'][:, 1] * side > 0))
            surface = data['vertices'][indices]
            for layer in range(5):
                for index in range(15):
                    scatter = math.sin(index * 17.31 + layer * 8.1 + side * 1.7)
                    vertical = (index * .61803398875 + layer * .271828 + .035 * scatter) % 1
                    target = head + height * np.array([-.006 - .017 * layer + .006 * scatter,
                                                        0, -.130 + .155 * vertical])
                    distance = (surface[:, 0] - target[0]) ** 2 + (surface[:, 2] - target[2]) ** 2
                    nearby = np.argsort(distance)[:16]
                    chosen = nearby[np.argmax(surface[nearby, 1] * side)]
                    # Start on the sculpt's cheek, so only the tapered tips extend its silhouette.
                    normal = normals[indices[chosen]]
                    root = surface[chosen] + normal * .0012 * height
                    direction = height * np.array([
                        -.020 - .008 * layer / 4,
                        side * (.014 + .008 * (1 - vertical) + .003 * scatter),
                        -.010 - .016 * (1 - vertical)])
                    palette = 14 if (index + 2 * layer) % 11 == 0 else \
                        13 if layer == 4 and index % 3 == 1 else 15
                    width = height * (.007 + .003 * math.sin(index * 5.3 + layer) ** 2)
                    result.append((root, direction, normal, width, {'head': 1}, palette))
    if kind in ('female', 'male', 'juvenile'):
        tip = data['tails'][names.index('tail4')]
        for layer in range(3):
            for index in range(14):
                angle = index * 2 * math.pi / 14 + .4 * layer
                normal = np.array([-.2, math.cos(angle), math.sin(angle)])
                normal /= np.linalg.norm(normal)
                root = tip + height * np.array([.025 - layer * .015, .014 * math.cos(angle),
                                                .014 * math.sin(angle)])
                result.append((root, height * np.array([-.06, .012 * math.cos(angle),
                                                       .012 * math.sin(angle)]), normal,
                               .024 * height, {'tail4': 1}, 14))
    return result


def geometry(data):
    vertices, triangles, uv, normals, weights = [], [], [], [], []
    names = data['bone_names'].tolist()
    trunk_indices = [names.index(name) for name in ('hips', 'spine1', 'spine2', 'chest')]
    trunk = data['weights'][:, trunk_indices].sum(axis=1) > .8
    belly = data['vertices'][trunk, 2].min() + .040 * data['height']
    for root, direction, normal, width, influence, palette in cards(data):
        tangent = np.cross(direction, normal)
        tangent /= max(np.linalg.norm(tangent), 1e-8)
        offset = len(vertices)
        for row, t in enumerate((0., .5, 1.)):
            curve = .070 if data['kind'] == 'male' and 'tail4' not in influence else .035
            center = root + direction * t + normal * np.linalg.norm(direction) * curve * math.sin(t * math.pi)
            for side in (-1, 1):
                point = center + tangent * width * .5 * side * (1 - .80 * t)
                if data['kind'] == 'male' and 'tail4' not in influence:
                    # Chest locks end at the belly contact plane, retaining paw contact when recumbent.
                    point[2] = max(point[2], belly + .008 * data['height'] * math.sin(offset * 2.3) ** 2)
                vertices.append(point)
                normals.append(normal)
                weights.append(influence)
        for row in range(2):
            for local in ((0, 1, 3), (0, 3, 2)):
                ids = np.array(local) + 2 * row
                triangles.append(ids + offset)
                uv.append([((palette + .03 + .94 * (int(i) % 2)) / 16,
                            .02 + .96 * (int(i) // 2) / 2) for i in ids])
    return np.array(vertices), np.array(triangles), np.array(uv), np.array(normals), weights


def hair_palette(name):
    palette = np.empty((16, 3))
    for index in range(14):
        blend = index / 13
        palette[index] = np.array([.48, .27, .095]) * (1 - blend) + np.array([.20, .10, .04]) * blend
    palette[14] = [.055, .027, .012]
    palette[15] = [.61, .59, .54]
    if name.startswith('tiger'):
        palette[13] = [.54, .23, .045]
        palette[14] = [.014, .011, .009]
        palette[15] = [.86, .79, .67]
    return palette


def hair_material(name):
    import bpy
    width, height = 1024, 512
    yy, xx = np.mgrid[:height, :width]
    u = (xx % (width // 16)) / (width // 16)
    v = yy / height
    phase = u * 5 + .17 * np.sin(v * 8 + u * 17)
    strand = np.abs(np.sin(phase * math.pi))
    tip = .72 + .25 * np.sin(np.floor(u * 5) * 17.3) ** 2
    edge = .35 * (1 - np.sin(np.pi * u) ** .4)
    alpha = ((strand > .22 + .62 * v ** 3 + edge) & (v < tip + .10 * strand)).astype(float)
    palette = hair_palette(name)
    rgb = palette[np.minimum(xx // (width // 16), 15)] * (.65 + .35 * strand[..., None])
    rgba = np.concatenate([rgb, alpha[..., None]], axis=-1)
    image = bpy.data.images.new(name + '_hair_groups', width, height, alpha=True)
    image.pixels.foreach_set(rgba.astype(np.float32).ravel())
    image.pack()
    mat = bpy.data.materials.new(name + '_hair')
    mat.use_nodes = True
    mat.surface_render_method = 'DITHERED'
    mat.alpha_threshold = .5
    mat.use_backface_culling = False
    shader = mat.node_tree.nodes['Principled BSDF']
    shader.inputs['Roughness'].default_value = .87
    texture = mat.node_tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    mat.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
    # A threshold node exports as alphaMode MASK under Blender 4.3's glTF exporter.
    threshold = mat.node_tree.nodes.new('ShaderNodeMath')
    threshold.operation = 'GREATER_THAN'
    threshold.inputs[1].default_value = .5
    mat.node_tree.links.new(texture.outputs['Alpha'], threshold.inputs[0])
    mat.node_tree.links.new(threshold.outputs[0], shader.inputs['Alpha'])
    return mat


def attach_cards(body, data, names):
    import bpy
    from . import build
    vertices, triangles, uvs, normals, weights = geometry(data)
    if len(vertices) == 0:
        return
    ob = build._mesh_object(data['kind'] + '_hair_cards', build.to_blender(vertices), triangles)
    ob.data.uv_layers.new(name=body.data.uv_layers.active.name)
    ob.data.uv_layers.active.data.foreach_set('uv', uvs.ravel())
    for name in names:
        group = ob.vertex_groups.new(name=name)
        for index, influence in enumerate(weights):
            if influence.get(name, 0) > 0:
                group.add([index], influence[name], 'REPLACE')
    for poly in ob.data.polygons:
        poly.use_smooth = True
    ob.data.normals_split_custom_set_from_vertices(build.to_blender(normals))
    ob.data.materials.append(hair_material(body.name))
    build._activate(body)
    ob.select_set(True)
    bpy.ops.object.join()
