# SPDX-License-Identifier: MIT
"""Landmark rigs and portable derivatives for the two scanned/sculpted birds.

Animal coordinates are metres, +X forward, +Y left, +Z up. Source data is loaded
without pickle. All source conversion occurs in prepare_bird through Blender.
"""
import math
from pathlib import Path

import numpy as np

from . import biped, geom, noise as nz, sculpt
from .geom import Bone
from .rig import Clip, Pose, Rig, rot_z

SOURCES = Path(__file__).resolve().parents[1] / 'sources'
FLAMINGO_NECK = np.array([
    (-.015, 0, .83), (.085, 0, .785), (.145, 0, .82), (.195, 0, .885),
    (.208, 0, .955), (.19, 0, 1.025), (.155, 0, 1.095), (.135, 0, 1.165),
    (.14, 0, 1.235), (.15, 0, 1.27), (.18, 0, 1.315), (.22, 0, 1.345)])


def skeleton(kind, scale=1.):
    if kind == 'penguin':
        bones = [Bone('root', None, (0, 0, 0), (.1, 0, 0), False),
                 Bone('hips', 'root', (-.02, 0, .16), (0, 0, .31)),
                 Bone('chest', 'hips', (0, 0, .31), (.015, 0, .44)),
                 Bone('neck1', 'chest', (.015, 0, .44), (.04, 0, .535)),
                 Bone('head', 'neck1', (.04, 0, .535), (.12, 0, .59)),
                 Bone('jaw', 'head', (.10, 0, .582), (.195, 0, .604)),
                 Bone('tail1', 'hips', (-.06, 0, .10), (-.115, 0, .024))]
        legs = biped.add_leg(bones, 'hips', [(-.02, .068, .16), (.035, .068, .09),
                            (-.025, .068, .038), (.04, .068, .022), (.13, .068, .018)])
        bones.extend([Bone('flipper1.L', 'chest', (.015, .093, .35), (.025, .137, .28)),
                      Bone('flipper2.L', 'flipper1.L', (.025, .137, .28), (.022, .153, .21)),
                      Bone('flipper3.L', 'flipper2.L', (.022, .153, .21), (.015, .145, .13))])
        neck = ['neck1']
        wings = {s: ['flipper1.' + s, 'flipper2.' + s, 'flipper3.' + s] for s in ('L', 'R')}
        muzzle = (.20, 0, .605)
    else:
        bones = [Bone('root', None, (0, 0, 0), (.1, 0, 0), False),
                 Bone('hips', 'root', (-.11, 0, .79), (-.015, 0, .87)),
                 Bone('chest', 'hips', (-.015, 0, .87), FLAMINGO_NECK[0])]
        neck = biped.add_chain(bones, 'neck', 'chest', FLAMINGO_NECK)
        bones.extend([Bone('head', neck[-1], FLAMINGO_NECK[-1], (.33, 0, 1.265)),
                      Bone('jaw', 'head', (.31, 0, 1.27), (.365, 0, 1.13)),
                      Bone('tail1', 'hips', (-.30, 0, .82), (-.45, 0, .68)),
                      Bone('wing.L', 'chest', (.035, .085, .90), (-.35, .095, .80))])
        # A short femoral pivot lets the relieved leg fold medially beneath
        # the plumage instead of forcing the long shank to hang below the body.
        bones.append(Bone('hip.L', 'hips', (-.20, 0, .86), (-.11, .077, .78)))
        legs = biped.add_leg(bones, 'hip.L', [(-.11, .077, .78), (-.20, .077, .42),
                            (-.092, .077, .052), (-.035, .077, .024), (.075, .077, .018)])
        wings = {s: ['wing.' + s] for s in ('L', 'R')}
        muzzle = (.365, 0, 1.115)
    bones = [Bone(b.name, b.parent, np.array(b.head) * scale, np.array(b.tail) * scale, b.deform)
             for b in bones]
    rig = Rig('hips', legs, chest='chest', neck=neck, head='head', jaw='jaw', tail=['tail1'],
              extra={'kind': kind, 'scale': scale, 'wings': wings,
                     'muzzle': ('head', tuple(np.array(muzzle) * scale))})
    return bones, rig


def chain_weights(vertices, bones, kind, side_hints=None):
    """Fit separate anatomical regions before blending along their bone chains.

    Nearest-bone weighting alone attaches a folded wing or thin neck to the trunk.
    Region gates keep those surfaces on their correct, independently moving chain.
    """
    expanded = geom.expand_bones(bones)
    names = [b.name for b in expanded]
    lookup = {b.name: b for b in expanded}
    weights = np.zeros((len(vertices), len(names)), dtype=np.float32)
    tag_names = ['body', 'beak', 'foot', 'flipper', 'wing', 'eye', 'tail', 'bare']
    tags = np.zeros((len(vertices), len(tag_names)), dtype=np.float32)
    for index, point in enumerate(vertices):
        x, y, z = point
        left = y >= 0 if side_hints is None else side_hints[index] > 0
        side = 'L' if left else 'R'
        tag = 'body'
        if kind == 'penguin':
            if z < .068 and x > -.055:
                chain = ['foot.' + side, 'toes.' + side]
                tag = 'foot'
            elif z < .14 and .042 < abs(y) < .105 and -.055 < x < .05:
                chain = ['thigh.' + side, 'shin.' + side, 'foot.' + side]
                tag = 'bare'
            elif abs(y) > (.105 + .08 * max(0, .35-z)) and .09 < z < .37:
                chain = ['flipper1.' + side, 'flipper2.' + side, 'flipper3.' + side]
                tag = 'flipper'
            elif z < .12 and x < -.055:
                chain = ['tail1']
                tag = 'tail'
            elif x > .119 and z > .54:
                chain = ['jaw'] if z < .605 + .12 * (x - .12) else ['head']
                tag = 'beak'
            else:
                chain = ['hips', 'chest', 'neck1', 'head']
        else:
            if x < -.32 and z > .58:
                chain = ['tail1']
                tag = 'tail'
            elif z < .075:
                chain = ['foot.' + side, 'toes.' + side]
                tag = 'foot'
            elif z < .65 or (z < .81 and -.24 < x < -.05 and abs(y) > .045):
                chain = ['thigh.' + side, 'shin.' + side, 'foot.' + side]
                tag = 'bare'
            elif x > .295 and z > 1.09:
                chain = ['head']
                tag = 'beak'
            elif z > 1.29 and x > .235:
                chain = ['head']
            elif x > -.025 and z > .73:
                chain = ['chest'] + [f'neck{i}' for i in range(1, len(FLAMINGO_NECK))] + ['head']
            elif abs(y) > .061 and -.38 < x < .07 and z > .74:
                chain = ['wing.' + side]
                tag = 'wing'
            elif x < -.32:
                chain = ['tail1']
                tag = 'tail'
            else:
                chain = ['hips', 'chest']
        segments = [(name, lookup[name].head, lookup[name].tail) for name in chain]
        for name, weight in geom.chain_weights(point, segments, blend=1.1 if kind == 'flamingo' else .7).items():
            weights[index, names.index(name)] = weight
        tags[index, tag_names.index(tag)] = 1
    return weights, tags, tag_names


def neutral_vertices(vertices, kind):
    """Coordinate conversion and gentle, documented taxidermy corrections."""
    from_scale = .65 if kind == 'penguin' else 1.4
    result = np.column_stack([-vertices[:, 1], vertices[:, 0], vertices[:, 2]])
    result[:, 2] -= result[:, 2].min()
    result *= from_scale / result[:, 2].max()
    if kind == 'penguin':
        result += [.07, .027, 0]
        body_pivot = np.array([.02, 0, 0])
        result = (result-body_pivot) @ rot_z(-18).T + body_pivot
        # The mounted head looks over its left shoulder. Counter-rotate smoothly
        # above the neck while preserving the bill and its original UV detail.
        factor = nz.smoothstep(.43, .55, result[:, 2])
        pivot = np.array([.04, .005, .50])
        moved = (result - pivot) @ rot_z(-18).T + pivot
        result += (moved-result) * factor[:, None]
        foot = (result[:, 2] < .06) & (result[:, 0] > -.055)
        result[foot, 2] = .003 + result[foot, 2] * .90
    else:
        # Shape the planted shank around a visible backward hock. The raised leg
        # is replaced with its reflected counterpart in the preparation script.
        leg = (result[:, 2] < .72) & (result[:, 1] < 0) & (result[:, 0] > -.2)
        result[leg, 0] -= .085 * np.sin(np.pi * result[leg, 2] / .78)
    return result


def instance(kind, juvenile=False):
    path = SOURCES / kind / 'mesh.npz'
    source = sculpt.read_source(str(path))
    scale = (.82 if kind == 'penguin' else .78) if juvenile else 1.
    data = dict(source, vertices=source['vertices'].copy() * scale,
                source_path=str(path), birdscan=True, photo_texture=True, detail_non_color=True,
                detail_strength=.8 if kind == 'penguin' else .35)
    bones, rig = skeleton(kind, scale)
    if juvenile and kind == 'penguin':
        # Broader downy trunk, with the same bill and feet rather than a uniform miniature.
        mask = nz.smoothstep(.10, .19, source['vertices'][:, 2]) * nz.smoothstep(.49, .40, source['vertices'][:, 2])
        data['vertices'][:, 1] *= 1 + .075 * mask
    return data, bones, rig


def photo_context(context, data, start, end, size, atlas):
    """Attach a sampled source colour only for the opt-in bird sculpt painter."""
    import bpy
    path = str(Path(data['source_path']).parent / 'base.png')
    image = bpy.data.images.load(path, check_existing=True)
    width, height = image.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape(height, width, 4)
    xx = np.minimum((np.arange(size) / size / atlas * width).astype(int), width-1)
    yy = np.minimum((np.arange(start, end) / size * height).astype(int), height-1)
    context.photo = pixels[yy[:, None], xx[None, :], :3]


def penguin_paint(ctx, juvenile=False, scale=1.):
    x, y, z = ctx.x / scale, ctx.y / scale, ctx.z / scale
    black = np.array([.018, .024, .030])
    white = np.array([.84, .85, .81])
    front = nz.smoothstep(.015, .070, x)
    belly = front * nz.smoothstep(.49, .43, z)
    color = nz.mix(black, white, belly)
    # Exactly one breast horseshoe; no second Magellanic adult neck band.
    level = .395 - .15 * (np.abs(y) / .115) ** 2
    band = nz.smoothstep(.020, .010, np.abs(z-level)) * front * nz.smoothstep(.16, .23, z)
    color = nz.mix(color, black, band)
    for sy, sz, radius in [(.029, .30, .005), (-.038, .28, .007), (.052, .22, .004),
                            (-.019, .185, .004), (.012, .345, .005), (-.062, .34, .004)]:
        distance = ((y-sy)/radius)**2 + ((z-sz)/(radius*1.2))**2
        color = nz.mix(color, black, nz.smoothstep(1.2, .6, distance) * front)
    cheek = ((x-.035)/.068)**2 + ((z-.56)/.066)**2
    ribbon = nz.smoothstep(.60, .76, cheek) * nz.smoothstep(1.15, .96, cheek)
    ribbon *= nz.smoothstep(.026, .043, np.abs(y))
    color = nz.mix(color, white, ribbon)
    pink = ((x-.091)/.014)**2 + ((z-.613)/.007)**2
    color = nz.mix(color, np.array([.63, .29, .31]), nz.smoothstep(1.2, .5, pink))
    if juvenile:
        color = ctx.photo.copy()
    for tag in ('beak', 'foot', 'flipper', 'tail'):
        if juvenile and tag in ('flipper', 'tail'):
            continue
        color = nz.mix(color, black, ctx.tag(tag))
    pale_bill = nz.smoothstep(.010, .004, np.abs(x-.144)) * ctx.tag('beak')
    color = nz.mix(color, np.array([.37, .37, .32]), pale_bill * .65)
    blotches = nz.smoothstep(.63, .78, nz.fbm(ctx.P, 85, 2, seed=312)) * ctx.tag('foot')
    color = nz.mix(color, np.array([.52, .28, .29]), blotches)
    color = nz.mix(color, np.array([.008, .006, .004]), ctx.tag('eye'))
    highlight = ((x-.091)/.0012)**2 + ((z-.602)/.0012)**2
    color = nz.mix(color, np.array([.95, .96, .94]), ctx.tag('eye') * nz.smoothstep(1., .3, highlight))
    return color


def flamingo_paint(ctx):
    x, y, z = ctx.x, ctx.y, ctx.z
    feather = nz.fbm(ctx.P * [1, 2, 5], 90, 2, seed=73)
    color = np.broadcast_to(np.array([.94, .83, .80]), ctx.P.shape).copy()
    coverts = ctx.tag('wing') * nz.smoothstep(.035, .085, np.abs(y))
    color = nz.mix(color, np.array([.73, .22, .27]), coverts * .75)
    black = np.array([.023, .018, .025])
    edge = coverts * nz.smoothstep(-.16, -.31, x) * nz.smoothstep(.89, .79, z)
    color = nz.mix(color, black, edge)
    # Preserve the source's draped feather ridges through its luminance detail.
    color *= (.94 + .08 * feather)[..., None]
    color = nz.mix(color, np.array([.78, .45, .48]), ctx.tag('bare') + ctx.tag('foot'))
    bill = nz.mix(np.array([.80, .61, .56]), black, nz.smoothstep(1.235, 1.20, z))
    color = nz.mix(color, bill, ctx.tag('beak'))
    color = nz.mix(color, np.array([.78, .57, .10]), ctx.tag('eye'))
    pupil = ((x-.277)/.004)**2 + ((z-1.311)/.004)**2
    color = nz.mix(color, black, ctx.tag('eye') * nz.smoothstep(1., .4, pupil))
    return color


def penguin_pose(solver, name, phase):
    angle = math.tau * phase
    scale = solver.rig.extra['scale']
    if name in ('swim', 'float', 'rest', 'sleep'):
        # Water poses have no planted feet. The origin remains at ground datum;
        # the manifest tells the renderer how far to lower it below the surface.
        pose = Pose()
        pitch = 80 if name == 'swim' else 67 if name == 'float' else 78
        pose.r('hips', ry=pitch + (2 * math.sin(2 * angle) if name == 'swim' else 0))
        pose.move('hips', dz=(.055 if name == 'swim' else -.015 if name == 'float' else -.053) * scale)
        pose.r('neck1', ry=-25 if name == 'swim' else -57)
        pose.r('head', ry=-48 if name == 'swim' else -12)
        for side, sign in [('L', 1), ('R', -1)]:
            if name == 'swim':
                pose.r('flipper1.' + side, rx=sign * (58 + 38 * math.sin(angle)),
                       rz=sign * 14 * math.sin(2 * angle))
                pose.r('flipper2.' + side, rx=sign * 5 * math.sin(angle + .3))
                pose.r('flipper3.' + side, ry=7 * math.sin(2 * angle + .4))
            elif name == 'float':
                pose.r('flipper1.' + side, rx=sign * 12)
            else:
                pose.r('flipper1.' + side, rx=sign * 15)
            pose.r('thigh.' + side, ry=-35)
            pose.r('shin.' + side, ry=55)
            pose.r('foot.' + side, ry=(82 if name in ('rest', 'sleep') else 35)
                   + (8 * math.sin(angle + sign) if name == 'float' else 0))
        biped.sagittal_profile(solver, pose, ['tail1'],
                              [215 if name in ('rest', 'sleep') else 180 if name == 'swim' else 185])
        if name in ('rest', 'sleep'):
            pose.r('head', rz=2 * math.sin(angle))
            if name == 'sleep':
                pose.r('head', ry=30, rz=80)
        elif name == 'float':
            pose.move('hips', dz=.003 * scale * math.sin(angle))
        return pose
    pose = biped.standing(solver)
    if name == 'idle':
        pose.r('head', rz=10 * math.sin(angle), ry=2 * math.sin(2 * angle))
        pose.r('chest', ry=.8 * math.sin(2 * angle))
        for side, sign in [('L', 1), ('R', -1)]:
            pose.r('flipper1.' + side, rx=sign * (6 + 3 * math.sin(2 * angle)))
    elif name in ('eat', 'drink'):
        pose.move('hips', dz=-.018 * scale)
        pose.r('hips', ry=65)
        pose.r('neck1', ry=16)
        pose.r('head', ry=18 + 2 * math.sin(4 * angle))
    elif name == 'vocalize':
        pose.r('neck1', ry=-26)
        pose.r('head', ry=-62)
        pose.r('chest', ry=2 * math.sin(4 * angle))
        pose.r('jaw', ry=12 + 7 * math.sin(4 * angle))
        for side, sign in [('L', 1), ('R', -1)]:
            pose.r('flipper1.' + side, rx=sign * (45 + 10 * math.sin(2 * angle)))
    return pose


def penguin_clips(solver):
    scale = solver.rig.extra['scale']
    clips = [biped.gait_clip(solver, biped.Stride('walk', .5 * scale, .40, .64, .025 * scale,
                                                bob=.003 * scale, roll=5, wing=12, nod=1)),
             biped.gait_clip(solver, biped.Stride('trot', 1.05 * scale, .30, .43, .035 * scale,
                                                bob=.005 * scale, roll=6, lean=10, wing=25))]
    for name in ('idle', 'eat', 'drink', 'rest', 'sleep', 'vocalize', 'swim', 'float'):
        def pose(phase, clip_name=name):
            return penguin_pose(solver, clip_name, phase)
        duration = .5 if name == 'swim' else 3 if name == 'float' else 4
        speed = 1.5 * scale if name == 'swim' else .12 * scale if name == 'float' else 0
        clip = Clip(name, duration, pose, speed=speed)
        if name in ('swim', 'float'):
            clip.contact_keys = []
        clips.append(clip)
    return clips


def flamingo_one_leg(solver, sleeping=False):
    pose = biped.standing(solver)
    pose.r('hip.R', ry=-34, rz=-139)
    pose.plant('R', (-.302, -.020, .814), foot_pitch=-78, toe_pitch=-78, pole=(1, 0, 0))
    if sleeping:
        angles = [35, 65, 95, 125, 155, 185, 215, 240, 300, 330, 350]
        biped.sagittal_profile(solver, pose, solver.rig.neck, angles)
        biped.sagittal_profile(solver, pose, ['head'], [204])
    return pose


def flamingo_feeding(solver, effort):
    pose = biped.standing(solver)
    pose.move('hips', dz=-.22 * effort)
    pose.r('hips', ry=8 * effort)
    start = [math.degrees(math.atan2(*(b-a)[[2, 0]]))
             for a, b in zip(FLAMINGO_NECK, FLAMINGO_NECK[1:])]
    end = [-35 - 7 * index for index in range(len(start))]
    angles = [a + (b-a)*effort for a, b in zip(start, end)]
    biped.sagittal_profile(solver, pose, solver.rig.neck, angles)
    biped.sagittal_profile(solver, pose, ['head'], [-155])
    return pose


def flamingo_clips(solver):
    from .quadruped import bisect_effort, muzzle
    clips = [biped.gait_clip(solver, biped.Stride('walk', .7, .60, .65, .10, bob=.006, nod=2.4)),
             biped.gait_clip(solver, biped.Stride('trot', 1.45, .40, .45, .14, bob=.010, wing=6))]
    feeding = {}
    for name, height in [('eat', .075), ('drink', .035)]:
        def trial(value):
            return muzzle(solver, flamingo_feeding(solver, value))[2]
        effort = bisect_effort(trial, height, name)
        feeding[name] = flamingo_feeding(solver, effort)
    for name in ('idle', 'eat', 'drink', 'rest', 'sleep', 'vocalize'):
        def pose(phase, clip_name=name):
            angle = math.tau * phase
            if clip_name in ('idle', 'rest', 'sleep'):
                result = flamingo_one_leg(solver, clip_name != 'idle')
                if clip_name == 'idle':
                    amount = .5 - .5 * math.cos(angle)
                    neutral = [math.degrees(math.atan2(*(b-a)[[2, 0]]))
                               for a, b in zip(FLAMINGO_NECK, FLAMINGO_NECK[1:])]
                    tucked = [35, 65, 95, 125, 155, 185, 215, 240, 300, 330, 350]
                    profile = [a + (b-a)*amount for a, b in zip(neutral, tucked)]
                    biped.sagittal_profile(solver, result, solver.rig.neck, profile)
                    biped.sagittal_profile(solver, result, ['head'], [-36 - 94*amount])
                    result.r('head', rz=20 * amount, ry=4 * amount * math.sin(4 * angle))
                elif clip_name == 'sleep':
                    biped.sagittal_profile(solver, result, ['head'], [230])
                    result.r('head', rz=-20)
            elif clip_name in feeding:
                result = biped.copy_pose(feeding[clip_name])
                result.r('neck1', rz=5 * math.sin(angle))
                for side, offset in [('L', 0), ('R', math.pi)]:
                    target = solver.toe_base(side)
                    target[2] += .015 * max(0., math.sin(2 * angle + offset))**2
                    result.plant(side, target)
            else:
                result = biped.standing(solver)
                biped.sagittal_profile(solver, result, solver.rig.neck,
                                      [78 - 2 * i for i in range(len(solver.rig.neck))])
                result.r('head', rz=32 * math.sin(2 * angle))
            return result
        clips.append(Clip(name, 5, pose))
    return clips


def model_previews(scene, arm, spec, clips, args):
    """Opt-in bird evidence at visitor distance, plus the water and one-leg poses."""
    import bpy
    from . import anim, build, preview
    output = Path(args.preview)
    output.mkdir(parents=True, exist_ok=True)
    prefix = str(output / spec.name)
    height = spec.meta['shoulder_height_m']
    center = build.to_blender((.04, 0, height * .5))
    radius = height * .69
    cam, ground = preview.setup_stage(scene, radius)
    preview.sculpt_lighting(scene)
    scene.render.threads = 3
    anim.clear_pose(arm)
    if not args.preview_clips_only:
        preview.render_views(scene, cam, center, radius, prefix)
        cam.data.type = 'PERSP'
        cam.data.lens = 55
        scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
        scene.render.image_settings.file_format = 'PNG'
        distance = 3 if spec.name.startswith('penguin') else 4
        cam.location = (distance*.65, -distance*math.sqrt(1-.65**2), 1.6)
        preview._look_at(cam, center)
        scene.render.filepath = prefix + '_eye.png'
        bpy.ops.render.render(write_still=True)
        cam.location = (distance, -.04, height*.55)
        preview._look_at(cam, center)
        scene.render.filepath = prefix + '_side.png'
        bpy.ops.render.render(write_still=True)
        cam.location = (height*.75, -height*1.3, height*1.03)
        preview._look_at(cam, build.to_blender((.12 if height < 1 else .28, 0, height*.88)))
        scene.render.filepath = prefix + '_face.png'
        bpy.ops.render.render(write_still=True)
    want = args.clips.split(',') if args.clips else [clip['name'] for clip in clips]
    for clip in clips:
        if clip['name'] not in want:
            continue
        anim.preview_clip(scene, cam, arm, spec.name, clip, center, radius,
                          prefix + '_' + clip['name'] + '_strip', phases=6, res=(400, 300))
        if clip['name'] in ('swim', 'float', 'rest', 'eat'):
            arm.animation_data.action = bpy.data.actions[spec.name + '_' + clip['name']]
            scene.frame_set((clip['frames']-1)//4)
            cam.data.lens = 58
            cam.location = (2.4 if height < 1 else 3.5, -.08, height*.50)
            target_height = .32 if clip['name'] == 'eat' and height > 1 else .48
            preview._look_at(cam, build.to_blender((.12, 0, height*target_height)))
            scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
            if clip['name'] in ('swim', 'float'):
                # Lower the floor for an unobscured submerged side view. Water
                # level is shown as a slim reference rod, not an opaque plane.
                ground.location.z = -.20
                bpy.ops.mesh.primitive_cube_add(size=1, location=(0, .25, spec.meta[clip['name']]['depth_m']))
                waterline = bpy.context.object
                waterline.name = 'surface_reference'
                waterline.scale = (.006, 1.4, .002)
            scene.render.filepath = prefix + '_' + clip['name'] + '.png'
            bpy.ops.render.render(write_still=True)
            if clip['name'] in ('swim', 'float'):
                bpy.data.objects.remove(waterline, do_unlink=True)
                ground.location.z = 0
            arm.animation_data.action = None
    anim.clear_pose(arm)
    if spec.name == 'penguin' and not args.preview_clips_only:
        mesh = next(ob for ob in arm.children if ob.type == 'MESH')
        copies = []
        for index, name in enumerate(['idle', 'walk', 'rest', 'vocalize', 'sleep']):
            arm.animation_data.action = bpy.data.actions[spec.name + '_' + name]
            scene.frame_set(index * 7)
            evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
            duplicate = bpy.data.objects.new('group_' + name, bpy.data.meshes.new_from_object(evaluated))
            scene.collection.objects.link(duplicate)
            duplicate.location = ((index-2)*.45, .13*(index%2), 0)
            copies.append(duplicate)
        mesh.hide_render = True
        cam.location = (2.6, -3.5, 1.6)
        cam.data.lens = 46
        preview._look_at(cam, (0, 0, .32))
        scene.render.filepath = prefix + '_group.png'
        bpy.ops.render.render(write_still=True)
        mesh.hide_render = False
        for duplicate in copies:
            bpy.data.objects.remove(duplicate, do_unlink=True)
        anim.clear_pose(arm)
