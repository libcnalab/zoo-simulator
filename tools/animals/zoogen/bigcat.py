# SPDX-License-Identifier: MIT
"""Shared sculpt, coat and motion for the four big cats; no Blender dependency."""
import math
from functools import partial
from pathlib import Path

import numpy as np

from . import noise as nz
from . import quadruped as quad
from .geom import Part, R, Spec
from .rig import Leg, Rig
from .sculpt import prepare_instance

SOURCE = Path(__file__).resolve().parents[1] / 'sources/bigcat/sculpt.npz'


def cat_rig(data):
    legs = {}
    for side, suffix in (('L', '.L'), ('R', '.R')):
        legs['F' + side] = Leg('upper_arm' + suffix, 'forearm' + suffix, 'hand' + suffix,
                               'fingers' + suffix, 'scapula' + suffix)
        legs['H' + side] = Leg('thigh' + suffix, 'shin' + suffix, 'foot' + suffix, 'toes' + suffix)
    nose = data['vertices'][np.argmax(data['vertices'][:, 0])].copy()
    return Rig('hips', legs, ['spine1', 'spine2'], 'chest', ['neck1', 'neck2'], 'head', 'jaw',
               ['tail1', 'tail2', 'tail3', 'tail4'], ['ear.L', 'ear.R'],
               extra={'muzzle': ('head', nose)})


def hair_parts(data, kind):
    names = data['bone_names'].tolist()
    head = data['heads'][names.index('head')]
    neck = data['heads'][names.index('neck1')]
    height = data['height']
    parts = []
    if kind == 'male':
        chain = [(name, data['heads'][names.index(name)], data['tails'][names.index(name)])
                 for name in ('chest', 'neck1', 'neck2', 'head')]
        back = neck + height * np.array([-.20, 0, -.10])
        front = head + height * np.array([-.035, 0, -.025])
        widths = [.10, .19, .215, .215, .19, .145]
        dorsal = [.10, .17, .20, .215, .20, .14]
        ventral = [.15, .26, .30, .29, .25, .18]
        rings = [R(back * (1 - index / 5) + front * (index / 5),
                   widths[index] * height, dorsal[index] * height, ventral[index] * height)
                 for index in range(6)]
        parts.append(Part('mane_undercoat', rings, chain=chain, fuse=False, tag='mane',
                          segments=30, subdiv=2, cap_start='flat', cap_end='flat',
                          rough=(.010 * height, 85 / height)))
        axis = (front - back) / np.linalg.norm(front - back)
        vertical = np.cross(axis, [0, 1, 0])
        for layer in range(8):
            for index in range(34):
                # Stagger roots and lengths: aligned rows read as scales, not locks of hair.
                t = np.clip(.12 + .84 * layer / 7 + .065 * math.sin(index * 4.71 + layer), .05, .99)
                center = back * (1 - t) + front * t
                width = np.interp(t * 5, np.arange(6), widths) * height
                top = np.interp(t * 5, np.arange(6), dorsal) * height
                bottom = np.interp(t * 5, np.arange(6), ventral) * height
                angle = 2 * math.pi * (index + .37 * layer + .16 * math.sin(index * 4.2 + layer)) / 34
                side, up = math.cos(angle), math.sin(angle)
                radial = np.array([0, side * width, 0]) + vertical * up * (top if up > 0 else bottom)
                root = center + radial * .94
                length = height * (.095 + .035 * (1 - t) + .032 * math.sin(index * 7.7 + layer))
                direction = np.array([-.75, .13 * side, -.55 + .30 * up])
                middle = center + radial * 1.03 + length * .40 * direction
                end = root + length * direction
                lock_width = height * (.017 + .005 * math.sin(index * 3.4 + layer))
                parts.append(Part(f'mane_lock_{layer}_{index}',
                                  [R(root, lock_width, lock_width * .35),
                                   R(middle, lock_width * .7, lock_width * .3),
                                   R(end, .0007 * height, .0007 * height)],
                                  chain=chain, fuse=False, tag='mane', segments=4, subdiv=1,
                                  cap_start='flat', cap_end='flat'))
    if kind == 'tiger':
        for side in (-1, 1):
            for index in range(5):
                root = head + height * np.array([.015 - .014 * index, side * .14, -.065 + .019 * index])
                end = root + height * np.array([-.055, side * (.018 + .004 * (index % 3)), -.025])
                parts.append(Part(f'ruff_{side}_{index}',
                                  [R(root, .026 * height, .030 * height),
                                   R(end, .004 * height, .004 * height)],
                                  weights={'head': 1}, fuse=False, tag='ruff', segments=8, subdiv=1,
                                  cap_end='point'))
    if kind in ('male', 'female'):
        tip = data['tails'][names.index('tail4')]
        for index in range(7):
            angle = index * 2 * math.pi / 7
            offset = height * np.array([0, math.cos(angle) * .012, math.sin(angle) * .012])
            parts.append(Part(f'tuft_{index}',
                              [R(tip + offset + [.045 * height, 0, 0], .017 * height, .017 * height),
                               R(tip + offset, .024 * height, .025 * height),
                               R(tip + offset - [.055 * height, 0, .01 * height],
                                 .004 * height, .004 * height)],
                              weights={'tail4': 1}, fuse=False, tag='tuft', segments=8, subdiv=1,
                              cap_end='point'))
    return parts


def paint(ctx, data, kind):
    height = data['height']
    p = ctx.P / height
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    normal = ctx.N
    names = data['bone_names'].tolist()
    head = data['heads'][names.index('head')] / height
    face = ctx.bones('head', 'jaw')
    pale = np.array([.80, .73, .59]) if kind != 'tiger' else np.array([.88, .87, .79])
    tawny = np.array([.64, .46, .255]) if kind != 'tiger' else np.array([.58, .28, .07])
    under = nz.smoothstep(.2, -.65, normal[..., 2]) * nz.smoothstep(.94, .55, z)
    inner = nz.smoothstep(.15, .85, -normal[..., 1] * np.sign(y)) * ctx.bones(
        'upper_arm', 'forearm', 'thigh', 'shin')
    muzzle = nz.smoothstep(head[0] + .10, head[0] + .21, x) * face
    chin = face * nz.smoothstep(head[2] - .025, head[2] - .12, z)
    coat = nz.mix(tawny, pale, np.clip(.85 * under + .65 * inner + .75 * muzzle + .8 * chin, 0, 1))
    grain = nz.fbm(p * [1.5, 1, 1], 95, 2, seed=31)
    coat *= (.95 + .1 * grain)[..., None]
    if kind == 'tiger':
        warp = nz.fbm(p, 6, 2, seed=71)
        stripes = np.zeros(x.shape)
        # Individually tapered, forked strokes have independent lengths and widths.
        # Their spacing is deliberately irregular; a periodic wave reads as a barcode.
        for index in range(22):
            jitter = math.sin(index * 13.71)
            center = -.62 + index * .09 + .025 * jitter
            bottom = .30 + .32 * (.5 + .5 * math.sin(index * 5.17))
            taper = nz.smoothstep(bottom, bottom + .18, z)
            curve = center + .045 * np.sin(z * (6 + index % 3) + index * 1.3)
            curve += .05 * jitter * (1 - z)
            width = (.010 + .011 * (.5 + .5 * math.sin(index * 4.83))) * taper
            distance = np.abs(x - curve)
            stroke = nz.smoothstep(width + .0035, width, distance) * taper
            stripes = np.maximum(stripes, stroke)
            if index % 3 != 0:
                branch = curve + .036 * nz.smoothstep(bottom + .08, .95, z)
                width2 = width * .53
                fork = nz.smoothstep(width2 + .003, width2, np.abs(x - branch))
                stripes = np.maximum(stripes, fork * nz.smoothstep(bottom + .05, bottom + .16, z))
        legs = ctx.bones('forearm', 'hand', 'fingers', 'shin', 'foot', 'toes')
        leg_phase = z * 39 + x * 7 + 2 * warp
        leg_bands = nz.smoothstep(.43, .31, np.abs(np.sin(leg_phase)))
        stripes = stripes * (1 - legs) + leg_bands * legs
        forehead = np.abs(np.sin((z - head[2]) * 65 - np.abs(y) * 19 + .6 * np.cos(np.abs(y) * 35)))
        facial = nz.smoothstep(.32, .19, forehead) * (1 - muzzle)
        stripes = stripes * (1 - face) + facial * face
        cheek = face * nz.smoothstep(head[2] + .01, head[2] - .08, z)
        coat = nz.mix(coat, pale, cheek * .85)
        tail = ctx.bones('tail')
        rings = nz.smoothstep(.55, .40, np.abs(np.sin(x * 38 + z * 9)))
        stripes = stripes * (1 - tail) + rings * tail
        coat = nz.mix(coat, [.024, .019, .013], stripes * .96)
        coat = nz.mix(coat, [.024, .019, .013], nz.smoothstep(.65, 1, ctx.bones('tail4')))
        ruff_color = nz.mix(tawny, pale, nz.smoothstep(head[2] + .07, head[2] - .06, z))
        coat = nz.mix(coat, ruff_color, ctx.tag('ruff') * .95)
    elif kind == 'juvenile':
        spots = np.zeros(x.shape)
        for index in range(43):
            a = (math.sin(index * 12.989 + 4) * 437.51) % 1
            b = (math.sin(index * 7.271 + 8) * 213.13) % 1
            cx = -.60 + 1.38 * a
            cz = .19 + .68 * b
            rx = .022 + .014 * ((index * .618) % 1)
            dx, dz = (x - cx) / rx, (z - cz) / (rx * .75)
            angle = np.arctan2(dz, dx)
            radius = np.sqrt(dx * dx + dz * dz) + .12 * np.sin(angle * 3 + index)
            broken = .55 + .45 * np.sin(angle * 2 + index * 3.1) ** 2
            spots = np.maximum(spots, np.exp(-((radius - .8) / .24) ** 2) * broken)
        flank = nz.smoothstep(.2, .65, np.abs(normal[..., 1])) * (1 - face)
        coat *= (1 - .09 * spots * flank)[..., None]
    # Ears: the back is black; tiger's conspicuous ocellus stays within that dark field.
    ears = ctx.bones('ear')
    back = nz.smoothstep(.0, -.5, normal[..., 0])
    coat = nz.mix(coat, [.045, .029, .018], ears * back)
    if kind == 'tiger':
        ear_head = data['heads'][names.index('ear.L')] / height
        spot = np.exp(-((z - ear_head[2] - .027) / .021) ** 4
                      - ((np.abs(y) - abs(ear_head[1]) - .018) / .025) ** 4)
        coat = nz.mix(coat, pale, ears * back * spot)
    # Fine dark whisker follicles, surrounded by pale whisker pads.
    dots = np.zeros(x.shape)
    for row in range(3):
        for column in range(4):
            cx = head[0] + .14 + column * .021
            cz = head[2] - .045 - row * .013 - column * .002
            dots = np.maximum(dots, np.exp(-((x - cx) / .0025) ** 2 - ((z - cz) / .0025) ** 2))
    coat = nz.mix(coat, [.10, .065, .035], dots * face * .48)
    nose = ctx.tag('nose') * nz.smoothstep(head[0] + .20, head[0] + .25, x)
    if kind == 'tiger':
        nose = ctx.tag('nose')
    coat = nz.mix(coat, [.028, .018, .016], nose)
    coat = nz.mix(coat, [.035, .018, .017], ctx.tag('lips'))
    coat = nz.mix(coat, [.23, .055, .065], ctx.tag('mouth'))
    coat = nz.mix(coat, [.82, .76, .59], ctx.tag('teeth'))
    coat = nz.mix(coat, [.23, .18, .12], ctx.tag('claws'))
    eye_vertices = data['vertices'][data['tags'][:, 1] > .5] / height
    eye_center = eye_vertices[eye_vertices[:, 1] > 0].mean(axis=0)
    dy = np.abs(y) - eye_center[1]
    dz = z - eye_center[2]
    eye_horizontal = -.6 * (x - eye_center[0]) + .8 * dy
    radius = np.sqrt((eye_horizontal / .012) ** 2 + (dz / .012) ** 2)
    iris = nz.mix([.39, .21, .035], [.018, .013, .008], nz.smoothstep(.66, .95, radius))
    iris = nz.mix([.006, .005, .004], iris, nz.smoothstep(.27, .38, radius))
    glint = np.exp(-((dy + .0035) / .0018) ** 2 - ((dz - .005) / .002) ** 2)
    iris = nz.mix(iris, [.95, .93, .83], glint)
    if kind == 'tiger':
        surround = nz.smoothstep(2.3, 1.15, radius) * face
        surround *= nz.smoothstep(eye_center[2] - .005, eye_center[2] + .015, z)
        coat = nz.mix(coat, pale, surround * .65)
    rim = np.exp(-((radius - 1.05) / .22) ** 2) * face
    coat = nz.mix(coat, [.075, .043, .02], rim * .8)
    coat = nz.mix(coat, iris, ctx.tag('eye'))
    if kind == 'male':
        strand = nz.fbm(p * [2, 22, 14], 6, 2, seed=9)
        fringe = nz.smoothstep(head[0] - .32, head[0] - .02, x)
        mane = nz.mix([.14, .075, .029], [.48, .29, .13], fringe * .8 + strand * .2)
        neck = data['heads'][names.index('neck1')] / height
        along = np.clip((x - neck[0]) / max(head[0] - neck[0], .01), 0, 1)
        center_z = neck[2] * (1 - along) + head[2] * along - .04
        angle = np.arctan2(y, (z - center_z) / 1.2)
        phase = angle * 155 + 6 * nz.value_noise(p, 8, seed=43) + 3 * x
        fibres = .76 + .18 * np.sin(phase) + .06 * np.sin(angle * 710 + 5 * x)
        mane *= (fibres * (.82 + .35 * strand))[..., None]
        coat = nz.mix(coat, mane, ctx.tag('mane'))
    coat = nz.mix(coat, [.055, .033, .018], ctx.tag('tuft'))
    return coat


def _gait_trial(solver, gait, phase, drop):
    return quad.gait_pose(solver, gait, phase, drop)


def _lower_idle(original, height, phase):
    return original(phase).move("hips", dz=-.008 * height)


def clips(solver, height, walk_speed, trot_speed, kind, lie_drop):
    walk = quad.Gait('walk', period=.8, stance=walk_speed * .8 * .64, duty=.64, offsets=quad.WALK,
                     lift=.045 * height, bob=.006 * height, hip_yaw=1.8, chest_yaw=1.5,
                     sway=.003 * height, roll=.4, heel_off=10, swing_pitch=24,
                     land_pitch=0, toe_follow=.25, scapula=.55, head_nod=.8, tail_sway=2.2)
    trot = quad.Gait('trot', period=.5, stance=trot_speed * .5 * .42, duty=.42, offsets=quad.TROT,
                     lift=.075 * height, bob=-.008 * height, hip_yaw=1.2, chest_yaw=1,
                     sway=.001 * height, roll=.2, heel_off=15, swing_pitch=30,
                     land_pitch=0, toe_follow=.3, scapula=.55, head_nod=.6, tail_sway=1.5)
    if kind == 'juvenile':
        walk.period = .5
        walk.stance = walk_speed * walk.period * walk.duty
        trot.period = .30
        trot.stance = trot_speed * trot.period * trot.duty
    for gait in (walk, trot):
        gait.drop = quad.fit_drop(solver, partial(_gait_trial, solver, gait),
                                  gait.reach_limit, most=.25 * height)
    params = quad.Params(
        gaits=[walk, trot], lie=quad.Lie(drop=lie_drop, hind_tuck=.06 * height,
                                       hind_out=.09 * height, front_out=.02 * height,
                                       tail_floor=.05 * height, tail_curl=18,
                                       front_elbow_out=.6,
                                       foot_lift=(.03 if kind == 'tiger' else .017) * height),
        head_down=quad.HeadDown(body_pitch=12, crouch=.15 * height, chest=8, neck=(45, 35), head=12),
        eat_height=.07 * height, drink_height=.04 * height, idle_bob=.0015 * height,
        idle_shift=.002 * height, idle_roll=.25, look_yaw=9, tail_idle=2.5, ear_flick=10,
        sleep_neck=(32, 32), sleep_head=14, sleep_yaw=4, sleep_chin=.085 * height,
        rest_bob=.001 * height, sleep_bob=.0015 * height,
        call_neck=5, call_head=6, call_open=25 if kind != 'juvenile' else 16)
    result = quad.standard_clips(solver, params)
    idle = result[0]
    idle.pose = partial(_lower_idle, idle.pose, height)
    call = result[-1]
    call.pose = partial(_roar, call.pose, height)
    return result


def _roar(original, height, phase):
    pose = original(phase)
    breath = math.sin(phase * math.pi * 14) * quad.envelope(phase, .22, .72)
    pose.move("hips", dz=-.008 * height * quad.envelope(phase, .22, .72))
    pose.r("spine1", ry=.7 * breath)
    pose.r("spine2", ry=-.7 * breath)
    return pose


def make_cat(name, kind, height, edits, walk_speed, trot_speed, meta=None):
    data = prepare_instance(SOURCE, edits, height)
    data['height'] = height
    eye_scale = edits.get('head', {}).get('eye_scale', 1.0)
    if eye_scale != 1.0:
        eye_vertices = data['vertices'][data['tags'][:, 1] > .5]
        for side in (-1, 1):
            center = eye_vertices[eye_vertices[:, 1] * side > 0].mean(axis=0)
            delta = data['vertices'] - center
            influence = np.exp(-(np.linalg.norm(delta, axis=1) / (.037 * height)) ** 4)
            data['vertices'] += delta * ((eye_scale - 1) * influence[:, None])
    bone_names = data['bone_names'].tolist()
    trunk = data['weights'][:, [bone_names.index(name) for name in
                               ('hips', 'spine1', 'spine2', 'chest')]].sum(axis=1) > .8
    lie_drop = float(data['vertices'][trunk, 2].min()) - .008 * height
    metadata = dict(common_name='Bengal tiger' if kind == 'tiger' else 'African lion',
                    scientific_name='Panthera tigris tigris' if kind == 'tiger' else 'Panthera leo',
                    shoulder_height_m=height, asset_license='CC-BY-4.0',
                    source_title='Free strong lioness - 4k', source_author='Pedro B. Goulart (Pebegou)',
                    source_url=('https://sketchfab.com/3d-models/'
                                'free-strong-lioness-4k-84d56e6cfcf247cb8fe468312f371677'),
                    proportion_edits=edits, roughness=.86)
    metadata.update(meta or {})
    return Spec(name, data['bones'], hair_parts(data, kind), paint=partial(paint, data=data, kind=kind),
                texture_size=2048, target_tris=23500 if kind == 'juvenile' else 34000,
                meta=metadata, rig=cat_rig(data),
                clips=partial(clips, height=height, walk_speed=walk_speed, trot_speed=trot_speed,
                              kind=kind, lie_drop=lie_drop),
                key_all_locations=True, sculpt=data)
