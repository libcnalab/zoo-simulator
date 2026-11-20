# SPDX-License-Identifier: MIT
"""Bear-specific landmarks, proportion edits, fur and motion on the sculpt path."""
from copy import deepcopy
from functools import partial
from pathlib import Path
import math

import numpy as np

from . import activity, quadruped as quad
from .geom import Bone, Part, R, Spec
from .rig import Clip, Leg, Pose, Rig, Solver, rot_y, rot_z
from .scan import landmark_bones, segment_distance
from .sculpt import prepare_instance

SOURCE = Path(__file__).resolve().parents[1] / 'sources/bear/sculpt.npz'

# Measured inside the upright source, after orientation and grounding in metres.
LANDMARKS = [
    ('hips', 'root', (-.10, 0, .77), (-.08, 0, 1.00)),
    ('spine1', 'hips', (-.08, 0, 1.00), (-.07, 0, 1.27)),
    ('spine2', 'spine1', (-.07, 0, 1.27), (-.06, 0, 1.55)),
    ('chest', 'spine2', (-.06, 0, 1.55), (-.04, 0, 1.76)),
    ('neck1', 'chest', (-.04, 0, 1.76), (.00, 0, 1.94)),
    ('neck2', 'neck1', (.00, 0, 1.94), (.08, 0, 2.17)),
    ('head', 'neck2', (.08, 0, 2.17), (.57, 0, 2.25)),
    ('jaw', 'head', (.14, 0, 2.15), (.49, 0, 1.98)),
    ('ear.L', 'head', (.21, .18, 2.34), (.22, .21, 2.43)),
    ('scapula.L', 'chest', (-.07, .26, 1.75), (-.10, .38, 1.78)),
    ('upper_arm.L', 'scapula.L', (-.10, .38, 1.78), (.02, .82, 1.57)),
    ('forearm.L', 'upper_arm.L', (.02, .82, 1.57), (.12, 1.00, 1.36)),
    ('hand.L', 'forearm.L', (.12, 1.00, 1.36), (.23, 1.055, 1.22)),
    ('fingers.L', 'hand.L', (.23, 1.055, 1.22), (.25, 1.075, 1.13)),
    ('thigh.L', 'hips', (-.10, .26, .77), (-.02, .28, .46)),
    ('shin.L', 'thigh.L', (-.02, .28, .46), (-.11, .28, .13)),
    ('foot.L', 'shin.L', (-.11, .28, .13), (.035, .28, .065)),
    ('toes.L', 'foot.L', (.035, .28, .065), (.14, .28, .045)),
    ('tail1', 'hips', (-.31, 0, .75), (-.37, 0, .70)),
    ('tail2', 'tail1', (-.37, 0, .70), (-.385, 0, .65)),
    ('tail3', 'tail2', (-.385, 0, .65), (-.385, 0, .62)),
    ('tail4', 'tail3', (-.385, 0, .62), (-.38, 0, .60)),
]


def source_weights(points, bones, heat):
    """Restrict heat to upright anatomical regions, repairing disconnected teeth/claws."""
    x, y, z = points.T
    weights = np.zeros_like(heat)
    for column, bone in enumerate(bones):
        name = bone.name
        stem = name.split('.')[0]
        if name == 'root':
            continue
        side = 1 if name.endswith('.L') else -1
        allowed = np.ones(len(points), dtype=bool)
        width = .20
        if stem in ('scapula', 'upper_arm', 'forearm', 'hand', 'fingers'):
            allowed &= y * side > .33
            allowed &= z > 1.00
            if stem != 'scapula':
                allowed &= y * side > .39
        elif stem in ('thigh', 'shin', 'foot', 'toes'):
            allowed &= (y * side > .10) & (z < .89)
            width = .14
        elif stem in ('head', 'jaw'):
            allowed &= z > (1.72 if stem == 'jaw' else 1.80)
            width = .16
            if stem == 'jaw':
                allowed &= (x > .20) & (z < 2.14 - .12 * (x - .2))
                width = .12
        elif stem == 'ear':
            allowed &= (z > 2.33) & (y * side > .13)
            width = .07
        elif stem.startswith('tail'):
            allowed &= (x < -.29) & (z < .81) & (np.abs(y) < .12)
            width = .06
        elif stem.startswith('neck'):
            allowed &= (z > 1.65) & (np.abs(y) < .43)
        else:
            allowed &= (np.abs(y) < .49) & (z > .63) & (z < 1.96)
        distance = segment_distance(points, np.asarray(bone.head), np.asarray(bone.tail))
        prior = np.exp(-1.5 * (distance / width) ** 2)
        weights[:, column] = allowed * (.65 * heat[:, column] + .35 * prior)
    empty = weights.sum(axis=1) < 1e-20
    if np.any(empty):
        distance = np.stack([segment_distance(points[empty], np.asarray(b.head), np.asarray(b.tail))
                             if b.deform else np.full(empty.sum(), 1e6) for b in bones], axis=1)
        weights[np.flatnonzero(empty), distance.argmin(axis=1)] = 1
    weights /= weights.sum(axis=1, keepdims=True)
    # The roaring sculpt's external lower lip extends below the internal gums.
    # Include that skin in closure; neck-only weights would leave a permanent gape.
    lower_lip = np.clip((x - .25) / .15, 0, 1) * np.clip((2.15 - z) / .10, 0, 1)
    lower_lip *= (z > 1.72) & (np.abs(y) < .22)
    weights *= 1 - lower_lip[:, None]
    weights[:, [bone.name for bone in bones].index('jaw')] += lower_lip
    return weights


def base_rig():
    legs = {}
    for side in ('L', 'R'):
        suffix = '.' + side
        legs['F' + side] = Leg('upper_arm' + suffix, 'forearm' + suffix, 'hand' + suffix,
                               'fingers' + suffix, 'scapula' + suffix)
        legs['H' + side] = Leg('thigh' + suffix, 'shin' + suffix, 'foot' + suffix, 'toes' + suffix)
    return Rig('hips', legs, ['spine1', 'spine2'], 'chest', ['neck1', 'neck2'], 'head', 'jaw',
               ['tail1', 'tail2', 'tail3', 'tail4'], ['ear.L', 'ear.R'])


def neutralize(data):
    """Bake a supported quadruped stance from the unrigged upright roaring sculpt."""
    bones = landmark_bones(LANDMARKS)
    rest = {}
    for bone in bones:
        matrix = np.eye(4)
        matrix[:3, 3] = bone.head
        rest[bone.name] = matrix
    solver = Solver(bones, rest, base_rig())
    pose = Pose().move('hips', dx=-.43, dz=-.16).r('hips', ry=78)
    pose.r('neck1', ry=-8).r('neck2', ry=-22).r('head', ry=-48)
    pose.r('jaw', ry=-39).r('tail1', ry=-90)
    for side, sign in (('L', 1), ('R', -1)):
        pose.plant('H' + side, (-.57, sign * .28, .065), pole=(1, sign * .2, 0))
        pose.plant('F' + side, (.53, sign * .36, .08), pole=(-1, sign * .35, 0))
        # The source wrist points down/out. A fixed world rotation makes its long
        # digits project forwards while the new hind feet keep their entire soles.
        from .rig import rot_x
        pose.feet['F' + side].foot = rot_y(-52) @ rot_x(-sign * 6)
        pose.feet['F' + side].toes = rot_y(-77) @ rot_x(-sign * 6)
    world = solver.solve(pose)
    print('NEUTRAL IK', solver.ik_error, solver.extension, flush=True)
    return bake_pose(data, world, rest)


def bake_pose(data, world, rest):
    points = np.zeros_like(data['vertices'], dtype=np.float64)
    heads, tails = [], []
    for index, name in enumerate(data['bone_names']):
        transform = world[name] @ np.linalg.inv(rest[name])
        points += data['weights'][:, index, None] * (
            data['vertices'] @ transform[:3, :3].T + transform[:3, 3])
        heads.append(world[name][:3, 3])
        tails.append(transform[:3, :3] @ data['tails'][index] + transform[:3, 3])
    return dict(data, vertices=points.astype(np.float32), heads=np.array(heads, dtype=np.float32),
                tails=np.array(tails, dtype=np.float32))


def fur_parts(data, juvenile):
    """Short opaque clumps break the ruff, belly and rear-leg outlines."""
    vertices = data['vertices']
    faces = vertices[data['triangles']]
    normals = np.zeros_like(vertices)
    face_normals = np.cross(faces[:, 1] - faces[:, 0], faces[:, 2] - faces[:, 0])
    for corner in range(3):
        np.add.at(normals, data['triangles'][:, corner], face_normals)
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-10)
    names = data['bone_names'].tolist()
    height = data['height']
    p = vertices / height
    body = data['tags'][:, 0] > .8
    region = ((p[:, 2] > .28) & (p[:, 2] < .62)) | (
        (p[:, 0] > .22) & (p[:, 0] < .72) & (p[:, 2] > .60))
    choices = np.flatnonzero(body & region & (np.abs(normals[:, 1]) > .35))
    parts = []
    count = min(90, len(choices))
    for number in range(count):
        index = choices[(number * 811) % len(choices)]
        point, normal = vertices[index], normals[index]
        length = height * (.045 + .020 * (.5 + .5 * math.sin(number * 7.13)))
        if juvenile:
            length *= 1.2
        direction = normal * .40 + np.array([-.42, 0, -.70])
        root = point - .008 * height * normal
        end = point + direction * length
        middle = root * .45 + end * .55 + normal * .006 * height
        weight = {name: float(data['weights'][index, i]) for i, name in enumerate(names)
                  if data['weights'][index, i] > 0}
        parts.append(Part('fur_' + str(number),
                          [R(root, .007 * height, .002 * height),
                           R(middle, .004 * height, .0015 * height),
                           R(end, .0005 * height, .0005 * height)],
                          weights=weight, tag='fur', fuse=False, segments=4, subdiv=1,
                          cap_start='flat', cap_end='point'))
    # A small lower-lip volume closes the residual opening in the roaring
    # source's external skin. It follows the jaw when the bear vocalizes.
    jaw_tip = data['tails'][names.index('jaw')]
    lip = jaw_tip + height * np.array([.015, 0, -.035])
    parts.append(Part('lower_lip',
                      [R(lip + [-.10 * height, 0, 0], .045 * height, .022 * height, .025 * height),
                       R(lip, .055 * height, .025 * height, .025 * height),
                       R(lip + [.055 * height, 0, 0], .030 * height, .015 * height, .020 * height)],
                      weights={'jaw': 1}, tag='lower_lip', fuse=False, segments=12, subdiv=2))
    return parts


def paint(ctx, data, polar, juvenile):
    from . import noise as nz
    p = ctx.P / data['height']
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    grain = nz.fbm(p * [.35, 2.2, 2.0], 55, 2, seed=10)
    broad = nz.fbm(p, 3.7, 3, seed=23)
    face = ctx.bones('head', 'jaw')
    hump = np.exp(-((x - .25) / .30) ** 2) * nz.smoothstep(.7, 1.0, z)
    muzzle = face * nz.smoothstep(.84, 1.15, x)
    if polar:
        base = np.array([.76, .72, .61]) if not juvenile else np.array([.84, .81, .73])
        coat = np.broadcast_to(base, (*x.shape, 3)).copy()
        coat *= (.90 + .13 * broad + .10 * grain)[..., None]
        coat = nz.mix(coat, base * .78, nz.smoothstep(.35, .10, z) * .35)
    else:
        base = np.array([.235, .125, .055])
        coat = nz.mix(base * .74, base * 1.3, broad)
        coat = nz.mix(coat, [.42, .28, .14], np.clip(hump * .55 + muzzle * .55, 0, 1))
        coat *= (.80 + .32 * grain)[..., None]
        if juvenile:
            collar = np.exp(-((x - .55) / .10) ** 2) * ctx.bones('neck1', 'neck2', 'chest')
            coat = nz.mix(coat, [.58, .46, .30], collar * .65)
    coat = nz.mix(coat, [.014, .011, .009], ctx.tag('nose'))
    coat = nz.mix(coat, [.10, .025, .028], ctx.tag('mouth'))
    coat = nz.mix(coat, [.68, .60, .42], ctx.tag('teeth'))
    coat = nz.mix(coat, [.018, .012, .006], ctx.tag('eye'))
    coat = nz.mix(coat, [.09, .065, .04], ctx.tag('claws'))
    pads = ctx.bones('hand', 'fingers', 'foot', 'toes') * nz.smoothstep(-.4, -.85, ctx.N[..., 2])
    if polar:
        patch = np.zeros(x.shape)
        names = data['bone_names'].tolist()
        for bone in ('hand.L', 'hand.R', 'foot.L', 'foot.R'):
            center = data['tails'][names.index(bone)] / data['height']
            patch = np.maximum(patch, np.exp(-((x - center[0]) / .085) ** 4
                                             - ((y - center[1]) / .065) ** 4))
        pads *= patch
    coat = nz.mix(coat, [.032, .024, .019], pads)
    coat = nz.mix(coat, [.055, .034, .024], ctx.tag('lower_lip') * nz.smoothstep(.35, .8, ctx.N[..., 2]))
    return coat


def make_bear(name, polar, juvenile, height, edits):
    data = prepare_instance(SOURCE, {'root': {'scale': [1, .78, 1]}, **edits}, height)
    names = data['bone_names'].tolist()
    vertices = data['vertices']
    # Broad smooth dorsal edits change actual anatomy, not just the pigment.
    x, y, z = (vertices / height).T
    dorsal = np.clip((z - .65) / .32, 0, 1) * np.exp(-(y / .36) ** 4)
    shoulder = np.exp(-((x - .26) / .27) ** 2)
    rump = np.exp(-((x + .48) / .29) ** 2)
    vertices[:, 2] += height * dorsal * ((-.13 * shoulder + .16 * rump) if polar else .07 * shoulder)
    neck_weight = data['weights'][:, [names.index(n) for n in ('neck1', 'neck2', 'head')]].sum(axis=1)
    vertices[:, 2] += height * .16 * neck_weight * np.clip((.77 - z) / .30, 0, 1)
    # Autumn condition fills the waist and hangs below the sculpted abdomen.
    trunk = data['weights'][:, [names.index(n) for n in ('hips', 'spine1', 'spine2', 'chest')]].sum(axis=1)
    vertices[:, 1] *= 1 + (.08 if polar else .18) * trunk
    belly = np.exp(-((x + .08) / .43) ** 2) * np.clip((.68 - z) / .25, 0, 1) * trunk
    vertices[:, 2] += (.045 if polar else .06) * height * belly
    # The longer polar muzzle is straight; brown has a shallow dish above the nose.
    face = data['weights'][:, names.index('head')]
    if polar:
        claws = data['tags'][:, data['tag_names'].tolist().index('claws')]
        vertices[:, 0] -= .035 * height * claws
    if not polar and not juvenile:
        vertices[:, 2] -= .025 * height * face * np.exp(-((x - 1.02) / .11) ** 2)
    floor = float(vertices[:, 2].min())
    vertices[:, 2] -= floor
    data['heads'][1:, 2] -= floor
    data['tails'][1:, 2] -= floor
    chest_x = data['heads'][names.index('chest'), 0]
    withers = (np.abs(vertices[:, 0] - chest_x) < .09 * height) & (np.abs(vertices[:, 1]) < .13 * height)
    factor = height / vertices[withers, 2].max()
    for key in ('vertices', 'heads', 'tails'):
        data[key] *= factor
    data['bones'] = [Bone(n, parent or None, head, tail, n != 'root')
                     for n, parent, head, tail in zip(names, data['bone_parents'], data['heads'], data['tails'])
                     if not n.endswith('.R')]
    data.update(height=height, kind='bear', normal_map=str(SOURCE.parent / 'normal.png'),
                normal_strength=.40 if juvenile else .55, detail_strength=.65, detail_non_color=True)
    rig = base_rig()
    jaw = names.index('jaw')
    mouth = data['vertices'][(data['weights'][:, jaw] > .3) & (data['tags'][:, 2] > .2)]
    if not len(mouth):
        mouth = data['tails'][jaw][None, :]
    nose = mouth[np.argmax(mouth[:, 0])].copy()
    middle = (data['heads'][names.index('hips'), 0] + chest_x * factor) / 2
    belly_region = (np.abs(data['vertices'][:, 0] - middle) < .12 * height)
    belly_region &= np.abs(data['vertices'][:, 1]) < .12 * height
    belly_height = float(data['vertices'][belly_region, 2].min())
    rig.extra.update(muzzle=('jaw', nose), belly_height=belly_height, mouth_surface=mouth,
                     skin_vertices=data['vertices'], skin_weights=data['weights'], skin_bones=data['bone_names'])
    meta = dict(common_name='Polar bear' if polar else 'Eurasian brown bear',
                scientific_name='Ursus maritimus' if polar else 'Ursus arctos',
                shoulder_height_m=height, asset_license='CC-BY-4.0',
                source_title='Strong Bear', source_author='Pedro B. Goulart (Pebegou)',
                source_url='https://sketchfab.com/3d-models/1d1bb459a39a428282ce4cb46bdc3ed5',
                source_pose='Upright roaring brown-bear-like sculpt; species identification is visual',
                derivation='Re-proportioned and repainted brown-bear sculpt' if polar else 'Reposed brown-bear sculpt',
                proportion_edits=edits, rest_pose='Sternal recumbency, chin on forepaws', sleep_pose='Lying on side with folded legs',
                activity_clips={'bathe': 'bathe'}, roughness=.9)
    if juvenile:
        parent = 'polar_bear' if polar else 'brown_bear'
        meta.update(variant_of=parent, variant='juvenile', relative_height=height / (1.5 if polar else 1.2))
    if polar:
        meta.update(swim=dict(clip='swim', depth_m=.91 * height, speed_mps=.65 * height),
                    water_origin_convention='Place the asset origin at water surface - depth_m')
    return Spec(name, data['bones'], fur_parts(data, juvenile),
                paint=partial(paint, data=data, polar=polar, juvenile=juvenile),
                texture_size=2048, target_tris=19500 if juvenile else 30000, meta=meta,
                rig=rig, clips=partial(bear_clips, height=height, polar=polar, juvenile=juvenile),
                key_all_locations=True, sculpt=data)


def bear_gait_pose(solver, gait, phase, drop):
    pose = quad.gait_pose(solver, gait, phase, drop)
    for key in ('FL', 'FR'):
        target = pose.feet[key]
        turn = rot_z(-7 if key == 'FL' else 7)
        target.foot = turn @ (target.foot if target.foot is not None else np.eye(3))
        target.toes = turn @ (target.toes if target.toes is not None else np.eye(3))
    pose.r('head', rz=2.8 * math.sin(2 * math.pi * phase))
    pose.r('chest', rx=1.5 * math.sin(2 * math.pi * phase))
    return pose


def bear_gait(solver, gait):
    drop = quad.fit_drop(solver, partial(bear_gait_pose, solver, gait), .975,
                         most=.35 * solver.rig.extra['height'])
    clip = Clip(gait.name, gait.period, partial(bear_gait_pose, solver, gait, drop=drop),
                speed=gait.speed, events=sorted((offset, 'step') for offset in gait.offsets.values()))
    clip.gait = gait
    return clip


def lying_pose(solver, params, height, sleeping, effort, phase, base=None):
    pose = deepcopy(base) if base is not None else quad.lie_pose(solver, params.lie)
    for key in ('FL', 'FR'):
        pose.feet[key].pos[0] = solver.head['head'][0] + .18 * height
        pose.feet[key].pos[2] = solver.toe_base(key)[2] + .015 * height
    pose.move('hips', dz=.001 * height * math.sin(4 * math.pi * phase))
    pose.r('neck1', ry=25 * effort, rz=20 if sleeping else 0)
    pose.r('neck2', ry=30 * effort, rz=12 if sleeping else 0)
    pose.r('head', ry=12 * effort, rz=10 if sleeping else 0)
    return pose


def bear_idle(original, phase):
    pose = original(phase)
    sniff = quad.bump(phase, .66, .15)
    pose.r('head', ry=-8 * sniff)
    pose.r('neck1', ry=-3 * sniff)
    return pose


def bear_eat(original, height, phase):
    pose = original(phase)
    # Hold food at the inner edge of the advanced left forepaw.
    pose.feet['FL'].pos += height * np.array([.08, -.06, 0])
    pose.r('head', rz=10 + 2 * math.sin(2 * math.pi * phase))
    return pose


def swim_pose(solver, height, phase):
    pose = Pose().move('hips', dz=-.025 * height)
    pose.r('neck1', ry=-8).r('head', ry=-6)
    pose.r('hips', rx=1.5 * math.sin(2 * math.pi * phase))
    for key in solver.rig.legs:
        foot = solver.toe_base(key)
        side = 1 if key.endswith('L') else -1
        angle = 2 * math.pi * (phase + (0 if side > 0 else .5))
        if key.startswith('F'):
            foot += height * np.array([.17 * math.sin(angle), side * .035 * math.cos(angle),
                                      .30 + .09 * math.cos(angle)])
            pose.plant(key, foot, foot_pitch=-15 + 20 * math.sin(angle), toe_pitch=-10)
        else:
            foot += height * np.array([-.12, side * .025 * math.sin(angle), .20])
            pose.plant(key, foot, foot_pitch=-15, toe_pitch=-10)
    return pose


def bathe_pose(solver, rest, height, phase):
    """A shallow-water wallow: the recumbent bear rolls its shoulders and paddles each forepaw."""
    angle = math.tau * phase
    pose = activity.copy_pose(rest.pose(phase))
    roll = 7 * math.sin(angle)
    pose.r('hips', rx=roll)
    pose.r('chest', rx=-.55 * roll)
    pose.r('head', rx=4 * math.sin(2 * angle + .5), rz=2 * math.sin(3 * angle))
    for key, offset in (('FL', 0), ('FR', .5)):
        stroke = math.sin(math.tau * (2 * phase + offset))
        pose.feet[key].pos += height * np.array([.045 * stroke, 0, .018 * max(0, stroke)])
    from .scan import skin_floor
    lift = .003 * height - skin_floor(solver, pose)
    if lift > 0:
        pose.move(solver.rig.body, dz=lift)
        for target in pose.feet.values():
            target.pos[2] += lift
    return pose


def bear_clips(solver, height, polar, juvenile):
    from .scan import skin_floor
    solver.rig.extra['height'] = height
    walk_speed = .42 if juvenile else 1.0
    lope_speed = .95 if juvenile else 2.5
    walk_period = (19 if juvenile else 28) / 30
    lope_period = (11 if juvenile else 17) / 30
    walk = quad.Gait('walk', walk_period, walk_speed * walk_period * .70, .70,
                     {'HL': 0, 'FL': .12, 'HR': .5, 'FR': .62}, lift=.05 * height,
                     bob=.005 * height, hip_yaw=1.2, chest_yaw=1.8, sway=.004 * height,
                     roll=.5, heel_off=0, swing_pitch=24, land_pitch=0, toe_follow=.1,
                     scapula=.20, head_nod=1.5, tail_sway=.5)
    lope = quad.Gait('trot', lope_period, lope_speed * lope_period * .38, .38,
                     {'HL': 0, 'HR': .10, 'FL': .48, 'FR': .58}, lift=.10 * height,
                     bob=-.018 * height, hip_yaw=.8, chest_yaw=1, sway=.002 * height,
                     roll=.3, heel_off=0, swing_pitch=30, land_pitch=0, toe_follow=.15,
                     scapula=.2, head_nod=2, tail_sway=.5)
    params = quad.Params([walk, lope], quad.Lie(solver.rig.extra['belly_height'] - .015 * height,
                         hind_tuck=.07 * height, hind_out=.10 * height, front_out=.03 * height,
                         foot_lift=.07 * height, front_elbow_out=.5, tail_floor=.07 * height),
                         head_down=quad.HeadDown(10, .10 * height, 4, (60, 35), 5),
                         eat_height=.065 * height, drink_height=.035 * height,
                         idle_bob=.001 * height, idle_shift=.003 * height, idle_roll=.25,
                         look_yaw=8, tail_idle=.5, ear_flick=7, chew_open=4,
                         call_neck=1, call_head=5, call_open=24, rest_bob=.001 * height)
    # Fit the actual skinned surface rather than assuming the skeleton is the belly.
    lower, upper = 0., params.lie.drop
    for _ in range(9):
        params.lie.drop = (lower + upper) / 2
        pose = lying_pose(solver, params, height, False, 0, 0)
        if skin_floor(solver, pose) < .008 * height:
            upper = params.lie.drop
        else:
            lower = params.lie.drop
    params.lie.drop = lower
    lying_base = quad.lie_pose(solver, params.lie)
    resting = quad.bisect_effort(partial(lying_muzzle_height, solver, params, height, False, lying_base),
                                .15 * height, 'bear resting chin')
    sleeping = quad.bisect_effort(partial(lying_muzzle_height, solver, params, height, True, lying_base),
                                 .17 * height, 'bear curled chin')
    idle = quad.idle_clip(solver, params)
    idle.pose = partial(bear_idle, idle.pose)
    eat = quad.eat_clip(solver, params)
    eat.pose = partial(bear_eat, eat.pose, height)
    rest = Clip('rest', 8, partial(lying_pose, solver, params, height, False, resting, base=lying_base))
    result = [idle, bear_gait(solver, walk), bear_gait(solver, lope), eat,
              quad.drink_clip(solver, params),
              rest,
              Clip('sleep', 8, partial(side_sleep_pose, side_sleep_base(solver, params, height, sleeping, lying_base), height)),
              quad.vocalize_clip(solver, params),
              Clip('bathe', 9, partial(bathe_pose, solver, rest, height))]
    if polar:
        swim = Clip('swim', 1.5, partial(swim_pose, solver, height), speed=.65 * height)
        swim.contact_keys = []
        result.append(swim)
    return result


def preview_stage(scene):
    """Game-like warm 55-degree sun, cool fill and a light sand floor."""
    import bpy
    from . import preview
    cam, floor = preview.setup_stage(scene, 3)
    preview.sculpt_lighting(scene)
    scene.render.threads = 3
    scene.cycles.samples = 16
    scene.view_settings.view_transform = 'AgX'
    sun = bpy.data.objects['sun']
    sun.rotation_euler = (math.radians(35), 0, math.radians(-35))
    sun.data.color = (1, .91, .78)
    sun.data.energy = 3.2
    rgb = np.array([200, 185, 150]) / 255
    linear = np.where(rgb < .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)
    shader = floor.data.materials[0].node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*linear, 1)
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.62, .74, .92, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .5
    return cam, floor


def still(scene, cam, path, location, target, lens=50, resolution=(1200, 800)):
    import bpy
    from . import preview
    cam.location = location
    cam.data.type = 'PERSP'
    cam.data.lens = lens
    preview._look_at(cam, target)
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)


def set_preview_clip(scene, arm, spec, clips, name, phase=.3):
    import bpy
    arm.animation_data.use_nla = False
    arm.animation_data.action = bpy.data.actions[spec.name + '_' + name]
    clip = next(item for item in clips if item['name'] == name)
    scene.frame_set(1 + round((clip['frames'] - 1) * phase))


def model_previews(scene, arm, spec, clips, args):
    """Eight/25-m visitor views and side, face, recumbency and six-phase clip sheets."""
    import bpy
    from . import anim, build, preview
    directory = Path(args.preview)
    directory.mkdir(parents=True, exist_ok=True)
    polar = spec.name.startswith('polar')
    juvenile = spec.name.endswith('juvenile')
    short = 'polar' if polar else 'brown'
    height = spec.meta['shoulder_height_m']
    points = spec.sculpt['vertices']
    center = build.to_blender((points.min(axis=0) + points.max(axis=0)) / 2)
    radius = float(np.linalg.norm(points.max(axis=0) - points.min(axis=0))) / 2
    cam, floor = preview_stage(scene)
    scene.cycles.samples = 12
    scene.cycles.use_denoising = False
    anim.clear_pose(arm)
    if not args.preview_clips_only:
        prefix = directory / ('S10_' + short)
        eye_path = str(prefix) + ('_cub.png' if juvenile else '_eye.png')
        still(scene, cam, eye_path, (5.2, center[1] - 6.079, 1.6), center,
              lens=55, resolution=(1600, 1000))
        if not juvenile:
            still(scene, cam, str(prefix) + '_25m.png', (16.25, center[1] - 18.998, 1.6), center,
                  lens=55, resolution=(1600, 1000))
            still(scene, cam, str(prefix) + '_side.png', (8, center[1], 1.6), center,
                  lens=55, resolution=(1600, 1000))
            head = spec.sculpt['heads'][spec.sculpt['bone_names'].tolist().index('head')]
            target = build.to_blender(head)
            still(scene, cam, str(prefix) + '_face.png', (1.8, target[1] - 2.5, 1.6), target,
                  lens=70, resolution=(1400, 1000))
            set_preview_clip(scene, arm, spec, clips, 'rest')
            still(scene, cam, str(prefix) + '_rest.png', (5.2, center[1] - 6.079, 1.6),
                  center - [0, 0, .20 * height], lens=60, resolution=(1600, 1000))
            set_preview_clip(scene, arm, spec, clips, 'sleep')
            still(scene, cam, str(prefix) + '_sleep.png', (8, center[1], 1.6),
                  center - [0, 0, .20 * height], lens=55, resolution=(1400, 900))
            anim.clear_pose(arm)
            if polar:
                ground_shader = floor.data.materials[0].node_tree.nodes.get('Principled BSDF')
                ground_shader.inputs['Base Color'].default_value = (.65, .76, .81, 1)
                still(scene, cam, str(prefix) + '_ice.png', (5.2, center[1] - 6.079, 1.6), center,
                      lens=55, resolution=(1600, 1000))
                set_preview_clip(scene, arm, spec, clips, 'swim')
                depth = spec.meta['swim']['depth_m']
                bpy.ops.mesh.primitive_plane_add(size=100, location=(0, 0, depth))
                water = bpy.context.object
                mat = bpy.data.materials.new('bear_preview_water')
                mat.use_nodes = True
                shader = mat.node_tree.nodes.get('Principled BSDF')
                shader.inputs['Base Color'].default_value = (.025, .16, .20, 1)
                shader.inputs['Roughness'].default_value = .22
                water.data.materials.append(mat)
                still(scene, cam, str(prefix) + '_swim.png', (5.2, center[1] - 6.079, depth + 1.6),
                      (center[0], center[1], depth), lens=55, resolution=(1600, 1000))
                bpy.data.objects.remove(water, do_unlink=True)
                ground_shader.inputs['Base Color'].default_value = (.578, .485, .305, 1)
                anim.clear_pose(arm)
    wanted = args.clips.split(',') if args.clips else [clip['name'] for clip in clips]
    scene.cycles.samples = 8
    cam.data.lens = 70
    for clip in clips:
        if clip['name'] in wanted:
            anim.preview_clip(scene, cam, arm, spec.name, clip, center, radius,
                              str(directory / ('S10_' + spec.name + '_' + clip['name'] + '_strip')),
                              phases=6, res=(400, 300))


def family_preview(output):
    import bpy
    from . import build
    scene = build.reset_scene()
    names = ('brown_bear', 'brown_bear_juvenile', 'polar_bear', 'polar_bear_juvenile')
    for name, offset in zip(names, (-2.0, -.75, .55, 2.0)):
        bpy.ops.import_scene.gltf(filepath=str(Path.cwd() / 'assets/animals' / (name + '.glb')))
        imported = list(bpy.context.selected_objects)
        roots = [ob for ob in imported if ob.parent is None]
        for root in roots:
            root.location.x += offset
        for ob in imported:
            if ob.animation_data:
                ob.animation_data.action = None
                ob.animation_data.use_nla = False
    cam, _ = preview_stage(scene)
    still(scene, cam, output, (5.2, -8.5, 2.0), (0, -.7, .75), lens=42,
          resolution=(1800, 1100))


def side_sleep_base(solver, params, height, effort, base=None):
    """Roll the folded recumbent pose onto its side, then settle skin and bones."""
    from .rig import rot_x
    from .scan import skin_floor
    folded = lying_pose(solver, params, height, True, effort, 0, base=base)
    for key, target in folded.feet.items():
        target.pos[1] = (.12 if key.endswith('L') else -.12) * height
        target.pole[1] = 0
    world = solver.solve(folded)
    pose = Pose()
    pose.offset = {name: offset.copy() for name, offset in folded.offset.items()}
    for name in solver.order:
        parent = solver.parent[name]
        if parent is None:
            continue
        parent_delta = world[parent][:3, :3] @ solver.rest[parent][:3, :3].T
        child_delta = world[name][:3, :3] @ solver.rest[name][:3, :3].T
        pose.rot[name] = parent_delta.T @ child_delta
    pose.rot['hips'] = rot_x(82) @ pose.rot['hips']
    world = solver.solve(pose)
    bone_floor = min(min(world[name][2, 3], solver.point(world, name, solver.tail[name])[2])
                     for name in solver.order if name != 'root')
    floor = min(skin_floor(solver, pose), bone_floor)
    pose.move('hips', dz=.008 * height - floor)
    return pose


def side_sleep_pose(base, height, phase):
    pose = Pose()
    pose.rot = dict(base.rot)
    pose.offset = {name: offset.copy() for name, offset in base.offset.items()}
    pose.move('hips', dz=.001 * height * math.sin(4 * math.pi * phase))
    return pose


def lying_muzzle_height(solver, params, height, sleeping, base, effort):
    return quad.muzzle(solver, lying_pose(solver, params, height, sleeping, effort, 0, base=base))[2]
