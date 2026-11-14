# SPDX-License-Identifier: MIT
"""Second-generation giraffe anatomy, scan graft, and species-specific behaviour.

The historical shoulder_height_m field means total head-top height for giraffes.
All construction and posing coordinates are metres, +X forward and +Z up.
"""
from dataclasses import replace
from functools import partial
import math
from pathlib import Path

import numpy as np

from . import geom, noise as nz, quadruped as quad
from .anatomy import chain
from .geom import Bone, Part, R, Spec
from .rig import Clip, Leg, Pose, Rig, rot_x, rot_y, rot_z
from .sculpt import read_source

SOURCE = Path(__file__).resolve().parents[1] / 'sources/giraffe/scan.npz'
HEIGHTS = {'male': 5.2, 'female': 4.5, 'juvenile': 2.0}


def proportions(points, variant):
    """A calf has relatively longer legs and a shorter neck, not a scaled adult."""
    p = np.asarray(points, dtype=float).copy()
    if variant == 'juvenile':
        p[..., 0] *= .46
        p[..., 1] *= .46
        p[..., 2] = np.interp(p[..., 2], [0, 2.0, 3.2, 4.5, 5.2], [0, .93, 1.37, 1.678, 2.0])
    elif variant == 'female':
        p *= 4.5 / 5.2
        p[..., 1] *= .91
    return p



def adult_space(points, variant):
    """Reference coordinates keep coat cells and facial masks consistent across ages."""
    p = np.asarray(points, dtype=float).copy()
    if variant == 'juvenile':
        p[..., :2] /= .46
        p[..., 2] = np.interp(p[..., 2], [0,.93,1.37,1.678,2], [0,2,3.2,4.5,5.2])
    elif variant == 'female':
        p /= 4.5 / 5.2
        p[..., 1] /= .91
    return p


def skeleton(variant):
    bones = [Bone('root', None, (0, 0, 0), (.2, 0, 0), False)]
    spine = [(-.95, 0, 2.35), (-.55, 0, 2.42), (.05, 0, 2.55),
             (.55, 0, 2.71), (.82, 0, 2.95)]
    names = ['hips', 'spine1', 'spine2', 'chest']
    parent = 'root'
    for index, name in enumerate(names):
        bones.append(Bone(name, parent, spine[index], spine[index+1]))
        parent = name
    neck = [(.82, 0, 2.95), (1.00, 0, 3.32), (1.16, 0, 3.73),
            (1.31, 0, 4.13), (1.43, 0, 4.53), (1.57, 0, 4.82)]
    for i in range(5):
        name = f'neck{i+1}'
        bones.append(Bone(name, parent, neck[i], neck[i+1]))
        parent = name
    bones.extend([Bone('head', parent, neck[-1], (2.04, 0, 4.64)),
                  Bone('jaw', 'head', (1.75, 0, 4.65), (2.04, 0, 4.58)),
                  Bone('ear.L', 'head', (1.71, .13, 4.93), (1.66, .37, 4.98))])
    fore = [(.73, .35, 2.93), (.76, .39, 2.74), (.54, .39, 1.39),
            (.80, .39, .28), (.88, .39, .10), (1.05, .39, .07)]
    hind = [(-1.02, .37, 2.40), (-.67, .39, 1.55),
            (-1.09, .39, .80), (-1.00, .39, .10), (-.82, .39, .07)]
    for pts, labels, parent in [(fore, ['scapula', 'upper_arm', 'forearm', 'hand', 'fingers'], 'chest'),
                                (hind, ['thigh', 'shin', 'foot', 'toes'], 'hips')]:
        for i, label in enumerate(labels):
            name = label + '.L'
            bones.append(Bone(name, parent, pts[i], pts[i+1]))
            parent = name
    tail = [(-1.40, 0, 2.54), (-1.57, 0, 2.12), (-1.65, 0, 1.68),
            (-1.68, 0, 1.27), (-1.69, 0, .91)]
    parent = 'hips'
    for i in range(4):
        name = f'tail{i+1}'
        bones.append(Bone(name, parent, tail[i], tail[i+1]))
        parent = name
    for bone in bones:
        bone.head = proportions(bone.head, variant)
        bone.tail = proportions(bone.tail, variant)
    return bones


def cloven_hoof(name, x, bone):
    """Tapered hoof capsules, 15 cm high and 25 cm long, with level soles."""
    parts = []
    for digit in (-1, 1):
        y = .39 + digit * .051
        parts.append(Part(f'{name}.{digit}.L', [
            R((x, y, .004), .048, .10, .15, n=2.6),
            R((x, y, .025), .050, .10, .15, n=2.6),
            R((x, y, .115), .042, .083, .105, n=2.3),
            R((x, y, .154), .033, .065, .075, n=2.0)],
            weights={bone: 1}, mirror=True, fuse=False, segments=12, subdiv=2,
            cap_start='flat', cap_end='flat', tag='hoof'))
    return parts


def body_parts(bones, variant):
    # Author in adult metres, then change each ring and its cross-section together.
    adult = skeleton('male')
    torso = chain(adult, 'hips', 'spine1', 'spine2', 'chest')
    parts = [Part('torso', [R((-1.42, 0, 2.42), .17, .25, .34),
                           R((-1.12, 0, 2.38), .40, .38, .54),
                           R((-.62, 0, 2.36), .46, .49, .57),
                           R((.0, 0, 2.44), .47, .56, .64),
                           R((.55, 0, 2.55), .43, .68, .78),
                           R((.90, 0, 2.66), .31, .58, .74),
                           R((1.07, 0, 2.77), .21, .32, .42)],
                  chain=torso, segments=32, cap_scale=.4)]
    neck_chain = chain(adult, 'chest', 'neck1', 'neck2', 'neck3', 'neck4', 'neck5', 'head')
    parts.append(Part('neck_root', [R((.64, 0, 2.89), .37, .47, .48),
                                    R((.94, 0, 3.18), .29, .36, .34),
                                    R((1.06, 0, 3.46), .22, .30, .28),
                                    R((1.10, 0, 3.65), .17, .34, .29),
                                    R((1.14, 0, 3.80), .13, .27, .26),
                                    R((1.16, 0, 3.92), .09, .15, .13)],
                      chain=neck_chain, segments=28, cap_end='flat', tag='neck'))
    for front in (True, False):
        label = 'front' if front else 'hind'
        labels = ('scapula.L', 'upper_arm.L', 'forearm.L', 'hand.L') if front else (
            'thigh.L', 'shin.L', 'foot.L')
        limb = chain(adult, *labels)
        # Bury the final pastern ring by two remesh voxels to keep the hoof contact closed.
        if front:
            rings = [R((.70, .23, 2.75), .16, .22), R((.62, .35, 2.32), .17, .22),
                     R((.59, .39, 2.05), .12, .16),
                     R((.55, .39, 1.52), .085, .10), R((.54, .39, 1.39), .12, .095),
                     R((.58, .39, 1.23), .075, .07), R((.73, .39, .65), .060, .060),
                     R((.80, .39, .30), .093, .095), R((.85, .39, .22), .065, .070),
                     R((.89, .39, .105), .060, .065)]
            x = .90
        else:
            rings = [R((-1.02, .21, 2.46), .17, .23), R((-.93, .31, 2.13), .19, .27),
                     R((-.75, .39, 1.75), .13, .20),
                     R((-.67, .39, 1.55), .105, .13), R((-.95, .39, 1.05), .065, .09),
                     R((-1.09, .39, .80), .095, .11), R((-1.04, .39, .40), .055, .06),
                     R((-1.02, .39, .28), .083, .085), R((-1.00, .39, .20), .060, .060),
                     R((-.98, .39, .105), .058, .060)]
            x = -.98
        parts.append(Part(label+'_leg.L', rings, chain=limb, mirror=True, segments=22,
                          tag='leg', cap_start='flat', cap_end='flat'))
        parts.extend(cloven_hoof(label+'_hoof', x, 'fingers.L' if front else 'toes.L'))
    tail_chain = chain(adult, 'tail1', 'tail2', 'tail3', 'tail4')
    tail_points = [p[1] for p in tail_chain] + [tail_chain[-1][2]]
    parts.append(Part('tail', [R(p, .035-i*.004, .035-i*.004) for i, p in enumerate(tail_points)],
                      chain=tail_chain, segments=12, tag='tail'))
    parts.append(Part('tassel', [R((-1.68,0,1.27),.03,.04), R((-1.70,0,1.03),.055,.060),
                               R((-1.73,0,.81),.035,.045), R((-1.75,0,.69),.006,.009)],
                      weights={'tail4':1}, segments=12, subdiv=3, fuse=False,
                      tag='hair', rough=(.008,70)))
    for strand in range(7):
        angle = 2 * math.pi * strand / 7
        y = .038 * math.sin(angle)
        x = -1.71 + .035 * math.cos(angle)
        parts.append(Part(f'tassel_lock.{strand}', [R((x, y, 1.08), .013, .016),
                          R((x - .025, y * .9, .86), .012, .014),
                          R((x - .04, y * .65, .68 + .025 * math.sin(angle)), .001, .002)],
                          weights={'tail4':1}, segments=6, subdiv=2, fuse=False, tag='hair'))
    scan = scan_instance('male')['vertices']
    mane_rings = [R((.43, 0, 3.17), .012, .035, .025),
                  R((.71, 0, 3.40), .012, .040, .025)]
    for z in np.linspace(3.65, 4.90, 27):
        band = scan[(np.abs(scan[:, 2] - z) < .022) & (np.abs(scan[:, 1]) < .07)]
        x = float(band[:, 0].min())
        mane_rings.append(R((x + .025, 0, z), .012, .045, .030))
    parts.append(Part('mane', mane_rings, chain=neck_chain, segments=8, subdiv=2,
                      fuse=False, tag='mane', rough=(.005, 95)))
    for side in (-1,1):
        label = 'L' if side>0 else 'R'
        parts.append(Part('live_eye.'+label,
                          [R((1.767,side*.122,4.881),.026,.030),
                           R((1.807,side*.122,4.881),.026,.029)],
                          weights={'head':1},segments=16,fuse=False,tag='eye'))
        for i in range(5):
            x = 1.755+i*.015
            parts.append(Part(f'lash.{label}.{i}',
                              [R((x,side*.139,4.908),.002,.002),
                               R((x-.010,side*(.164+.003*i),4.918),.0006,.0006)],
                              weights={'head':1},segments=5,subdiv=1,fuse=False,tag='hair'))
    if variant != 'male':
        lean = .11 if variant == 'juvenile' else 0
        for side in (-1, 1):
            parts.append(Part(f'ossicone_tuft.{side}',
                              [R((1.78, side * .065, 5.12), .018, .018),
                               R((1.78 - lean * .6, side * .065, 5.17), .025, .025),
                               R((1.78 - lean, side * .065, 5.20), .005, .006)],
                              weights={'head': 1}, segments=10, subdiv=2, fuse=False,
                              cap_end='point', cap_scale=.3, tag='hair', rough=(.004, 100)))
            for lock in (-1, 0, 1):
                y = side * .065 + lock * .012
                parts.append(Part(f'ossicone_lock.{side}.{lock}',
                                  [R((1.78 - lean * .4, y, 5.15), .008, .009),
                                   R((1.78 - lean - .005 * lock, y, 5.20), .0008, .001)],
                                  weights={'head': 1}, segments=5, subdiv=1, fuse=False,
                                  cap_start='flat', cap_end='point', cap_scale=.2, tag='hair'))
    by_name = {b.name:b for b in bones}
    for part in parts:
        for ring in part.rings:
            before = ring.c.copy()
            ring.c = proportions(before, variant)
            scale = .46 if variant == 'juvenile' else 4.5/5.2 if variant == 'female' else 1
            if variant == 'juvenile' and part.tag == 'eye':
                scale *= 1.22
            ring.ry *= scale * (.91 if variant == 'female' else 1)
            ring.up *= scale
            ring.dn *= scale
        if part.chain:
            part.chain = [(name, by_name[name].head, by_name[name].tail) for name, _, _ in part.chain]
    return parts


def coat_at(points, variant='male'):
    """A continuous reticulated coat in animal space, independent of mesh seams."""
    p = np.asarray(points)
    leg = 1 - nz.smoothstep(1.05, 1.65, p[..., 2])
    face = nz.smoothstep(4.55, 4.85, p[..., 2])
    # Integrate the change in frequency, avoiding a sudden patch-scale ring.
    q = p.copy()
    q[..., 2] -= .9 * np.maximum(1.65 - p[..., 2], 0) ** 2
    q += 2.5 * face[..., None] * (p - np.array([1.6, 0, 4.65]))
    f1, f2, cell = nz.worley(q, 3.5, seed=38, jitter=.83)
    patch_mask = nz.smoothstep(.045, .075, f2 - f1)
    cream = np.array([235, 222, 195]) / 255
    patch = np.array([165, 95, 50]) / 255
    patch = patch * (.82 + .20 * cell[..., None])
    patch *= (1 - .10 * nz.smoothstep(.18, .55, f2 - f1))[..., None]
    if variant == 'juvenile':
        patch = nz.mix(patch, [ .80, .57, .33 ], .35)
    coat = nz.mix(cream, patch, patch_mask)
    coat = nz.mix(coat, cream, leg)
    belly = (1 - nz.smoothstep(1.88, 2.04, p[..., 2])) * (1 - leg)
    belly *= 1 - nz.smoothstep(.18, .39, np.abs(p[..., 1]))
    coat = nz.mix(coat, cream, belly * .75)
    fur = .98 + .04 * nz.fbm(p * np.array([1, 1, 8]), 105, 2, seed=3)
    return coat * fur[..., None]


def surface_coat(points, variant):
    """Facial leather and dark bald tips replace the taxidermy's baked lighting."""
    p = np.asarray(points)
    coat = coat_at(p, variant)
    muzzle = nz.smoothstep(1.94, 2.09, p[..., 0]) * (1 - nz.smoothstep(4.63, 4.77, p[..., 2]))
    coat = nz.mix(coat, [.66, .48, .29], muzzle)
    lip = muzzle * (1 - nz.smoothstep(4.57, 4.61, p[..., 2]))
    coat = nz.mix(coat, [.19, .12, .075], lip * .8)
    nostril_distance = ((p[..., 0] - 2.005) / .040) ** 2
    nostril_distance += ((np.abs(p[..., 1]) - .067) / .032) ** 2
    nostril_distance += ((p[..., 2] - 4.625) / .025) ** 2
    nostril = 1 - nz.smoothstep(.45, 1.0, nostril_distance)
    coat = nz.mix(coat, [.095, .055, .032], nostril)
    eye_distance = ((p[..., 0] - 1.787) / .061) ** 2
    eye_distance += ((np.abs(p[..., 1]) - .128) / .044) ** 2
    eye_distance += ((p[..., 2] - 4.881) / .055) ** 2
    coat = nz.mix(coat, [.57, .37, .20], (1 - nz.smoothstep(.5, 1.5, eye_distance)) * .8)
    ear = ((p[..., 0] - 1.71) / .12) ** 2
    ear += ((np.abs(p[..., 1]) - .26) / .13) ** 2
    ear += ((p[..., 2] - 4.99) / .032) ** 2
    coat = nz.mix(coat, [.88, .77, .59], (1 - nz.smoothstep(.4, 1.1, ear)) * .8)
    # Colour the scan's existing mane ridge as well as the added hairs. Leaving the
    # ridge coat-coloured makes holes appear between the scanned and modelled strands.
    back = np.interp(p[..., 2], [3.2, 3.5, 3.7, 3.9, 4.1, 4.3, 4.5, 4.7, 4.9],
                     [.60, .77, .83, .97, 1.09, 1.18, 1.26, 1.35, 1.49])
    mane = 1 - nz.smoothstep(back + .015, back + .055, p[..., 0])
    mane *= 1 - nz.smoothstep(.025, .075, np.abs(p[..., 1]))
    mane *= nz.smoothstep(3.15, 3.30, p[..., 2]) * (1 - nz.smoothstep(4.87, 4.96, p[..., 2]))
    coat = nz.mix(coat, [.21, .105, .045], mane)
    horn = nz.smoothstep(5.075, 5.15, p[..., 2])
    coat = nz.mix(coat, [.17, .105, .065], horn)
    return coat


def paint(ctx, variant='male'):
    coat = surface_coat(adult_space(ctx.P, variant), variant)
    coat = nz.mix(coat, [.115, .083, .060], ctx.tag('hoof'))
    coat = nz.mix(coat, [.028, .021, .014], ctx.tag('hair'))
    coat = nz.mix(coat, [.21, .105, .045], ctx.tag('mane'))
    coat = nz.mix(coat, [.003, .002, .001], ctx.tag('eye'))
    return coat


def scan_instance(variant):
    source = read_source(str(SOURCE))
    p = source['vertices'].astype(float).copy()
    # Undo the mount's lateral lean along its centre line, preserving local girth.
    p[:,1] -= .20 + .20*p[:,2]
    p[:,0] -= np.interp(p[:,2],[-2,-1.5,-1,-.55,0],[.20,.13,.04,0,0])
    p = p @ rot_y(22).T * .88
    p += [1.68, 0, 0]
    p[:,2] += 5.2-p[:,2].max()
    # Level the mounted head's roll and yaw without changing its facial proportions.
    center = np.array([1.78,0,4.85])
    leveled = (p-center) @ (rot_z(5) @ rot_x(5)).T + center
    leveled[:,1] -= .015
    blend = nz.smoothstep(4.5,4.8,p[:,2])
    p = p*(1-blend[:,None])+leveled*blend[:,None]
    p[:,2] += 5.2-p[:,2].max()
    # The scan is a male mount. Female and calf ossicones are thinner, tufted.
    if variant != 'male':
        horn = p[:,2] > 5.02
        centers = np.where(p[horn,1]>0,.065,-.065)
        p[horn,1] = centers+.65*(p[horn,1]-centers)
        p[horn,0] = 1.78+.75*(p[horn,0]-1.78)
    if variant == 'juvenile':
        horn = nz.smoothstep(5.01, 5.2, p[:, 2])
        p[:, 0] -= .11 * horn
        ear = nz.smoothstep(.15, .24, np.abs(p[:, 1])) * nz.smoothstep(4.78, 4.92, p[:, 2])
        p[:, 1] *= 1 + .25 * ear
    # Trim the irregular cut well inside the replacement neck root, not at its skin.
    keep = (p[source['triangles'],2] > 3.66).all(axis=1)
    return dict(source, vertices=proportions(p, variant),
                triangles=source['triangles'][keep], uv=source['uv'][keep])


def make_spec(variant='male'):
    name = 'giraffe' if variant == 'male' else 'giraffe_'+variant
    bones = skeleton(variant)
    rig = Rig('hips', {
        'FL':Leg('upper_arm.L','forearm.L','hand.L','fingers.L','scapula.L'),
        'FR':Leg('upper_arm.R','forearm.R','hand.R','fingers.R','scapula.R'),
        'HL':Leg('thigh.L','shin.L','foot.L','toes.L'),
        'HR':Leg('thigh.R','shin.R','foot.R','toes.R')}, spine=['spine1','spine2'],
        chest='chest', neck=[f'neck{i}' for i in range(1,6)], head='head', jaw='jaw',
        tail=[f'tail{i}' for i in range(1,5)], ears=['ear.L','ear.R'])
    height = HEIGHTS[variant]
    meta = dict(common_name='Giraffe', scientific_name='Giraffa reticulata',
                shoulder_height_m=height, withers_height_m=float(proportions([0,0,3.3],variant)[2]),
                body_length_m=4.3*height/5.2, roughness=.94, asset_license='CC-BY-4.0',
                source_author='aellis43',
                source_url='https://sketchfab.com/3d-models/673b69aaafc14d94940a862225794f01',
                coat_identity=('Reticulated pattern repainted over the complete animal; '
                               'scan identity unverified'),
                fast_gait='Moderate rotary gallop exported under the required name trot',
                activity_clips={'browse': 'eat'})
    if variant != 'male':
        meta.update(variant_of='giraffe',variant=variant,relative_height=height/5.2)
    spec = Spec(name,bones,body_parts(bones,variant),paint=partial(paint,variant=variant),texture_size=2048,
                voxel=.020*(height/5.2),target_tris=10500 if variant!='juvenile' else 6000,
                smooth_iters=4,meta=meta,rig=rig,key_all_locations=True,validate_mesh=True)
    rig.extra['lie_drop'] = float(geom.loft(spec.parts[0])[0][:,2].min()) - .018*height/5.2
    spec.sculpt = dict(scan_instance(variant), giraffe2=True, rigged=True, variant=variant)
    spec.clips = make_clips
    return spec


def make_clips(solver):
    scale = solver.head['head'][2]/4.82
    rig = solver.rig
    gaits = []
    for name, speed, duty, period, offsets in [
            ('walk',1.2*math.sqrt(scale),.68,1.8*math.sqrt(scale),quad.PACE),
            ('trot',2.7*math.sqrt(scale),.45,1.05*math.sqrt(scale),quad.GALLOP)]:
        gait = quad.Gait(name,period,speed*duty*period,duty,offsets,lift=.12*scale,
                         bob=.018*scale,hip_yaw=1.2,chest_yaw=1.0,sway=.014*scale,
                         roll=.5,heel_off=8,swing_pitch=20,land_pitch=0,toe_follow=.25,
                         scapula=.25,head_nod=0,tail_sway=4)
        clip = quad.gait_clip(solver,gait)
        original = clip.pose
        def gait_pose(u, original=original, amplitude=1.8 if name=='walk' else 4.0):
            pose = original(u)
            pose.r('neck1',ry=amplitude*math.sin(2*math.pi*u))
            pose.r('head',ry=-amplitude*.7*math.sin(2*math.pi*u+.3))
            return pose
        clip.pose = gait_pose
        gaits.append(clip)

    def idle(u):
        a = 2*math.pi*u
        pose = quad.stand(solver,Pose().move('hips',dz=-.025*scale))
        pose.r('neck3',rz=2.8*math.sin(a))
        pose.r('head',rz=5*math.sin(a),ry=1.2*math.sin(2*a))
        pose.r('jaw',rz=2.3*math.sin(7*a),ry=1.5*(1+math.sin(7*a)))
        pose.r('ear.L',rx=12*math.sin(3*a)**9)
        pose.r('ear.R',rx=-14*math.sin(5*a+.8)**9)
        pose.r('tail2',rz=8*math.sin(a)+8*math.sin(3*a)**7)
        pose.r('tail4',rz=12*math.sin(3*a-.5))
        return pose

    def eat(u):
        pose = idle(u)
        pose.r('neck1',ry=-8)
        pose.r('neck4',ry=-6)
        pose.r('head',ry=-25)
        return pose

    hd = quad.HeadDown(body_pitch=22,crouch=(.55 if scale < .5 else .08)*scale,chest=0,
                       neck=(115,10,0,0,0),head=-150,splay=.85*scale)
    effort = quad.bisect_effort(
        lambda k:quad.muzzle(solver,quad.head_down_pose(solver,hd,k))[2],.045*scale,'giraffe drink')

    def drink(u):
        pose = quad.head_down_pose(solver,hd,effort)
        pose.r('jaw',rz=.4*math.sin(8*math.pi*u))
        return pose

    lying = quad.Lie(drop=rig.extra['lie_drop'],hind_tuck=.18*scale,hind_out=.14*scale,
                    front_out=.08*scale,tail_floor=.11*scale,foot_lift=.025*scale,
                    front_elbow_out=.20)
    rest_base = quad.lie_pose(solver,lying)

    def rest(u):
        pose = Pose()
        pose.rot = dict(rest_base.rot)
        pose.offset = dict(rest_base.offset)
        pose.feet = dict(rest_base.feet)
        pose.r('head',rz=4*math.sin(2*math.pi*u))
        pose.r('ear.L',rx=8*math.sin(6*math.pi*u)**7)
        return pose

    def sleep(u):
        pose = rest(u)
        first_angle = -80 if scale < .5 else -67
        for name, angle in zip(rig.neck,(first_angle,-20,-20,-25,-25)):
            pose.r(name,ry=angle)
        pose.r('head',ry=170 if scale < .5 else 80-first_angle,
               rz=180 if scale < .5 else 8)
        return pose

    def vocalize(u):
        pose = idle(u)
        jerk = math.sin(math.pi*u)**6
        pose.r('neck4',ry=-4*jerk)
        pose.r('head',ry=-8*jerk)
        return pose

    return [Clip('idle',8,idle),*gaits,Clip('eat',5,eat),Clip('drink',5,drink),
            Clip('rest',8,rest),Clip('sleep',8,sleep),Clip('vocalize',2,vocalize,loop=False)]


def scan_weights(vertices, bones, variant):
    names = [b.name for b in bones]
    w = np.zeros((len(vertices),len(bones)))
    neck = [(b.name,b.head,b.tail) for b in bones if b.name in
            ['chest','neck1','neck2','neck3','neck4','neck5','head']]
    reference = adult_space(vertices, variant)
    for i,p in enumerate(vertices):
        adult = reference[i]
        influences = geom.chain_weights(p,neck,.8)
        skull = max(float(nz.smoothstep(1.48,1.70,adult[0]) *
                          nz.smoothstep(4.45,4.62,adult[2])),
                    float(nz.smoothstep(4.75,4.90,adult[2])))
        influences = {name:value*(1-skull) for name,value in influences.items()}
        influences['head'] = influences.get('head',0)+skull
        if adult[2] > 4.84 and abs(adult[1]) > .18:
            influences = {'ear.L' if p[1]>0 else 'ear.R':1.0}
        if adult[0] > 1.80 and 4.50 < adult[2] < 4.67:
            influences = {'jaw':.85,'head':.15}
        for name,value in influences.items():
            w[i,names.index(name)] = value
    return w


def trim_graft(mesh, height, keep_above, tags=None, tag_names=None):
    """Cut just the neck skin; preserve facial additions, hair and interpolated weights."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.verts.ensure_lookup_table()
    if tags is not None:
        for column, name in enumerate(tag_names):
            layer = bm.verts.layers.float.new('coat_' + name)
            for vertex in bm.verts:
                vertex[layer] = float(tags[vertex.index, column])
        neck = bm.verts.layers.float['coat_neck']
        faces = [face for face in bm.faces if sum(v[neck] for v in face.verts) > .3 * len(face.verts)]
        vertices = {v for face in faces for v in face.verts}
        edges = {edge for face in faces for edge in face.edges}
        geometry = sorted(vertices, key=lambda v: v.index) + sorted(edges, key=lambda e: e.index) + faces
    else:
        geometry = list(bm.verts) + list(bm.edges) + list(bm.faces)
    bmesh.ops.bisect_plane(bm, geom=geometry, dist=1e-7, plane_co=(0, 0, height),
                          plane_no=(0, 0, 1), clear_inner=keep_above, clear_outer=not keep_above)
    if keep_above:
        small_holes = [edge for edge in bm.edges if edge.is_boundary
                       and all(height + 1e-5 < v.co.z < height + .25 for v in edge.verts)]
        bmesh.ops.holes_fill(bm, edges=small_holes, sides=12)
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()


def stitch_graft(mesh, lower, upper):
    """Zip the two open neck rings, making a single skin with shared smooth normals."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(mesh)
    rings = []
    centers = []
    parameters = []
    for height in (lower, upper):
        edges = [edge for edge in bm.edges if edge.is_boundary
                 and all(abs(v.co.z - height) < 1e-5 for v in edge.verts)]
        vertices = sorted(set(v for edge in edges for v in edge.verts), key=lambda v: v.index)
        adjacency = {vertex: [] for vertex in vertices}
        for edge in edges:
            first, second = edge.verts
            adjacency[first].append(second)
            adjacency[second].append(first)
        if len(vertices) < 8 or any(len(adjacent) != 2 for adjacent in adjacency.values()):
            raise ValueError(f'Incomplete giraffe graft ring at {height}')
        # Follow topology, not polar angle: the scanned mane makes the ring non-convex.
        remaining = set(vertices)
        loops = []
        while remaining:
            start = min(remaining, key=lambda v: v.index)
            ordered = [start]
            previous = None
            current = start
            while True:
                following = min((v for v in adjacency[current] if v != previous), key=lambda v: v.index)
                if following == start:
                    break
                ordered.append(following)
                previous, current = current, following
            remaining.difference_update(ordered)
            loops.append(ordered)
        ordered = max(loops, key=len)
        other = set(v for loop in loops if loop is not ordered for v in loop)
        if other:
            bmesh.ops.holes_fill(bm, edges=[edge for edge in edges if edge.verts[0] in other], sides=0)
        start_index = max(range(len(ordered)), key=lambda i: ordered[i].co.x)
        ordered = ordered[start_index:] + ordered[:start_index]
        area = sum(v.co.x * ordered[(i + 1) % len(ordered)].co.y
                   - v.co.y * ordered[(i + 1) % len(ordered)].co.x for i, v in enumerate(ordered))
        if area < 0:
            ordered = ordered[:1] + list(reversed(ordered[1:]))
        center = sum((np.array(v.co) for v in ordered)) / len(ordered)
        centers.append(center)
        rings.append(ordered)
        lengths = [float((ordered[(i + 1) % len(ordered)].co - v.co).length)
                   for i, v in enumerate(ordered)]
        parameters.append(np.r_[0, np.cumsum(lengths)] / sum(lengths))
    i, j = 0, 0
    bottom, top = rings
    while i < len(bottom) or j < len(top):
        next_bottom = parameters[0][min(i + 1, len(bottom))]
        next_top = parameters[1][min(j + 1, len(top))]
        if i < len(bottom) and (j == len(top) or next_bottom <= next_top):
            vertices = [bottom[i % len(bottom)], bottom[(i + 1) % len(bottom)], top[j % len(top)]]
            i += 1
        else:
            vertices = [bottom[i % len(bottom)], top[(j + 1) % len(top)], top[j % len(top)]]
            j += 1
        positions = np.array([v.co for v in vertices])
        center = positions.mean(0)
        axis = centers[0] + (centers[1] - centers[0]) * ((center[2] - lower) / (upper - lower))
        normal = np.cross(positions[1] - positions[0], positions[2] - positions[0])
        if np.dot(normal, center - axis) < 0:
            vertices.reverse()
        face = bm.faces.new(vertices)
        face.smooth = True
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()


def loop_frames(mesh, uv_name):
    """Corner tangent frames let the baked scan survive a new equal-density UV layout."""
    mesh.calc_tangents(uvmap=uv_name)
    count = len(mesh.loops)
    tangent = np.empty((count, 3), dtype=np.float32)
    normal = np.empty_like(tangent)
    sign = np.empty(count, dtype=np.float32)
    mesh.loops.foreach_get('tangent', tangent.ravel())
    mesh.loops.foreach_get('normal', normal.ravel())
    mesh.loops.foreach_get('bitangent_sign', sign)
    bitangent = np.cross(normal, tangent) * sign[:, None]
    return np.stack([tangent, bitangent, normal], axis=-1).reshape(-1, 3, 3, 3)


def save_map(name, pixels, normal=False):
    import bpy
    size = pixels.shape[0]
    image = bpy.data.images.new(name, size, size, alpha=False)
    if normal:
        image.colorspace_settings.name = 'Non-Color'
    rgba = np.concatenate([pixels, np.ones((size, size, 1))], axis=-1).astype(np.float32)
    image.pixels.foreach_set(rgba.ravel())
    output = Path.cwd() / 'build-probe'
    output.mkdir(parents=True, exist_ok=True)
    image.filepath_raw = str(output / (name + '.png'))
    image.file_format = 'PNG'
    image.save()
    saved = bpy.data.images.load(image.filepath_raw, check_existing=False)
    if normal:
        saved.colorspace_settings.name = 'Non-Color'
    bpy.data.images.remove(image)
    return saved


def build_giraffe(spec):
    """Paint one surface-space coat and preserve scan detail in a shared density atlas."""
    import bpy
    from . import build
    from .rigged import mesh_from_data
    data = spec.sculpt
    captured = []

    def capture(ctx):
        # Only retain vertex tags; painting happens once in the final combined atlas.
        captured.append((ctx._tags, ctx._tag_names))
        return np.full((ctx.size, ctx.size, 3), .5)

    body, info = build.build_mesh(replace(spec, sculpt=None, texture_size=32, paint=capture))
    print(f'GIRAFFE {spec.name}: body constructed; stitching neck', flush=True)
    tags, tag_names = captured[0]
    lower = float(proportions([0, 0, 3.64], data['variant'])[2])
    upper = float(proportions([0, 0, 3.82], data['variant'])[2])
    trim_graft(body.data, lower, False, tags, tag_names)
    body_faces = len(body.data.polygons)
    body.data.uv_layers.active.name = 'source_detail'
    scan = mesh_from_data(data, spec.name + '_scan')
    scan.data.uv_layers.active.name = 'source_detail'
    weights = scan_weights(data['vertices'], info['bones'], data['variant'])
    for i, name in enumerate(info['bone_names']):
        group = scan.vertex_groups.new(name=name)
        for vertex in np.flatnonzero(weights[:, i] > 0):
            group.add([int(vertex)], float(weights[vertex, i]), 'REPLACE')
    if data['variant'] == 'juvenile':
        build._activate(scan)
        modifier = scan.modifiers.new('calf_scan_budget', 'DECIMATE')
        modifier.ratio = .70
        modifier.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    trim_graft(scan.data, upper, True)
    scan_faces_end = body_faces + len(scan.data.polygons)
    mat = body.data.materials[0]
    scan.data.materials.append(mat)
    build._activate(body)
    scan.select_set(True)
    bpy.ops.object.join()
    mesh = body.data
    stitch_graft(mesh, lower, upper)
    mesh.materials.clear()
    mesh.materials.append(mat)
    for poly in mesh.polygons:
        poly.material_index = 0
    old_uv = np.empty((len(mesh.loops), 2), dtype=np.float32)
    mesh.uv_layers.active.data.foreach_get('uv', old_uv.ravel())
    old_frames = loop_frames(mesh, 'source_detail')
    print(f'GIRAFFE {spec.name}: {len(mesh.polygons)} faces; unwrapping shared atlas', flush=True)
    mesh.uv_layers.new(name='giraffe_atlas')
    mesh.uv_layers.active_index = len(mesh.uv_layers) - 1
    mesh.uv_layers.active.active_render = True
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=.003)
    bpy.ops.uv.average_islands_scale()
    bpy.ops.uv.pack_islands(margin=.003, rotate=True)
    bpy.ops.object.mode_set(mode='OBJECT')
    new_frames = loop_frames(mesh, 'giraffe_atlas')
    uv = np.empty_like(old_uv)
    mesh.uv_layers.active.data.foreach_get('uv', uv.ravel())
    indices = np.empty(len(mesh.loops), dtype=np.int32)
    mesh.loops.foreach_get('vertex_index', indices)
    triangles = indices.reshape(-1, 3)
    vertices = np.empty((len(mesh.vertices), 3), dtype=np.float32)
    mesh.vertices.foreach_get('co', vertices.ravel())
    vertices = build.from_blender(vertices)
    all_tags = np.zeros((len(vertices), len(tag_names)), dtype=np.float32)
    for column, name in enumerate(tag_names):
        attribute = mesh.attributes['coat_' + name]
        values = np.empty(len(vertices), dtype=np.float32)
        attribute.data.foreach_get('value', values)
        all_tags[:, column] = values
    size = 2048
    print(f'GIRAFFE {spec.name}: painting coat and transferring normal detail', flush=True)
    tri_id, bary = build._rasterize(uv.reshape(-1, 3, 2), size)
    valid = tri_id >= 0
    filled = np.flatnonzero(valid)
    flat_id = tri_id.ravel()
    flat_bary = bary.reshape(-1, 3)
    rgb = np.zeros((size * size, 3), dtype=np.float32)
    normals = np.full_like(rgb, [.5, .5, 1])
    source = bpy.data.images.load(str(SOURCE.parent / 'normal.png'), check_existing=False)
    source.colorspace_settings.name = 'Non-Color'
    source_pixels = np.empty(source.size[0] * source.size[1] * 4, dtype=np.float32)
    source.pixels.foreach_get(source_pixels)
    source_pixels = source_pixels.reshape(source.size[1], source.size[0], 4)
    old_uv = old_uv.reshape(-1, 3, 2)
    tag_colors = {'hoof': [.115, .083, .060], 'hair': [.028, .021, .014],
                  'mane': [.21, .105, .045], 'eye': [.003, .002, .001]}
    for start in range(0, len(filled), 65536):
        at = filled[start:start + 65536]
        faces = flat_id[at]
        bc = flat_bary[at]
        ids = triangles[faces]
        point = np.sum(vertices[ids] * bc[..., None], axis=1)
        reference = adult_space(point, data['variant'])
        color = surface_coat(reference, data['variant'])
        for tag, value in tag_colors.items():
            if tag in tag_names:
                mask = np.sum(all_tags[ids, tag_names.index(tag)] * bc, axis=1)
                color = nz.mix(color, value, mask)
        rgb[at] = color
        basis = np.sum(new_frames[faces] * bc[..., None, None], axis=1)
        for column in range(3):
            basis[:, :, column] /= np.maximum(np.linalg.norm(basis[:, :, column], axis=1), 1e-8)[:, None]
        # Short fur is sampled in metres, so its scale cannot change at the graft or a UV seam.
        grain = np.stack([nz.fbm(point * [1, 1, 5], 115, 2, seed=seed) - .5
                          for seed in (14, 29, 47)], axis=-1) * .16
        world = basis[:, :, 2] + build.to_blender(grain)
        scanned = (faces >= body_faces) & (faces < scan_faces_end)
        if scanned.any():
            scan_faces = faces[scanned]
            scan_bc = bc[scanned]
            old = np.sum(old_uv[scan_faces] * scan_bc[..., None], axis=1)
            sx = np.clip((old[:, 0] * source.size[0]).astype(int), 0, source.size[0] - 1)
            sy = np.clip((old[:, 1] * source.size[1]).astype(int), 0, source.size[1] - 1)
            detail = source_pixels[sy, sx, :3] * 2 - 1
            old_basis = np.sum(old_frames[scan_faces] * scan_bc[..., None, None], axis=1)
            detail_world = np.einsum('nij,nj->ni', old_basis, detail)
            blend = .45 * nz.smoothstep(3.72, 4.2, reference[scanned, 2])
            world[scanned] += (detail_world - old_basis[:, :, 2]) * blend[:, None]
        world /= np.maximum(np.linalg.norm(world, axis=1), 1e-8)[:, None]
        mapped = np.einsum('nji,nj->ni', basis, world)
        mapped /= np.maximum(np.linalg.norm(mapped, axis=1), 1e-8)[:, None]
        normals[at] = mapped * .5 + .5
    rgb = build._dilate(rgb.reshape(size, size, 3), valid)
    normals = build._dilate(normals.reshape(size, size, 3), valid)
    base = save_map(spec.name + '_base', rgb)
    normal = save_map(spec.name + '_normal', normals, normal=True)
    # Export a single UV set. The old photo layout has served only to transfer normal detail.
    mesh.uv_layers.remove(mesh.uv_layers['source_detail'])
    shader = mat.node_tree.nodes['Principled BSDF']
    for node in mat.node_tree.nodes:
        if node.type == 'TEX_IMAGE':
            node.image = base
    tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
    tex.image = normal
    nmap = mat.node_tree.nodes.new('ShaderNodeNormalMap')
    nmap.inputs['Strength'].default_value = .65
    mat.node_tree.links.new(tex.outputs['Color'], nmap.inputs['Color'])
    mat.node_tree.links.new(nmap.outputs['Normal'], shader.inputs['Normal'])
    mesh.update()
    info.update(triangles=len(mesh.polygons), vertices=len(mesh.vertices), image=base)
    return body, info


def habitat_stage(scene, height):
    """Warm sun at 55 degrees, blue fill, and the habitat's sRGB 200/185/150 sand."""
    import bpy
    from mathutils import Vector
    from . import preview
    cam, ground = preview.setup_stage(scene, height * .6)
    ground.scale *= 10
    preview.sculpt_lighting(scene)
    scene.render.threads = 3
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.66, .79, 1, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .45
    sun = bpy.data.objects['sun']
    sun.data.energy = 3
    sun.data.color = (1, .91, .78)
    sun.rotation_euler = Vector((-.48, .31, -math.sin(math.radians(55)))).to_track_quat('-Z', 'Y').to_euler()
    sand = [((value / 255 + .055) / 1.055) ** 2.4 for value in (200, 185, 150)]
    shader = ground.data.materials[0].node_tree.nodes['Principled BSDF']
    shader.inputs['Base Color'].default_value = (*sand, 1)
    return cam


def model_previews(scene, arm, spec, clips, args):
    """Visitor-distance evidence, anatomical close-ups, and six-frame clip strips."""
    import bpy
    from . import anim, build, preview
    out = Path(args.preview)
    out.mkdir(parents=True,exist_ok=True)
    prefix = str(out/('S11b_final_'+spec.name))
    height = spec.meta['shoulder_height_m']
    center = build.to_blender((.20,0,height*.5))
    radius = height*.60
    cam = habitat_stage(scene, height)
    anim.clear_pose(arm)
    if not args.preview_clips_only:
        preview.render_views(scene,cam,center,radius,prefix,res=(600,600))
        scene.render.resolution_x,scene.render.resolution_y = 1600,1100
        scene.render.image_settings.file_format = 'PNG'
        cam.data.type='PERSP'
        cam.data.lens=48
        cam.location=(7.2,-9.6,1.6)
        preview._look_at(cam,center)
        scene.render.filepath=prefix+'_eye.png'
        bpy.ops.render.render(write_still=True)
        cam.location=(18,-24,1.6)
        preview._look_at(cam,center)
        scene.render.filepath=prefix+'_eye30.png'
        bpy.ops.render.render(write_still=True)
        cam.data.type='ORTHO'
        cam.data.ortho_scale=height*1.55
        cam.location=(12,-.20,height*.5)
        preview._look_at(cam,center)
        scene.render.filepath=prefix+'_side.png'
        bpy.ops.render.render(write_still=True)
        if spec.name=='giraffe':
            cam.data.type='PERSP'
            cam.data.lens=70
            for suffix,location,target in [
                    ('face_below',(1.5,-4.3,1.6),(0,-1.95,4.8)),
                    ('face_level',(2.0,-4.3,4.85),(0,-1.95,4.8)),
                    ('join',(2.5,-3.4,3.25),(0,-.9,3.40)),
                    ('feet',(1.8,-2.7,.48),(0,-.55,.40))]:
                cam.location=location
                preview._look_at(cam,target)
                scene.render.filepath=prefix+'_'+suffix+'.png'
                bpy.ops.render.render(write_still=True)
            tiles=[]
            for suffix in ('face_below','face_level'):
                im=bpy.data.images.load(prefix+'_'+suffix+'.png')
                tiles.append(np.array(im.pixels[:],dtype=np.float32).reshape(1100,1600,4))
                bpy.data.images.remove(im)
            preview.save_sheet(tiles,2,(1600,1100),prefix+'_face.png')
    cam.data.type='PERSP'
    cam.data.lens=70
    want=args.clips.split(',') if args.clips else [c['name'] for c in clips]
    for clip in clips:
        if clip['name'] in want:
            samples = scene.cycles.samples
            scene.cycles.samples = 6
            anim.preview_clip(scene,cam,arm,spec.name,clip,center,radius,
                              prefix+'_'+clip['name']+'_strip',phases=6,res=(400,350))
            scene.cycles.samples = samples
            if spec.name=='giraffe' and clip['name'] in ('drink','rest'):
                # preview_clip leaves its final sampled pose active.
                scene.render.resolution_x,scene.render.resolution_y=1600,1100
                cam.data.type='ORTHO'
                cam.data.ortho_scale=7.2
                cam.location=(10,-.3,2.1)
                preview._look_at(cam,(0,-.3,1.8))
                scene.render.filepath=prefix+'_'+clip['name']+'.png'
                bpy.ops.render.render(write_still=True)
                cam.data.type='PERSP'
                if clip['name']=='rest':
                    cam.data.lens=48
                    cam.location=(16,-12,1.6)
                    preview._look_at(cam,(0,-.1,1.5))
                    scene.render.filepath=prefix+'_rest20.png'
                    bpy.ops.render.render(write_still=True)
