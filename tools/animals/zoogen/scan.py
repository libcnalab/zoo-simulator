# SPDX-License-Identifier: MIT
"""Static quadruped derivatives: landmark rigs, regional skinning and equid motion.

The source importer lives in prepare_scan.py. Runtime builds accept only the
prepared numeric archive (never pickle), and do not need the downloaded source.
"""
import math
from functools import lru_cache, partial
from pathlib import Path

import numpy as np

from . import quadruped as quad
from .geom import Bone, Spec, expand_bones
from .rig import Clip, Leg, Pose, Rig
from .sculpt import prepare_instance, read_source


def landmark_bones(landmarks):
    """Expand explicit species landmarks into the canonical quadruped hierarchy."""
    bones = [Bone('root', None, (0, 0, 0), (.1, 0, 0), False)]
    for name, parent, start, end in landmarks:
        bones.append(Bone(name, parent, np.array(start), np.array(end)))
    return expand_bones(bones)


def segment_distance(points, start, end):
    axis = end - start
    t = np.clip((points - start) @ axis / max(float(axis @ axis), 1e-12), 0, 1)
    return np.linalg.norm(points - start - t[:, None] * axis, axis=1)


def corrected_weights(vertices, bones, heat):
    """Constrain heat weights to anatomical regions before smoothing/truncation.

    Heat diffusion alone leaks across the belly and between nearby legs. Distances
    to the measured joint chains provide a fallback for disconnected scan pieces.
    """
    by = {b.name: b for b in bones}
    x, y, z = vertices.T
    chest = by['chest'].head
    hip = by['hips'].head
    scale = float(chest[2]) / 1.20
    weights = np.zeros((len(vertices), len(bones)))
    side_gate = np.interp(z, [.65 * scale, .95 * scale], [.025 * scale, .15 * scale])
    for j, bone in enumerate(bones):
        if not bone.deform:
            continue
        distance = segment_distance(vertices, np.asarray(bone.head), np.asarray(bone.tail))
        allowed = np.ones(len(vertices), dtype=bool)
        stem = bone.name.split('.')[0]
        if stem in ('scapula', 'upper_arm', 'forearm', 'hand', 'fingers'):
            allowed &= (x > by['forearm.L'].head[0] - .10 * scale) & (z < chest[2] + .03 * scale)
            allowed &= y * (1 if bone.name.endswith('.L') else -1) > side_gate
            allowed &= x < by['upper_arm.L'].head[0] + .09 * scale
            if stem != 'scapula':
                allowed &= z < by['upper_arm.L'].head[2] + .12 * scale
        elif stem in ('thigh', 'shin', 'foot', 'toes'):
            allowed &= (x < by['shin.L'].head[0] + .12 * scale) & (z < hip[2] + .06 * scale)
            allowed &= y * (1 if bone.name.endswith('.L') else -1) > side_gate
        elif stem.startswith('tail'):
            allowed &= x < hip[0] - .19 * scale
            allowed &= np.abs(y) < .12 * scale
        elif stem == 'ear':
            allowed &= z > by['ear.L'].head[2] - .01 * scale
            allowed &= y * (1 if bone.name.endswith('.L') else -1) > .045 * scale
            allowed &= x > by['head'].head[0] - .10 * scale
        elif stem in ('head', 'jaw'):
            allowed &= x > by['head'].head[0] - .11
            if stem == 'jaw':
                allowed &= z < by['jaw'].head[2] - .035 * scale
        elif stem in ('hips', 'spine1', 'spine2', 'chest'):
            allowed &= z > .50 * scale
            allowed &= x < chest[0] + .40 * scale
        elif stem in ('neck1', 'neck2'):
            allowed &= z > np.where(x > chest[0] + .30 * scale, .65, .86) * scale
            allowed &= x > chest[0] - .13 * scale
        width = .11 if stem in ('head', 'jaw', 'ear') or stem.startswith('tail') else .16
        width *= scale
        geometric = np.exp(-np.square(distance / width) * 1.5)
        # Retain heat's surface connectivity inside each anatomical region;
        # the geometric prior repairs unweighted islands.
        weights[:, j] = allowed * (.8 * heat[:, j] + .2 * geometric)
        if stem in ('upper_arm', 'forearm', 'hand', 'fingers', 'thigh', 'shin', 'foot', 'toes'):
            weights[:, j] = allowed * geometric * (.3 + .7 * heat[:, j])
        if stem in ('scapula', 'upper_arm', 'forearm', 'hand', 'fingers'):
            weights[:, j] *= np.clip((1.10 * scale - z) / (.30 * scale), 0, 1)
        elif stem in ('thigh', 'shin', 'foot', 'toes'):
            weights[:, j] *= np.clip((1.05 * scale - z) / (.35 * scale), 0, 1)
        if stem in ('neck1', 'neck2'):
            weights[:, j] *= np.clip((x - chest[0] + .10 * scale) / (.30 * scale), 0, 1)
    empty = weights.sum(axis=1) < 1e-20
    for index in np.flatnonzero(empty):
        distances = [segment_distance(vertices[index:index+1], np.asarray(b.head),
                                      np.asarray(b.tail))[0] if b.deform else 1e6 for b in bones]
        weights[index, int(np.argmin(distances))] = 1
    weights /= weights.sum(axis=1, keepdims=True)
    return weights


def quadruped_rig(data):
    legs = {}
    for side in ('L', 'R'):
        suffix = '.' + side
        legs['F' + side] = Leg('upper_arm' + suffix, 'forearm' + suffix, 'hand' + suffix,
                               'fingers' + suffix, 'scapula' + suffix)
        legs['H' + side] = Leg('thigh' + suffix, 'shin' + suffix, 'foot' + suffix, 'toes' + suffix)
    nose = data['tails'][data['bone_names'].tolist().index('jaw')].copy()
    jaw_index = data['bone_names'].tolist().index('jaw')
    mouth = data['vertices'][data['mouth_targets'] == jaw_index].copy()
    if not len(mouth):
        mouth = data['vertices'][data['weights'][:, jaw_index] > .5].copy()
    if not len(mouth):
        mouth = nose[None, :]
    nose = mouth[np.argmin(mouth @ np.array([-3., 0., 1.]))].copy()
    heads = dict(zip(data['bone_names'], data['heads']))
    middle = (heads['hips'][0] + heads['chest'][0]) * .5
    points = data['vertices']
    belly = (np.abs(points[:, 0] - middle) < .08 * data['height'])
    belly &= np.abs(points[:, 1]) < .08 * data['height']
    belly_height = float(points[belly, 2].min())
    return Rig('hips', legs, ['spine1', 'spine2'], 'chest', ['neck1', 'neck2'], 'head', 'jaw',
               ['tail1', 'tail2', 'tail3', 'tail4'], ['ear.L', 'ear.R'],
               extra={'muzzle': ('jaw', nose), 'mouth_surface': mouth, 'belly_height': belly_height,
                      'skin_vertices': data['vertices'], 'skin_weights': data['weights'],
                      'skin_bones': data['bone_names']})


def make_scan_spec(config):
    path = Path(__file__).resolve().parents[1] / 'sources' / config['source'] / 'scan.npz'
    data = prepare_instance(path, config.get('edits', {}), config['height'])
    # Measure the dorsal shoulder beside the narrow mane, not the hair crest.
    chest = data['heads'][data['bone_names'].tolist().index('chest')]
    points = data['vertices']
    shoulder = (np.abs(points[:, 0] - chest[0]) < .06 * config['height'])
    shoulder &= (np.abs(points[:, 1]) > .025 * config['height'])
    shoulder &= (np.abs(points[:, 1]) < .10 * config['height'])
    factor = config['height'] / points[shoulder, 2].max()
    for key in ('vertices', 'heads', 'tails'):
        data[key] = data[key] * factor
    data['scale'] *= factor
    data['bones'] = [Bone(name, parent or None, head, tail, name != 'root')
                     for name, parent, head, tail in zip(data['bone_names'], data['bone_parents'],
                                                         data['heads'], data['tails'])
                     if not name.endswith('.R')]
    fur = config.get('fur_length', 0)
    if fur:
        vertices = data['vertices']
        triangles = data['triangles']
        faces = vertices[triangles]
        normals = np.zeros_like(vertices)
        face_normals = np.cross(faces[:, 1] - faces[:, 0], faces[:, 2] - faces[:, 0])
        for corner in range(3):
            np.add.at(normals, triangles[:, corner], face_normals)
        normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-12)
        grain = np.sin(vertices @ np.array([133, 197, 251])) ** 2
        coat = data['tags'][:, data['tag_names'].tolist().index('body')].copy()
        coat *= np.clip((vertices[:, 2] - .10) / .10, 0, 1)
        data['vertices'] = vertices + normals * (fur * coat * grain)[:, None]
    data.update(rigged=True, optional_maps=True, kind='scan', height=config['height'],
                shape={'normal_strength': 1, 'color_saturation': 1, 'color_gain': 1})
    if config.get('texture_transform') is not None:
        data['texture_transform'] = config['texture_transform']
    # A smooth UV displacement gives each flank/variant a different stripe contour;
    # the sagittal fade keeps the dorsal and ventral joins identical.
    tri_points = data['vertices'][data['triangles']]
    uv = data['uv'].copy()
    amplitude = config.get('stripe_variation', 0)
    fade = np.clip(np.abs(tri_points[..., 1]) / (.14 * config['height']), 0, 1) ** 2
    fade *= uv_border_fade(str(path))
    right = tri_points[..., 1] < 0
    side_factor = right.astype(float) + (.55 if config.get('vary_both_sides', False) else 0)
    uv[..., 0] += amplitude * fade * side_factor * np.sin(31 * uv[..., 1])
    data['uv'] = uv
    meta = dict(config.get('meta', {}), shoulder_height_m=config['height'])
    return Spec(config['name'], data['bones'], [], texture_size=2048,
                target_tris=config.get('target_tris', 29000),
                meta=meta, rig=quadruped_rig(data), sculpt=data, key_all_locations=True,
                clips=partial(config.get('clip_factory', equid_clips), height=config['height']))


def coat_variation(rgba, variant):
    """Retain the authored pattern; foal pigment is warm brown instead of black."""
    result = rgba.copy()
    rgb = result[..., :3]
    red = (rgb[..., 0] > 1.45 * rgb[..., 1]) & (rgb[..., 0] > 1.5 * rgb[..., 2])
    grey = rgb.mean(axis=-1) * .65
    result[..., :3] = np.where(red[..., None], grey[..., None], rgb)
    if variant == 'juvenile':
        lum = result[..., :3] @ np.array([.2126, .7152, .0722])
        dark = np.array([.18, .085, .035])
        pale = np.array([.91, .82, .65])
        result[..., :3] = dark + np.clip(lum[..., None], 0, 1) * (pale - dark)
    return result


def equid_clips(solver, height):
    scale = height / 1.35
    gaits = [quad.Gait('walk', .80, .62 * scale, .65, quad.WALK, lift=.08 * scale,
                       bob=.009 * scale, hip_yaw=1.2, chest_yaw=1.1, sway=.006 * scale,
                       roll=.5, heel_off=10, swing_pitch=28, land_pitch=0, toe_follow=0,
                       scapula=.3, head_nod=2.8, tail_sway=3),
             quad.Gait('trot', .64, .65 * scale, .42, quad.TROT, lift=.15 * scale,
                       bob=-.025 * scale, hip_yaw=1, chest_yaw=1, sway=.003 * scale,
                       roll=.3, heel_off=12, swing_pitch=35, land_pitch=0, toe_follow=0,
                       scapula=.3, head_nod=1.5, tail_sway=4)]
    params = quad.Params(gaits, quad.Lie(solver.rig.extra['belly_height'] - .008 * scale, hind_tuck=.12 * scale,
                                        hind_out=.08 * scale, front_out=.04 * scale,
                                        foot_lift=.04 * scale),
                         head_down=quad.HeadDown(12 if height < 1 else 7,
                                                  (.20 if height < 1 else .07) * scale,
                                                  2, (105, 0), -30),
                         drink_height=.045,
                         idle_bob=.002 * scale, idle_roll=.2, idle_shift=.003 * scale,
                         look_yaw=7, tail_idle=4, sleep_neck=(35, 30), sleep_head=20,
                         sleep_chin=.07 * scale, sleep_yaw=0, call_neck=-7, call_head=-8,
                         call_open=12, rest_bob=.002 * scale, sleep_bob=.001 * scale)
    fit_lying_clearance(solver, params, scale)
    base = quad.lie_pose(solver, params.lie)
    effort = quad.bisect_effort(
        lambda k: mouth_height(solver, equid_rest_pose(solver, params, scale, True, k, 0, base)),
        .005 * scale, 'equid sleep')
    clips = [Clip('idle', params.idle_period, partial(equid_idle_pose, solver, params, scale))]
    clips += [quad.gait_clip(solver, gait) for gait in gaits]
    clips += [quad.eat_clip(solver, params), quad.drink_clip(solver, params),
              Clip('rest', 8, partial(equid_rest_pose, solver, params, scale, False, 0, base=base)),
              Clip('sleep', 8, partial(equid_rest_pose, solver, params, scale, True, effort, base=base)),
              quad.vocalize_clip(solver, params)]
    clips[-1].pose = partial(equid_call_pose, solver, params)
    return clips


def model_previews(scene, arm, spec, clips, args):
    """Visitor-distance views, side-view landmark overlay and six-phase clip sheets."""
    import bpy
    from . import anim, build, preview
    from .rigged import model_previews as rigged_previews
    rigged_previews(scene, arm, spec, clips, args)
    if args.preview_clips_only:
        return
    anim.clear_pose(arm)
    cam = scene.camera
    prefix = str(Path(args.preview) / spec.name)
    cam.data.lens = 55
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
    cam.location = (0, -4, 1.6)
    preview._look_at(cam, build.to_blender((.7, 0, .95 * spec.meta['shoulder_height_m'])))
    scene.render.filepath = prefix + '_face.png'
    bpy.ops.render.render(write_still=True)
    rest = next(clip for clip in clips if clip['name'] == 'rest')
    arm.animation_data.use_nla = False
    arm.animation_data.action = bpy.data.actions[spec.name + '_rest']
    scene.frame_set(int(rest['frames'] * .28))
    cam.location = (6, 0, 1.6)
    preview._look_at(cam, (0, 0, .4))
    scene.render.filepath = prefix + '_rest_eye.png'
    bpy.ops.render.render(write_still=True)
    anim.clear_pose(arm)
    render_bones(scene, cam, spec, prefix + '_bones.png')


def render_bones(scene, cam, spec, path):
    """Orthographic lateral projection places landmark lines in front of the skin."""
    import bpy
    from mathutils import Vector
    from . import build, preview
    mat = bpy.data.materials.new('landmark_overlay')
    mat.diffuse_color = (1, .12, .01, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes['Principled BSDF']
    shader.inputs['Base Color'].default_value = (1, .12, .01, 1)
    shader.inputs['Emission Color'].default_value = (1, .08, .005, 1)
    shader.inputs['Emission Strength'].default_value = .8
    objects = []
    for bone in expand_bones(spec.bones):
        if not bone.deform or bone.name.endswith('.R'):
            continue
        start, end = np.array(bone.head), np.array(bone.tail)
        start[1] = end[1] = .52
        a, b = Vector(build.to_blender(start)), Vector(build.to_blender(end))
        direction = b - a
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=.006,
                                           depth=direction.length, location=(a+b)/2)
        ob = bpy.context.object
        ob.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler()
        ob.data.materials.append(mat)
        objects.append(ob)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=8, ring_count=4, radius=.014, location=a)
        bpy.context.object.data.materials.append(mat)
        objects.append(bpy.context.object)
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 3.0
    cam.location = (6, -.1, .83)
    preview._look_at(cam, (0, -.1, .83))
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    for ob in objects:
        bpy.data.objects.remove(ob, do_unlink=True)
    cam.data.type = 'PERSP'


def family_preview(output):
    """Render all three derivative builds together at their authored metre scale."""
    import importlib
    import bpy
    from . import build, preview
    scene = build.reset_scene()
    for name, offset in [('zebra', -.95), ('zebra_female', .25), ('zebra_juvenile', 1.2)]:
        spec = importlib.import_module('species.' + name).make_spec()
        ob, _ = build.build_mesh(spec)
        ob.location.x = offset
        ob.location.y = -.3 if name == 'zebra_juvenile' else 0
    cam, _ = preview.setup_stage(scene, 3)
    preview.sculpt_lighting(scene)
    scene.render.resolution_x, scene.render.resolution_y = 1800, 1100
    cam.data.lens = 46
    cam.location = (5, -6.5, 2.4)
    preview._look_at(cam, (0, 0, .75))
    scene.render.filepath = str(output)
    bpy.ops.render.render(write_still=True)


def folded_forelegs(solver, pose, scale):
    """Flex the carpus so the front cannon lies back under the forearm."""
    for key in ('FL', 'FR'):
        toe = solver.toe_base(key)
        toe[0] -= .12 * scale
        toe[1] += (.035 if key == 'FL' else -.035) * scale
        toe[2] = .17 * scale
        pose.plant(key, toe, foot_pitch=95, toe_pitch=95,
                   pole=(-1, .2 if key == 'FL' else -.2, -.3))
    return pose


def equid_rest_pose(solver, params, scale, sleeping, effort, phase, base=None):
    if base is None:
        base = quad.lie_pose(solver, params.lie)
    pose = Pose()
    pose.rot = dict(base.rot)
    pose.offset = dict(base.offset)
    pose.feet = dict(base.feet)
    pose = folded_forelegs(solver, pose, scale)
    pose.move(solver.rig.body, dz=.001 * scale * math.sin(4 * math.pi * phase))
    for bone in solver.rig.neck:
        pose.r(bone, ry=(-10 + 38 * effort if sleeping else -10))
    pose.r(solver.rig.head, ry=(-3 + 18 * effort if sleeping else -3))
    return pose


def equid_idle_pose(solver, params, scale, phase):
    pose = quad.idle_clip(solver, params).pose(phase)
    toe = solver.toe_base('HR')
    toe[0] += .055 * scale
    toe[2] += .025 * scale
    pose.plant('HR', toe, foot_pitch=15, toe_pitch=12)
    pose.r(solver.rig.head, rz=3 * math.sin(12 * math.pi * phase) *
           quad.bump(phase, .65, .10))
    return pose


def repose_bind(data, rotations):
    """Bake landmark-relative neutral rotations into the surface and joint rest data."""
    from .rig import euler
    names = data['bone_names'].tolist()
    parents = data['bone_parents'].tolist()
    transforms = {}
    heads, tails = [], []
    points = np.zeros_like(data['vertices'], dtype=np.float64)
    for index, (name, parent) in enumerate(zip(names, parents)):
        head = data['heads'][index]
        local = np.eye(4)
        local[:3, :3] = euler(*rotations.get(name, (0, 0, 0)))
        local[:3, 3] = head - local[:3, :3] @ head
        transform = transforms.get(parent, np.eye(4)) @ local
        transforms[name] = transform
        heads.append((transform @ np.append(head, 1))[:3])
        tails.append((transform @ np.append(data['tails'][index], 1))[:3])
        points += data['weights'][:, index, None] * (
            data['vertices'] @ transform[:3, :3].T + transform[:3, 3])
    return dict(data, vertices=points.astype(np.float32), heads=np.array(heads, dtype=np.float32),
                tails=np.array(tails, dtype=np.float32))


def reweight_source(data, landmarks, rotations):
    """Refit the saved surface/heat solution without reopening untrusted input."""
    from . import build
    bones = landmark_bones(landmarks)
    vertices = data['raw_vertices']
    weights = corrected_weights(vertices, bones, data['heat_weights'])
    weights = build._laplacian(weights, data['triangles'], 2, .18)
    weights = build._limit_influences(weights)
    for index in np.flatnonzero(data['mouth_targets'] >= 0):
        weights[index] = 0
        weights[index, data['mouth_targets'][index]] = 1
    tags = np.zeros_like(data['tags'])
    tag_names = data['tag_names'].tolist()
    for index, name in enumerate(data['bone_names']):
        stem = name.split('.')[0]
        tag = ('tail' if stem.startswith('tail') else 'hoof' if stem in ('fingers', 'toes')
               else stem if stem in tag_names else 'body')
        tags[:, tag_names.index(tag)] += weights[:, index]
    data = dict(data, vertices=vertices, weights=weights.astype(np.float32), tags=tags,
                heads=np.array([bone.head for bone in bones], dtype=np.float32),
                tails=np.array([bone.tail for bone in bones], dtype=np.float32))
    return repose_bind(data, rotations)


@lru_cache(maxsize=4)
def uv_border_fade(path):
    """Protect UV-island edges while varying stripes inside each painted chart.

    Moving a chart edge samples unrelated atlas content and makes a visible seam.
    Shortest-path distances on the UV graph fade the perturbation to zero there.
    """
    import heapq
    uv = read_source(path)['uv']
    points, inverse = np.unique(uv.reshape(-1, 2), axis=0, return_inverse=True)
    triangles = inverse.reshape(-1, 3)
    edges = np.concatenate([triangles[:, [0, 1]], triangles[:, [1, 2]], triangles[:, [2, 0]]])
    edges = np.sort(edges, axis=1)
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    adjacency = [[] for _ in points]
    for left, right in edges:
        distance = float(np.linalg.norm(points[left] - points[right]))
        adjacency[left].append((int(right), distance))
        adjacency[right].append((int(left), distance))
    boundary = np.unique(edges[counts == 1])
    distances = np.full(len(points), np.inf)
    distances[boundary] = 0
    queue = [(0., int(index)) for index in boundary]
    heapq.heapify(queue)
    while queue:
        distance, index = heapq.heappop(queue)
        if distance > distances[index] or distance > .02:
            continue
        for neighbor, length in adjacency[index]:
            candidate = distance + length
            if candidate < distances[neighbor]:
                distances[neighbor] = candidate
                heapq.heappush(queue, (candidate, neighbor))
    return np.clip(distances[inverse].reshape(uv.shape[:2]) / .02, 0, 1)


def mouth_height(solver, pose):
    """Ground contact follows the lip surface, not an internal jaw landmark."""
    points = solver.rig.extra['mouth_surface']
    transform = solver.solve(pose)['jaw'] @ np.linalg.inv(solver.rest['jaw'])
    return float((points @ transform[:3, :3].T + transform[:3, 3])[:, 2].min())


def skin_floor(solver, pose):
    """Lowest point of the actual linear-blend skin in a proposed pose."""
    extra = solver.rig.extra
    vertices = extra['skin_vertices']
    weights = extra['skin_weights']
    world = solver.solve(pose)
    heights = np.zeros(len(vertices))
    for column, bone in enumerate(extra['skin_bones']):
        transform = world[bone] @ np.linalg.inv(solver.rest[bone])
        heights += weights[:, column] * (vertices @ transform[2, :3] + transform[2, 3])
    return float(heights.min())


def fit_lying_clearance(solver, params, scale):
    """Settle onto the brisket/limb surface without burying the deformed skin."""
    upper = params.lie.drop
    lower = max(0, upper - .18 * scale)
    for _ in range(10):
        candidate = (lower + upper) * .5
        params.lie.drop = candidate
        pose = equid_rest_pose(solver, params, scale, False, 0, 0)
        if skin_floor(solver, pose) < .005 * scale:
            upper = candidate
        else:
            lower = candidate
    params.lie.drop = lower


def equid_call_pose(solver, params, phase):
    pose = quad.vocalize_clip(solver, params).pose(phase)
    effort = quad.envelope(phase, .22, .72)
    for bone in solver.rig.ears:
        side = 1 if bone.endswith('.L') else -1
        pose.r(bone, ry=15 * effort, rz=-side * 10 * effort)
    return pose
