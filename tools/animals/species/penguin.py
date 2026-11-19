# SPDX-License-Identifier: MIT
"""Adult African penguin, 0.65 m to the standing head top (shoulderHeightM).

The adult plumage is painted by us on a Magellanic specimen's shape. This is not
an African penguin scan. A compact numeric derivative supplies the original UVs,
feather luminance and corrected mounted-specimen geometry.
"""
from zoogen import biped, birdscan
from zoogen.geom import Spec


def paint(context):
    return birdscan.penguin_paint(context)


def make_spec(juvenile=False):
    data, bones, rig = birdscan.instance('penguin', juvenile)
    scale = rig.extra['scale']
    name = 'penguin_juvenile' if juvenile else 'penguin'
    eyes = biped.eye_part((.092 * scale, .047 * scale, .600 * scale), .0045 * scale)
    meta = {'common_name': 'African penguin', 'scientific_name': 'Spheniscus demersus',
            'shoulder_height_m': .65 * scale, 'body_length_m': .65 * scale, 'roughness': .88,
            'asset_license': 'CC-BY-4.0', 'source_title': 'Juvenile Magellanic Penguin',
            'source_author': 'RISD Nature Lab',
            'source_url': 'https://sketchfab.com/3d-models/juvenile-magellanic-penguin-bf69c563457a4bb184fb8af2bf2d2811',
            'source_species': 'Spheniscus magellanicus',
            'plumage': ('Source juvenile photo colour' if juvenile
                        else 'African adult painted on Magellanic specimen shape'),
            'spot_layout': 'juvenile_unspotted' if juvenile else 'adult_01',
            'swim': {'clip': 'swim', 'depth_m': .45 * scale, 'speed': 1.5 * scale},
            'float': {'clip': 'float', 'depth_m': .25 * scale, 'speed': .12 * scale},
            'water_origin_convention': 'Set asset origin to water surface minus depth_m; clip speed is metres/second',
            # The game's clip for a still activity (AnimalRenderer).
            'activity_clips': {'preen': 'preen'}}
    if juvenile:
        meta.update(variant_of='penguin', variant='juvenile', relative_height=scale)
    def coat(context):
        return birdscan.penguin_paint(context, juvenile=juvenile, scale=scale)
    return Spec(name, bones, [eyes], paint=coat, sculpt=data, target_tris=9800 if juvenile else 11900,
                texture_size=1024, rig=rig, clips=birdscan.penguin_clips, key_all_locations=True, meta=meta)
