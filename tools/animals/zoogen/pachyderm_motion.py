# SPDX-License-Identifier: MIT
"""Contact-constrained pachyderm locomotion and species-specific trunk/body poses."""
import math
from functools import partial

import numpy as np

from . import activity, quadruped as quad
from .rig import Clip, Pose, rot_y, smooth

LATERAL = {'HL': 0.0, 'FL': .25, 'HR': .5, 'FR': .75}


def skin_points(solver, pose, data, indices=None):
    """Skin bind vertices in animal space, also used by independent ground checks."""
    world = solver.solve(pose)
    vertices = data['vertices'] if indices is None else data['vertices'][indices]
    weights = data['weights'] if indices is None else data['weights'][indices]
    points = np.zeros_like(vertices)
    for i, name in enumerate(data['bone_names']):
        rotation = world[name][:3, :3] @ solver.rest[name][:3, :3].T
        points += weights[:, i, None] * ((vertices - solver.head[name]) @ rotation.T + world[name][:3, 3])
    return points


def grounded(solver, pose, data):
    floor = skin_points(solver, pose, data)[:, 2].min()
    lift = .008 - floor
    pose.move('hips', dz=lift)
    for target in pose.feet.values():
        target.pos[2] += lift
    return pose


def ears_tail(pose, solver, phase, elephant, amount=1):
    a = 2 * math.pi * phase
    for side, sign in [('.L', 1), ('.R', -1)]:
        if elephant:
            flap = 11 * math.sin(2 * a + sign * .35) + 4 * math.sin(5 * a + sign)
            pose.r('ear' + side, rz=sign * amount * flap)
            pose.r('ear_tip' + side, rz=sign * amount * 6 * math.sin(2 * a + sign * .35 - .8))
        else:
            flick = quad.bump(phase, .22 if sign > 0 else .61, .04)
            pose.r('ear' + side, rz=sign * amount * 22 * flick, ry=-8 * amount * flick)
    for i, name in enumerate(solver.rig.tail):
        pose.r(name, rz=amount * (3 + i) * math.sin(3 * a - i * .65))
    return pose


def trunk_curl(pose, angles, sway=0):
    for i in range(10):
        pose.r('trunk' + str(i + 1), ry=angles[i], rz=sway * math.sin(i * .4))
    return pose


def trunk_to(solver, base, target, initial=None):
    """Bounded planar CCD distributes curvature over ten deform joints."""
    angles = np.array(initial if initial is not None else [12.] * 10, dtype=float)
    point = solver.tail['trunk10']
    for _ in range(60):
        pose = Pose()
        pose.rot, pose.offset, pose.feet = dict(base.rot), dict(base.offset), dict(base.feet)
        trunk_curl(pose, angles)
        world = solver.solve(pose)
        tip = solver.point(world, 'trunk10', point)
        if np.linalg.norm(tip - target) < .012:
            break
        for i in reversed(range(10)):
            joint = world['trunk' + str(i + 1)][:3, 3]
            current, desired = tip - joint, target - joint
            cross = current[2] * desired[0] - current[0] * desired[2]
            angle = math.degrees(math.atan2(cross, current[0] * desired[0] + current[2] * desired[2]))
            previous = angles[i]
            angles[i] = np.clip(previous + angle * .45, -48, 48)
            tip = joint + rot_y(angles[i] - previous) @ (tip - joint)
    return angles


def idle_pose(solver, data, phase):
    a = 2 * math.pi * phase
    pose = quad.stand(solver, Pose())
    pose.move('hips', dz=-.012 * data['height'] + .002 * data['height'] * math.sin(3 * a),
              dy=.003 * data['height'] * math.sin(2 * a))
    pose.r('chest', ry=.4 * math.sin(3 * a))
    elephant = data['kind'] == 'elephant'
    ears_tail(pose, solver, phase, elephant)
    if elephant:
        trunk_curl(pose, [0, 0, 0, 0, 1, 2, 4, 7, 9, 12], 1.5 * math.sin(a))
    else:
        pose.r('head', rz=2 * math.sin(a), ry=math.sin(3 * a))
    return pose


def gait_pose(solver, gait, drop, data, phase):
    pose = quad.gait_pose(solver, gait, phase, drop)
    elephant = data['kind'] == 'elephant'
    ears_tail(pose, solver, phase, elephant, .45)
    if elephant:
        trunk_curl(pose, [2 + 1.5 * math.sin(2 * math.pi * phase - i * .3) for i in range(10)])
    return pose


def elephant_feed(solver, data, angles_down, angles_up, drink, phase):
    pose = quad.stand(solver, Pose()).move('hips', dz=-.05 * data['height'])
    # Hold the gathered material/water at each end, with a smooth transport interval.
    u = phase % 1
    if u < .2:
        blend = 0
    elif u < .5:
        blend = smooth((u - .2) / .3)
    elif u < .7:
        blend = 1
    else:
        blend = 1 - smooth((u - .7) / .3)
    trunk_curl(pose, angles_down * (1-blend) + angles_up * blend)
    pose.r('jaw', ry=3 * blend * (1 - math.cos(12 * math.pi * phase)))
    if drink:
        pose.r('head', ry=-2 * blend)
    ears_tail(pose, solver, phase, True, .25)
    return pose


# Dust bathing, one toss to each side: [trunk down (0..1, scooping at the ground before the
# fore feet), trunk tossed (0..1, curled up over the forehead and to one side), side (+1 the
# left), fore foot scuffing loose earth (+1 the left, -1 the right)]. The side changes only
# while the trunk hangs at rest; toss and return are slow enough that no joint of the bull's
# trunk moves faster than 5 m/s.
DUST_BATHE = [
    (0.00, (0, 0, -1, 0), .16),
    (0.02, (0, 0, 1, 0), .02),
    (0.10, (1, 0, 1, 1), .08),
    (0.20, (1, 0, 1, 0), .04),
    (0.33, (0, 1, 1, 0), .13),
    (0.50, (0, 0, 1, 0), .16),
    (0.52, (0, 0, -1, 0), .02),
    (0.60, (1, 0, -1, -1), .08),
    (0.70, (1, 0, -1, 0), .04),
    (0.83, (0, 1, -1, 0), .13),
]
# The toss, per trunk joint from the face to the tip, fitted so that the tip ends above the
# forehead and to one side for bull, cow and calf: pitch (- raises) and sideways bend (degrees).
TOSS_PITCH = np.linspace(-48, -4, 10)
TOSS_BEND = np.linspace(35, -48, 10)
TRUNK_IDLE = np.array([0, 0, 0, 0, 1, 2, 4, 7, 9, 12], dtype=float)


def elephant_dust_bathe(solver, data, angles_scoop, phase):
    """`dust_bathe`: the elephant loosens the earth with a fore foot, gathers it with the trunk
    and throws it up over its head, once to each side, the ears flapping."""
    height = data['height']
    down, toss, side, scuff = activity.held(phase, DUST_BATHE)
    pose = quad.stand(solver, Pose()).move('hips', dz=-.025 * height * (1 + down))
    for key, sign in (('FL', 1), ('FR', -1)):
        amount = max(0.0, sign * scuff)
        if amount > 0:
            # Scraping strokes: forward, down and back along the ground, lifted to return.
            u = (phase * 10) % 1.0
            target = solver.toe_base(key)
            target[0] += amount * .09 * height * math.cos(math.tau * u)
            target[2] += amount * .03 * height * max(0.0, math.sin(math.tau * u))
            pose.plant(key, target)
    pitch = TRUNK_IDLE + (angles_scoop - TRUNK_IDLE) * down + (TOSS_PITCH - TRUNK_IDLE) * toss
    # Scooping: the tip works sideways at the ground.
    scoop = 2.5 * down * math.sin(8 * math.tau * phase)
    for i in range(10):
        pose.r('trunk' + str(i + 1), ry=pitch[i], rz=scoop * math.sin(i * .4) + side * toss * TOSS_BEND[i])
    pose.r('head', ry=-9 * toss, rz=5 * side * toss)
    ears_tail(pose, solver, phase, True, .5 + 1.2 * toss)
    return pose


def lateral_sleep(solver, data, phase):
    pose = Pose().r('hips', rx=88)
    for side in ('.L', '.R'):
        pose.r('upper_arm' + side, ry=12)
        pose.r('forearm' + side, ry=-20)
        pose.r('thigh' + side, ry=-12)
        pose.r('shin' + side, ry=25)
        pose.r('ear' + side, rz=55 if side == '.L' else -55)
    if data['kind'] == 'elephant':
        trunk_curl(pose, [3, 5, 9, 13, 16, 18, 20, 22, 24, 26])
    pose.r('chest', ry=.15 * math.sin(4 * math.pi * phase))
    return grounded(solver, pose, data)


def elephant_rest(solver, data, phase):
    if data['variant'] == 'juvenile':
        return lateral_sleep(solver, data, phase)
    pose = idle_pose(solver, data, phase)
    # A relaxed distal curl keeps the tip clear of the sand throughout the sway.
    trunk_curl(pose, [0, 0, 0, 0, 2, 6, 12, 20, 28, 32], .3 * math.sin(2 * math.pi * phase))
    position = solver.toe_base('HR') + [-.025 * data['height'], 0, .012 * data['height']]
    pose.plant('HR', position, foot_pitch=3)
    return pose


def call_pose(solver, data, phase):
    activity = math.sin(math.pi * phase) ** 2
    pose = quad.stand(solver, Pose()).move('hips', dz=-.012 * data['height'] - .025 * activity)
    pose.r('head', ry=-8 * activity)
    pose.r('jaw', ry=10 * activity)
    for side, sign in [('.L', 1), ('.R', -1)]:
        pose.r('ear' + side, rz=sign * 20 * activity)
    if data['kind'] == 'elephant':
        trunk_curl(pose, np.array([-35, -35, -30, -25, -20, -10, 12, 18, 18, 12]) * activity)
    return pose


def swim_pose(solver, gait, drop, data, phase):
    pose = gait_pose(solver, gait, drop, data, phase)
    pose.r('neck1', ry=-6)
    pose.r('head', ry=-8)
    if data['kind'] == 'elephant':
        for n in range(1, 11):
            pose.rot.pop('trunk' + str(n), None)
        trunk_curl(pose, [-34, -32, -28, -24, -20, -10, 8, 10, 10, 8])
    return pose


def clips(solver, data):
    height = data['height']
    elephant = data['kind'] == 'elephant'
    result = [Clip('idle', 12, partial(idle_pose, solver, data))]
    for name, period, speed, duty, pattern in [
            ('walk', 1.6 if elephant else 1.4, data['target_walk'], .66 if elephant else .7, LATERAL if elephant else quad.WALK),
            ('trot', .68 if elephant else .62, data['target_fast'], .45 if elephant else .42,
             LATERAL if elephant else quad.TROT),
            ('swim', 2.3, data['target_walk'] * .6, .72, LATERAL if elephant else quad.WALK)]:
        stride = speed * period * duty
        # Cap reach at the authored rig's scale; the resulting measured stance speed is
        # recorded rather than pretending that an unreachable requested stride was achieved.
        stride = min(stride, height * (.40 if elephant else .35))
        # Quantize before making the path, so baked 30 Hz timing has the same
        # stance velocity as the continuous solver and manifest.
        period = max(1, round(stride / (speed * duty) * 30)) / 30
        stride = speed * period * duty
        gait = quad.Gait(name, period, stride, duty, pattern, lift=.035 * height,
                        bob=.002 * height, hip_yaw=.5, chest_yaw=.5, sway=0, roll=.2,
                        heel_off=0, swing_pitch=8, land_pitch=0, toe_follow=0, scapula=.1,
                        tail_sway=2, head_nod=.6)
        base = quad.gait_clip(solver, gait)
        function = swim_pose if name == 'swim' else gait_pose
        clip = Clip(name, period, partial(function, solver, gait, base.drop + .003, data),
                    speed=gait.speed, events=base.events)
        clip.footfall_offsets = pattern
        clip.duty = duty
        result.append(clip)
    if elephant:
        base = quad.stand(solver, Pose()).move('hips', dz=-.05 * height)
        tip = solver.tail['trunk10'].copy()
        down = tip.copy()
        down[0] += .15 * height
        down[2] = .05
        up = solver.point(solver.solve(base), 'head', solver.rig.extra['mouth'])
        angles_down = trunk_to(solver, base, down, [0] * 10)
        angles_up = trunk_to(solver, base, up, [18] * 10)
        result += [Clip('eat', 6, partial(elephant_feed, solver, data, angles_down, angles_up, False)),
                   Clip('drink', 8, partial(elephant_feed, solver, data, angles_down, angles_up, True)),
                   Clip('rest', 12, partial(elephant_rest, solver, data)),
                   Clip('sleep', 12, partial(lateral_sleep, solver, data))]
    else:
        params = quad.Params(gaits=[], head_down=quad.HeadDown(body_pitch=5, crouch=.06 * height,
                             chest=2, neck=(16, 20), head=5),
                             lie=quad.Lie(drop=.43 * height, hind_tuck=.08 * height, hind_out=.15 * height,
                                          front_out=.12 * height, foot_lift=.06 * height),
                             look_yaw=3, tail_idle=2, sleep_neck=(6, 9), sleep_head=4,
                             sleep_chin=.05, idle_bob=.002, rest_bob=.001, sleep_bob=.001)
        result += [quad.eat_clip(solver, params), quad.drink_clip(solver, params)]
        # A side-sleep keeps the full barrel in contact with the floor. Sternal rest is
        # handled separately so the animal actually folds its legs beneath the body.
        rest = quad.rest_clip(solver, params)
        result += [Clip('rest', rest.duration, partial(rhino_rest, solver, data, rest)),
                   Clip('sleep', 12, partial(lateral_sleep, solver, data))]
    result.append(Clip('vocalize', 3, partial(call_pose, solver, data), loop=False, events=[(.3, 'call')]))
    if elephant:
        # Dust is gathered a little above the ground, where the curled tip clears it.
        scoop = down.copy()
        scoop[2] = .08 * height / 3.2 + .05
        angles_scoop = trunk_to(solver, base, scoop, [0] * 10)
        result.append(Clip('dust_bathe', 16, partial(elephant_dust_bathe, solver, data, angles_scoop)))
    return result


def rhino_rest(solver, data, clip, phase):
    return grounded(solver, clip.pose(phase), data)
