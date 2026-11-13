# SPDX-License-Identifier: MIT
"""Plains zebra from the intact half of Shikaki_Naito's static anatomy study."""
from functools import partial
from zoogen.scan import coat_variation, equid_clips, make_scan_spec

# Animal-space metres after source orientation/grounding. Heads are anatomical
# centres, not the skin surface: withers 1.35, elbow .87, carpus .49,
# distal fetlock/hoof pivot .10, hoof .055; hip 1.10, stifle .79, hock .43.
LANDMARKS = [
    ('hips', 'root', (-.57, 0, 1.10), (-.34, 0, 1.14)),
    ('spine1', 'hips', (-.34, 0, 1.14), (-.06, 0, 1.17)),
    ('spine2', 'spine1', (-.06, 0, 1.17), (.25, 0, 1.20)),
    ('chest', 'spine2', (.25, 0, 1.20), (.28, 0, 1.16)),
    ('neck1', 'chest', (.28, 0, 1.16), (.70, 0, 1.32)),
    ('neck2', 'neck1', (.70, 0, 1.32), (.95, 0, 1.36)),
    ('head', 'neck2', (.95, 0, 1.36), (1.35, 0, .91)),
    ('jaw', 'head', (1.04, 0, 1.18), (1.34, 0, .88)),
    ('ear.L', 'head', (1.04, .11, 1.33), (1.16, .22, 1.44)),
    ('scapula.L', 'chest', (.30, .19, 1.25), (.46, .23, 1.02)),
    ('upper_arm.L', 'scapula.L', (.46, .23, 1.02), (.31, .25, .87)),
    ('forearm.L', 'upper_arm.L', (.31, .25, .87), (.40, .25, .49)),
    ('hand.L', 'forearm.L', (.40, .25, .49), (.43, .25, .10)),
    ('fingers.L', 'hand.L', (.43, .25, .10), (.51, .25, .055)),
    ('thigh.L', 'hips', (-.57, .23, 1.10), (-.37, .26, .79)),
    ('shin.L', 'thigh.L', (-.37, .26, .79), (-.65, .26, .43)),
    ('foot.L', 'shin.L', (-.65, .26, .43), (-.61, .26, .10)),
    ('toes.L', 'foot.L', (-.61, .26, .10), (-.51, .26, .055)),
    ('tail1', 'hips', (-.78, 0, 1.20), (-.85, 0, 1.05)),
    ('tail2', 'tail1', (-.85, 0, 1.05), (-.87, 0, .95)),
    ('tail3', 'tail2', (-.87, 0, .95), (-.89, 0, .85)),
    ('tail4', 'tail3', (-.89, 0, .85), (-.90, 0, .76)),
]

# The importer applies this proper rotation, then the sagittal translation and
# scale. The retained source flank is negative Y; no downloaded code is loaded.
PREPARATION = {'rotation': [[0, -1, 0], [1, 0, 0], [0, 0, 1]],
               'origin': [.10, -1.33776, -.00804843], 'scale': .965,
               'mouth': {'front': 1.16, 'slope': .65, 'level': 1.75},
               'ridge': {'start': .32, 'end': 1.05, 'remove_end': 1.45,
                         'base_z': 1.47, 'slope': -.13, 'half_width': .09, 'height': .075},
               'surface_edits': [{'min': [.25, -.075, 1.40], 'max': [2, .075, 2],
                                  'anchor': [0, 0, 1.40], 'scale': [1, 1, .65]},
                                 {'min': [1.10, -.09, 1.38], 'max': [2, .09, 2],
                                  'anchor': [1.10, 0, 1.38], 'scale': [.35, 1, .35]}],
               'keep_side': -1, 'triangles': 28500, 'neutral_rotations': {'neck1': [0, -20, 0],
                                      'ear.L': [30, -25, 0], 'ear.R': [-30, -25, 0]}}



def make_variant(name, height, variant, edits):
    meta = {'common_name': 'Plains zebra', 'scientific_name': 'Equus quagga', 'asset_license': 'CC BY 4.0',
            'source': 'Shikaki_Naito, Zebra Anatomy, CC BY 4.0', 'variant': variant}
    if variant != 'male':
        meta.update(variant_of='zebra', relative_height=height / 1.35)
    edits = {'ear': {'scale': [1.1, 1.1, 1.3]}, **edits}
    return make_scan_spec({'name': name, 'source': 'zebra', 'height': height,
                           'meta': meta, 'edits': edits, 'clip_factory': equid_clips,
                           'target_tris': 19500 if variant == 'juvenile' else 29000,
                           'fur_length': .002 if variant == 'juvenile' else 0,
                           'stripe_variation': {'male': .0018, 'female': .0032, 'juvenile': .0026}[variant],
                           'vary_both_sides': variant != 'male',
                           'texture_transform': partial(coat_variation, variant=variant)})


def make_spec():
    return make_variant('zebra', 1.35, 'male', {})
