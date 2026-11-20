# SPDX-License-Identifier: MIT
"""Shared sculpt, coat and motion for the four big cats; no Blender dependency."""
import math
from functools import partial
from pathlib import Path

import numpy as np

from . import activity, quadruped as quad
from .geom import Part, R, Spec
from .rig import Clip, Leg, Pose, Rig
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
               extra={'muzzle': ('head', nose), 'skin_vertices': data['vertices'],
                      'skin_weights': data['weights'], 'skin_bones': data['bone_names']})


def hair_parts(data, kind):
    names = data['bone_names'].tolist()
    height = data['height']
    parts = []
    if kind == 'male':
        chain = [(name, data['heads'][names.index(name)], data['tails'][names.index(name)])
                 for name in ('chest', 'neck1', 'neck2', 'head')]
        from .bigcat_hair import mane_sections
        centers, widths, dorsal, ventral = mane_sections(data)
        rings = [R(center, width, top, bottom)
                 for center, width, top, bottom in zip(centers, widths, dorsal, ventral)]
        parts.append(Part('mane_undercoat', rings, chain=chain, fuse=False, tag='mane',
                          segments=30, subdiv=2, cap_start='flat', cap_end='flat',
                          rough=(.010 * height, 85 / height), radius_scale=1.0))
    if kind in ('male', 'female'):
        tip = data['tails'][names.index('tail4')]
        for index in range(1):
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
        sleep_neck=(32, 32), sleep_head=14, sleep_yaw=4,
        sleep_chin=(.045 if kind == 'juvenile' else .035) * height,
        rest_bob=.001 * height, sleep_bob=.0015 * height,
        call_neck=5, call_head=6, call_open=25 if kind != 'juvenile' else 16)
    result = quad.standard_clips(solver, params)
    idle = result[0]
    idle.pose = partial(_lower_idle, idle.pose, height)
    call = result[-1]
    call.pose = partial(_roar, call.pose, height)
    if kind == 'tiger':
        rest = next(clip for clip in result if clip.name == 'rest')
        result.append(Clip('bathe', 8, partial(_bathe, solver, rest, height)))
        swim = Clip('swim', 1.2, partial(_swim, solver, height), speed=.9 * height)
        swim.contact_keys = []
        result.append(swim)
    return result


def _roar(original, height, phase):
    pose = original(phase)
    breath = math.sin(phase * math.pi * 14) * quad.envelope(phase, .22, .72)
    pose.move("hips", dz=-.008 * height * quad.envelope(phase, .22, .72))
    pose.r("spine1", ry=.7 * breath)
    pose.r("spine2", ry=-.7 * breath)
    return pose


def _bathe(solver, rest, height, phase):
    """The tiger lies in the shallows, rolls its chest and bats water with alternating paws."""
    angle = math.tau * phase
    pose = activity.copy_pose(rest.pose(phase))
    roll = 6 * math.sin(angle)
    pose.r('hips', rx=roll).r('chest', rx=-.7 * roll)
    pose.r('head', rx=3 * math.sin(2 * angle + .4), rz=2 * math.sin(3 * angle))
    for key, offset in (('FL', 0), ('FR', .5)):
        stroke = math.sin(math.tau * (2 * phase + offset))
        pose.feet[key].pos += height * np.array([.06 * stroke, 0, .025 * max(0, stroke)])
    from .scan import skin_floor
    lift = .003 * height - skin_floor(solver, pose)
    if lift > 0:
        pose.move(solver.rig.body, dz=lift)
        for target in pose.feet.values():
            target.pos[2] += lift
    return pose


def _swim(solver, height, phase):
    """A level tiger paddle with alternating forepaw pulls and a trailing tail."""
    angle = math.tau * phase
    pose = Pose().move('hips', dz=-.015 * height)
    pose.r('hips', rx=1.2 * math.sin(angle))
    pose.r('spine1', rx=-.8 * math.sin(angle))
    pose.r('neck1', ry=-7).r('neck2', ry=-4).r('head', ry=-4)
    for key in solver.rig.legs:
        foot = solver.toe_base(key)
        side = 1 if key.endswith('L') else -1
        stroke = angle + (0 if side > 0 else math.pi)
        if key.startswith('F'):
            foot += height * np.array([.18 * math.sin(stroke), side * .03 * math.cos(stroke),
                                      .25 + .08 * math.cos(stroke)])
            pose.plant(key, foot, foot_pitch=-12 + 18 * math.sin(stroke), toe_pitch=-8)
        else:
            foot += height * np.array([-.13, side * .025 * math.sin(stroke), .17])
            pose.plant(key, foot, foot_pitch=-12, toe_pitch=-8)
    for index, tail in enumerate(solver.rig.tail):
        pose.r(tail, rz=(4 - .6 * index) * math.sin(angle - .45 * index))
    return pose


def make_cat(name, kind, height, edits, walk_speed, trot_speed, meta=None):
    data = prepare_instance(SOURCE, edits, height)
    data['height'] = height
    data['kind'] = kind
    data['source_material'] = True
    eye_scale = edits.get('head', {}).get('eye_scale', 1.0)
    if eye_scale != 1.0:
        eye_vertices = data['vertices'][data['tags'][:, 1] > .5]
        for side in (-1, 1):
            center = eye_vertices[eye_vertices[:, 1] * side > 0].mean(axis=0)
            delta = data['vertices'] - center
            influence = np.exp(-(np.linalg.norm(delta, axis=1) / (.037 * height)) ** 4)
            data['vertices'] += delta * ((eye_scale - 1) * influence[:, None])
    muzzle_scale = edits.get('head', {}).get('muzzle_scale', 1.0)
    if muzzle_scale != 1.0:
        names = data['bone_names'].tolist()
        pivot = data['heads'][names.index('head'), 0] + .12 * height
        distance = data['vertices'][:, 0] - pivot
        ramp = (distance + np.sqrt(distance ** 2 + (.005 * height) ** 2)) / 2
        face = data['weights'][:, [names.index('head'), names.index('jaw')]].sum(axis=1)
        data['vertices'][:, 0] -= (1 - muzzle_scale) * ramp * face
    if kind == 'juvenile':
        from .bigcat_shape import soften_cub
        soften_cub(data)
    bone_names = data['bone_names'].tolist()
    trunk = data['weights'][:, [bone_names.index(name) for name in
                               ('hips', 'spine1', 'spine2', 'chest')]].sum(axis=1) > .8
    lie_drop = float(data['vertices'][trunk, 2].min())
    if kind != 'juvenile':
        lie_drop -= .008 * height
    metadata = dict(common_name='Bengal tiger' if kind == 'tiger' else 'African lion',
                    scientific_name='Panthera tigris tigris' if kind == 'tiger' else 'Panthera leo',
                    shoulder_height_m=height, asset_license='CC-BY-4.0',
                    source_title='Free strong lioness - 4k', source_author='Pedro B. Goulart (Pebegou)',
                    source_url=('https://sketchfab.com/3d-models/'
                                'free-strong-lioness-4k-84d56e6cfcf247cb8fe468312f371677'),
                    proportion_edits=edits, roughness=.84,
                    material_source='adapted authored base, normal and occlusion',
                    preparation_version=2)
    metadata.update(meta or {})
    if kind == 'male':
        metadata['mane_revision'] = 'Short narrow curved locks over opaque undercoat'
        metadata['mane_card_locks'] = 2199
    if kind == 'tiger':
        metadata['activity_clips'] = {'bathe': 'bathe'}
        metadata['swim'] = {'clip': 'swim', 'depth_m': .70 * height,
                            'speed_mps': .9 * height}
        metadata['water_origin_convention'] = 'Place the asset origin at water surface - depth_m'
        metadata['cheek_ruff_revision'] = 'Layered warm cream locks with irregular roots'
        metadata['cheek_ruff_cards'] = 150
    body_budget = 23000 if kind == 'juvenile' else 36000 if kind == 'male' else 33000
    return Spec(name, data['bones'], hair_parts(data, kind),
                texture_size=1024, target_tris=body_budget,
                meta=metadata, rig=cat_rig(data),
                clips=partial(clips, height=height, walk_speed=walk_speed, trot_speed=trot_speed,
                              kind=kind, lie_drop=lie_drop),
                key_all_locations=True, sculpt=data)
