# SPDX-License-Identifier: MIT
"""Greater flamingo, 1.4 m to the head top, the bird shoulderHeightM convention.

The muppe5 sculpt imports upright with an S-neck and a lifted foot, despite the
research brief describing a feeding pose. Its neck centreline is retained.
"""
import numpy as np

from zoogen import birdscan
from zoogen.geom import Spec


def paint(context):
    return birdscan.flamingo_paint(context)


def paint_juvenile(context):
    return birdscan.flamingo_paint(context, juvenile=True, scale=.86)


def make_spec(juvenile=False):
    scale = .86 if juvenile else 1.
    data, bones, rig = birdscan.instance('flamingo', juvenile=juvenile)
    meta = {'common_name': 'Greater flamingo', 'scientific_name': 'Phoenicopterus roseus',
            'shoulder_height_m': 1.4 * scale, 'body_length_m': 1.2 * scale,
            'roughness': .92 if juvenile else .89,
            'asset_license': 'CC-BY-4.0', 'source_author': 'muppe5',
            'source_title': 'Flamingo (descriptive; no title exposed by importer)',
            'source_url': 'https://sketchfab.com/3d-models/b363f8f4f7394ddb9d5b3a337f2f7fc7',
            'source_pose': 'Upright S-neck with one lifted foot',
            # The game's clip for a still activity (AnimalRenderer).
            'activity_clips': {'huddle': 'huddle', 'preen': 'preen',
                               'filter_feed': 'filter_feed'},
            'swim': {'clip': 'swim', 'depth_m': .72 * scale, 'speed': .45 * scale},
            'neck_centerline_m': float(scale * sum(np.linalg.norm(b-a)
                for a, b in zip(birdscan.FLAMINGO_NECK, birdscan.FLAMINGO_NECK[1:])))}
    if juvenile:
        meta.update({'variant_of': 'flamingo', 'variant': 'juvenile',
                     'relative_height': scale,
                     'age_shape': 'First-year juvenile: nearly grown, grey-pink plumage',
                     'sex': 'juvenile'})
    return Spec('flamingo_juvenile' if juvenile else 'flamingo', bones, [],
                paint=paint_juvenile if juvenile else paint, sculpt=data, target_tris=13500,
                texture_size=1024, rig=rig, clips=birdscan.flamingo_clips,
                key_all_locations=True, meta=meta)
