# SPDX-License-Identifier: MIT
"""Measurements of exported skin texels and ivory geometry, excluding unused atlas space."""
import json
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

def glb(path):
    with path.open('rb') as source:
        header = source.read(12)
        if header[:4] != b'glTF':
            raise ValueError('Not a GLB')
        size, _ = struct.unpack('<II', source.read(8))
        document = json.loads(source.read(size))
        size, _ = struct.unpack('<II', source.read(8))
        binary = source.read(size)
    return document, binary


def accessor(document, binary, index):
    item = document['accessors'][index]
    view = document['bufferViews'][item['bufferView']]
    dtype = {5121: '<u1', 5123: '<u2', 5125: '<u4', 5126: '<f4'}[item['componentType']]
    columns = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[item['type']]
    offset = view.get('byteOffset', 0) + item.get('byteOffset', 0)
    stride = view.get('byteStride', np.dtype(dtype).itemsize * columns)
    return np.ndarray((item['count'], columns), dtype=dtype, buffer=binary,
                      offset=offset, strides=(stride, np.dtype(dtype).itemsize))


def image_data(doc, binary, texture):
    import bpy
    item = doc['images'][doc['textures'][texture['index']]['source']]
    view = doc['bufferViews'][item['bufferView']]
    payload = binary[view.get('byteOffset', 0):view.get('byteOffset', 0) + view['byteLength']]
    scratch = Path(__file__).resolve().parents[3] / 'build-probe/animal-previews'
    scratch.mkdir(parents=True, exist_ok=True)
    suffix = '.png' if item['mimeType'] == 'image/png' else '.jpg'
    with tempfile.NamedTemporaryFile(dir=scratch, prefix='S6b_measure_', suffix=suffix) as temporary:
        temporary.write(payload)
        temporary.flush()
        image = bpy.data.images.load(temporary.name, check_existing=False)
        image.colorspace_settings.name = 'Non-Color'
        width, height = image.size
        pixels = np.empty(width * height * 4, dtype=np.float32)
        image.pixels.foreach_get(pixels)
        bpy.data.images.remove(image)
    # glTF's V origin is at the top; Blender's image rows start at the bottom.
    return pixels.reshape(height, width, 4)[::-1, :, :3].astype(np.float64)


def coverage(uv, size):
    """Rasterize only covered texel centers, never the atlas background or gutters."""
    mask = np.zeros(size, dtype=bool)
    for triangle in uv:
        points = triangle * [size[1], size[0]] - .5
        low = np.maximum(np.ceil(points.min(axis=0)).astype(int), 0)
        high = np.minimum(np.floor(points.max(axis=0)).astype(int), [size[1]-1, size[0]-1])
        if np.any(high < low):
            continue
        first, second = points[1] - points[0], points[2] - points[0]
        determinant = first[0] * second[1] - first[1] * second[0]
        if abs(determinant) < 1e-9:
            continue
        y, x = np.mgrid[low[1]:high[1]+1, low[0]:high[0]+1]
        dx, dy = x - points[0, 0], y - points[0, 1]
        u = (dx * second[1] - dy * second[0]) / determinant
        v = (first[0] * dy - first[1] * dx) / determinant
        mask[low[1]:high[1]+1, low[0]:high[0]+1] |= (u >= 0) & (v >= 0) & (u + v <= 1)
    return mask


def measure(path):
    # Blender is already required to build these assets; use its image decoders
    # instead of adding a new dependency to the portable Python tests.
    try:
        import bpy  # noqa: F401
    except ImportError:
        command = ['blender', '-b', '-t', '3', '--factory-startup', '--python-exit-code', '1',
                   '--python', str(Path(__file__).resolve()), '--', str(Path(path).resolve())]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        line = next(line for line in result.stdout.splitlines() if line.startswith('S6B_METRICS '))
        return json.loads(line[len('S6B_METRICS '):])
    doc, binary = glb(Path(path))
    mat = doc['materials'][0]
    base = image_data(doc, binary, mat['pbrMetallicRoughness']['baseColorTexture'])
    base = np.where(base <= .04045, base / 12.92, ((base + .055) / 1.055) ** 2.4)
    ao = image_data(doc, binary, mat['occlusionTexture'])
    skin = doc['skins'][0]
    names = [doc['nodes'][i]['name'] for i in skin['joints']]
    limb_names = ('scapula', 'upper_arm', 'forearm', 'hand', 'fingers',
                  'thigh', 'shin', 'foot', 'toes', 'tail')
    core = np.array([name in ('hips', 'spine1', 'spine2', 'chest') or name.startswith(limb_names)
                     for name in names])
    faces = []
    tusk_lengths = []
    tusk_arcs = []
    for mesh in doc['meshes']:
        for prim in mesh['primitives']:
            attr = prim['attributes']
            uv = accessor(doc, binary, attr['TEXCOORD_0'])
            weights = accessor(doc, binary, attr['WEIGHTS_0'])
            joints = accessor(doc, binary, attr['JOINTS_0'])
            body = (core[joints] * weights).sum(axis=1) > .5
            tris = accessor(doc, binary, prim['indices']).reshape(-1, 3)
            faces.extend(uv[tris[np.all(body[tris], axis=1)]])
            # Ivory occupies its own tile. Equal longitudinal UV coordinates
            # identify exported rings for a centerline arc measurement.
            if 'elephant' in str(path):
                pos = accessor(doc, binary, attr['POSITION'])
                tusk = ((uv[:, 0] > 8/2048) & (uv[:, 0] < 520/2048) &
                        (uv[:, 1] > 1-1560/2048) & (uv[:, 1] < 1-1048/2048))
                for sign in (-1, 1):
                    points = pos[tusk & (pos[:, 0]*sign > 0)]
                    if len(points):
                        # Furthest separation is a useful lower bound for curved tusks.
                        length = max(np.linalg.norm(points-p, axis=1).max() for p in points)
                        tusk_lengths.append(float(length))
                        longitudinal = uv[tusk & (pos[:, 0]*sign > 0), 1]
                        centers = []
                        for coordinate in np.unique(np.round(longitudinal, 6))[::-1]:
                            ring = points[np.abs(longitudinal-coordinate) < 5e-7]
                            if len(ring) >= 8:
                                centers.append(np.unique(ring, axis=0).mean(axis=0))
                        if not centers:
                            # S6 assigned successive V values to vertices rather than
                            # rings. Recover its 16-sided sections for before/after review.
                            ordered = []
                            for coordinate in np.unique(np.round(longitudinal, 6))[::-1]:
                                ordered.append(points[np.argmin(np.abs(longitudinal-coordinate))])
                            if (len(ordered) - 2) % 16 == 0:
                                centers = np.array(ordered[:-2]).reshape(-1, 16, 3).mean(axis=1)
                        if len(centers) > 2:
                            tusk_arcs.append(float(np.linalg.norm(np.diff(centers, axis=0), axis=1).sum()))
    faces = np.array(faces)
    pixels = base[coverage(faces, base.shape[:2])]
    occ = ao[coverage(faces, ao.shape[:2]), 0]
    triangles = sum(doc['accessors'][primitive['indices']]['count'] // 3
                    for mesh in doc['meshes'] for primitive in mesh['primitives'])
    return dict(linear_rgb=pixels.mean(axis=0).tolist(),
                luminance=float((pixels @ [.2126, .7152, .0722]).mean()),
                ao_mean=float(occ.mean()), ao_p05=float(np.quantile(occ, .05)),
                tusk_chord_m=tusk_lengths, tusk_arc_m=tusk_arcs,
                triangles=triangles, bones=len(names), bytes=Path(path).stat().st_size)



if __name__ == '__main__':
    print('S6B_METRICS ' + json.dumps(measure(sys.argv[sys.argv.index('--') + 1])))
