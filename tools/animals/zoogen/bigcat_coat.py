# SPDX-License-Identifier: MIT
"""Colour adaptations of the authored atlas, with strokes located on the animal."""
import math

import numpy as np

from . import noise as nz


def soften(values, radius):
    result = values.copy()
    for axis in (0, 1):
        result = sum(np.roll(result, shift, axis) for shift in range(-radius, radius + 1)) / (2 * radius + 1)
    return result


def tiger_stripes(ctx, p, head, white):
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    side = np.where(y >= 0, 1., -1.)
    noise = nz.value_noise(p, 19, seed=71) - .5
    strokes = np.zeros(x.shape)
    # Independent rooted strokes: irregular spacing, staggered ends, tapered forks and breaks.
    for index in range(17):
        jitter = math.sin(index * 13.71 + 2)
        center = -.75 + index * .105 + .035 * jitter + side * .024 * math.sin(index * 4.3)
        bottom = .38 + .26 * (.5 + .5 * np.sin(index * 5.17 + side * .9))
        top = .98 + .12 * math.sin(index * 2.2)
        t = np.clip((z - bottom) / (top - bottom), 0, 1)
        curve = center - .12 * (1 - t) * np.maximum(0, .5 - center)
        curve += .032 * np.sin(t * (4 + index % 3) + index * 1.7) + .006 * noise
        width = (.012 + .014 * (.5 + .5 * math.sin(index * 4.83))) * np.sqrt(t)
        stroke = nz.smoothstep(width + .003, width, np.abs(x - curve))
        stroke *= nz.smoothstep(bottom, bottom + .04, z)
        if index % 4 == 1:
            stroke *= 1 - .95 * np.exp(-((t - .48) / .07) ** 4)
        strokes = np.maximum(strokes, stroke)
        if index % 3 != 0:
            separation = .05 * np.sin(np.pi * t) ** 1.2
            branch = nz.smoothstep(width * .5 + .003, width * .5, np.abs(x - curve - separation))
            strokes = np.maximum(strokes, branch * nz.smoothstep(.08, .25, t))
    trunk = ctx.bones('hips', 'spine1', 'spine2', 'chest', 'neck1', 'neck2')
    strokes *= trunk * (1 - white) * nz.smoothstep(.05, .5, np.abs(ctx.N[..., 1]))
    legs = ctx.bones('upper_arm', 'forearm', 'hand', 'fingers', 'thigh', 'shin', 'foot', 'toes')
    bands = np.zeros(x.shape)
    for index in range(9):
        level = .04 + .055 * index + .003 * index * index + .009 * math.sin(index * 5.3)
        curve = level + .023 * np.sin(x * 15 + side * index) + .010 * noise
        width = .004 + .009 * level
        ring = nz.smoothstep(width + .004, width, np.abs(z - curve))
        half = nz.smoothstep(-.25, .3, -ctx.N[..., 0])
        front_lower = ctx.bones('forearm', 'hand', 'fingers') * nz.smoothstep(.50, .45, z)
        side_ring = .35 + .65 * nz.smoothstep(-.4, .6, np.abs(ctx.N[..., 1]) - .2 * ctx.N[..., 0])
        bands = np.maximum(bands, ring * side_ring * (1 - front_lower * (1 - half)))
    strokes = np.maximum(strokes, bands * legs)
    face = ctx.bones('head', 'jaw')
    facial = np.zeros(x.shape)
    for index in range(4):
        level = head[2] + .065 + .031 * index
        curve = level - (.12 + .045 * index) * np.abs(y)
        curve += (.006 + .003 * (index % 2)) * np.cos(np.abs(y) * (31 + 7 * index))
        short = nz.smoothstep(.13 - index * .015, .07 - index * .010, np.abs(y))
        facial = np.maximum(facial, nz.smoothstep(.006, .003, np.abs(z - curve)) * short)
    cheeks = np.zeros(x.shape)
    for index in range(3):
        curve = head[2] - .025 - index * .035 + .30 * (x - head[0])
        curve += .013 * np.sin((x - head[0]) * 25 + index * 2)
        line = nz.smoothstep(.009, .004, np.abs(z - curve))
        line *= nz.smoothstep(head[0] + .13, head[0] + .03, x)
        cheeks = np.maximum(cheeks, line)
    strokes = strokes * (1 - face) + np.maximum(facial, cheeks) * face
    tail = ctx.bones('tail')
    rings = nz.smoothstep(.47, .30, np.abs(np.sin(x * 38 + z * 9)))
    strokes = strokes * (1 - tail) + rings * tail
    return np.clip(strokes, 0, 1)


def coat(ctx, source, data):
    kind, height = data['kind'], data['height']
    p = ctx.P / height
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    names = data['bone_names'].tolist()
    head = data['heads'][names.index('head')] / height
    face = ctx.bones('head', 'jaw')
    # Preserve the source's regional colour variation, freckles, eyelids and fur detail.
    result = np.clip(source * [1.42, 1.35, 1.22], 0, 1)
    if kind == 'male':
        result *= [.91, .88, .84]
    if kind == 'juvenile':
        result = np.clip(result * 1.10 + [.018, .017, .014], 0, 1)
        spots = np.zeros(x.shape)
        for index in range(76):
            cx = -.7 + 2 * ((math.sin(index * 12.989 + 4) * 437.51) % 1)
            cz = .13 + 1.5 * ((math.sin(index * 7.271 + 8) * 213.13) % 1)
            radius = .020 + .010 * ((index * .618) % 1)
            dx, dz = (x - cx) / radius, (z - cz) / radius
            angle = np.arctan2(dz, dx)
            distance = np.sqrt(dx * dx + dz * dz) + .12 * np.sin(angle * 3 + index)
            spots = np.maximum(spots, np.exp(-((distance - .8) / .28) ** 2)
                               * (.45 + .55 * np.sin(angle * 2 + index) ** 2))
        spot_region = np.maximum(nz.smoothstep(.1, .7, np.abs(ctx.N[..., 1])),
                                 face * nz.smoothstep(.2, .7, ctx.N[..., 2]))
        result *= (1 - .18 * spots * spot_region)[..., None]
    if kind == 'tiger':
        luminance = source @ np.array([.2126, .7152, .0722])
        detail = np.clip(luminance / .30, .22, 1.8)
        orange = np.array([.68, .30, .065]) * detail[..., None]
        under = nz.smoothstep(.15, -.5, ctx.N[..., 2]) * nz.smoothstep(.85, .62, z)
        inner = nz.smoothstep(.15, .65, -ctx.N[..., 1] * np.sign(y)) * ctx.bones(
            'upper_arm', 'forearm', 'thigh', 'shin')
        chest = ctx.bones('chest', 'neck1', 'neck2') * nz.smoothstep(.85, .60, z)
        chin = face * nz.smoothstep(head[2] - .055, head[2] - .13, z)
        muzzle = face * nz.smoothstep(head[0] + .12, head[0] + .22, x)
        muzzle *= nz.smoothstep(head[2] - .055, head[2] - .105, z)
        eye = data['vertices'][(data['tags'][:, 1] > .5) & (data['vertices'][:, 1] > 0)].mean(axis=0) / height
        eyebrow = np.exp(-((x - eye[0]) / .038) ** 4 - ((np.abs(y) - eye[1]) / .035) ** 4
                         - ((z - eye[2] - .024) / .019) ** 4)
        cheek = face * nz.smoothstep(.08, .13, np.abs(y))
        cheek *= nz.smoothstep(head[2] + .02, head[2] - .04, z)
        white = np.clip(under + inner + chest + np.maximum(chin, muzzle) + cheek + eyebrow, 0, 1)
        pale = np.clip(.61 + .27 * detail[..., None], 0, .95)
        result = nz.mix(orange, pale, white)
        stripe = tiger_stripes(ctx, p, head, white)
        result = nz.mix(result, [.014, .011, .009], stripe)
        # The source's dark nose and eyelid markings survive remapping instead of being painted over.
        dark = nz.smoothstep(.075, .025, source.max(axis=-1)) * face
        result = nz.mix(result, source * .7, dark)
        ears = ctx.bones('ear')
        back = nz.smoothstep(.0, -.45, ctx.N[..., 0])
        result = nz.mix(result, [.018, .013, .010], ears * back)
        ear = data['heads'][names.index('ear.L')] / height
        spot = np.exp(-((z - ear[2] - .027) / .022) ** 4
                      - ((np.abs(y) - abs(ear[1]) - .018) / .028) ** 4)
        result = nz.mix(result, [.88, .86, .79], ears * back * spot)
        result = nz.mix(result, [.014, .011, .009], nz.smoothstep(.7, 1, ctx.bones('tail4')))
    # Authored round pupils and amber iris contain much better detail than analytic circles.
    # Keep the prepared nose-pad boundary and its texture variation while darkening leather.
    pink = nz.smoothstep(1.2, 1.8, source[..., 0] / (source[..., 1] + .005))
    pink *= nz.smoothstep(1.0, 1.5, source[..., 2] / (source[..., 1] + .005))
    result = nz.mix(result, source * [.25, .35, .35], face * pink)
    special = np.clip(ctx.tag('eye') + ctx.tag('mouth') + ctx.tag('teeth') + ctx.tag('claws'), 0, 1)
    result = nz.mix(result, source, special)
    if kind == 'male':
        neck = data['heads'][names.index('neck1')] / height
        along = np.clip((x - neck[0]) / max(head[0] - neck[0], .01), 0, 1)
        center_z = neck[2] * (1 - along) + head[2] * along
        angle = np.arctan2(y, z - center_z)
        phase = angle * 93 + 4 * nz.value_noise(p, 9, seed=43) + 3 * x
        strands = .64 + .25 * np.sin(phase) ** 2 + .11 * np.sin(phase * 3.7) ** 2
        mane = nz.mix([.20, .10, .04], [.52, .32, .13], along ** 2)
        result = nz.mix(result, mane * strands[..., None], ctx.tag('mane'))
    result = nz.mix(result, [.055, .027, .012], ctx.tag('tuft'))
    return np.clip(result, 0, 1)
