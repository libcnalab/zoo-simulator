# SPDX-License-Identifier: MIT
"""Steppe equids derived from the prepared CC BY equid scan, with new projected coats."""
import numpy as np

from .scan import make_scan_spec


def make_equid(name, common, scientific, height, edits, chestnut):
    spec = make_scan_spec({
        'name': name, 'source': 'zebra', 'height': height, 'edits': edits,
        'meta': {'common_name': common, 'scientific_name': scientific,
                 'asset_license': 'CC BY 4.0', 'source': 'Shikaki_Naito, Zebra Anatomy, CC BY 4.0',
                 'activity_clips': {'sentinel': 'sentry'}}})
    data = spec.sculpt
    # Project the coat from the side. Paint both flanks in animal coordinates so the
    # ventral white, dark stockings and muzzle follow the animal after proportion edits.
    points = data['vertices'] / height
    lower = points.min(axis=0) - .01
    span = points.max(axis=0) + .01 - lower
    coords = ((points - lower) / span)[:, [0, 2]]
    data['uv'] = coords[data['triangles']]
    head = data['heads'][data['bone_names'].tolist().index('head')] / height
    nose = data['tails'][data['bone_names'].tolist().index('head')] / height

    def paint(rgba):
        rows, columns = rgba.shape[:2]
        x = lower[0] + span[0] * (np.arange(columns)[None, :] + .5) / columns
        z = lower[2] + span[2] * (np.arange(rows)[:, None] + .5) / rows
        coat = np.array([.35, .125, .052] if chestnut else [.51, .30, .125])
        pale = np.array([.84, .74, .57])
        black = np.array([.042, .030, .024])

        def mix(a, b, weight):
            return a + np.clip(weight, 0, 1)[..., None] * (b - a)

        belly = .65 + .045 * np.cos(5 * x)
        rgb = mix(coat, pale, (belly - z) * (22 if chestnut else 9))
        rgb = mix(rgb, black, (.095 - z) * 42 if chestnut else (.40 - z) * 7)
        # Dark upright crest, tail brush, mealy muzzle, nostrils and eyes.
        crest = np.interp(x, [-.65, .14, .42, head[0], head[0] + .025],
                          [1.00, .99, 1.09, head[2] + .065, 1.5])
        rgb = mix(rgb, black, (z - crest) * 60)
        rgb = mix(rgb, black, (-.555 - x) * 40 * np.clip((.84 - z) * 25, 0, 1))
        rgb = mix(rgb, pale, (x - nose[0] + .13) * 11)
        for cx, cz, rx, rz in [(nose[0] - .025, nose[2] + .025, .020, .012),
                                (head[0] + .12, head[2] + .005, .021, .015)]:
            d = ((x - cx) / rx) ** 2 + ((z - cz) / rz) ** 2
            rgb = mix(rgb, black, (1.25 - d) * 4)
        grain = np.random.default_rng(73).normal(1, .028, (rows, columns, 1))
        rgba[..., :3] = np.clip(rgb * grain, 0, 1)
        return rgba

    data['texture_transform'] = paint
    return spec
