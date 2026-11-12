# SPDX-License-Identifier: MIT
"""Clip generators for four-legged animals. Pure numpy (no bpy).

Everything here is kinematic, built on what is known about how quadrupeds move:

* Symmetrical gaits are described the way Hildebrand's gait diagrams describe them: a
  cycle period, a duty factor (the fraction of the cycle a foot is on the ground) and the
  phase at which each foot touches down. A lateral-sequence walk, a trot and a pace differ
  only in these numbers.
* A foot on the ground does not move: during stance its target travels backwards under the
  body at exactly the clip's ground speed, so a character moved at that speed (the
  ``speed`` written into the manifest) shows no foot sliding. The legs are solved by IK
  from those targets, never the other way round.
* The trunk rides on the legs: in a walk each limb girdle is highest at the mid-stance of
  its legs (inverted pendulum), in a trot lowest (spring-mass), which is why ``bob`` changes
  sign between the two. The pelvis and the shoulders yaw with the reaching leg and the
  spine and neck undo it, so the head stays steady, as it does in a real animal.

Legs are keyed "FL", "FR", "HL", "HR".
"""
import math
from dataclasses import dataclass, field

import numpy as np

from .rig import Clip, Pose, bump, envelope, hermite, rot_y, smooth

WALK = {"HL": 0.0, "FL": 0.25, "HR": 0.5, "FR": 0.75}      # lateral sequence, as most mammals
TROT = {"HL": 0.0, "FR": 0.0, "HR": 0.5, "FL": 0.5}        # diagonal pairs
PACE = {"HL": 0.0, "FL": 0.06, "HR": 0.5, "FR": 0.56}      # lateral pairs (camels, giraffes)

HEEL_OFF_SPAN = 0.32        # last part of the stance during which the heel rises
LAND_SPAN = 0.12            # first part of the stance during which the foot settles


def _side(name):
    return 1.0 if name.endswith("L") else -1.0


def _is_hind(key):
    return key.startswith("H")


@dataclass
class Gait:
    name: str
    period: float               # seconds per cycle
    stance: float               # metres the body travels over one planted foot
    duty: float                 # fraction of the cycle a foot is on the ground
    offsets: dict               # leg key -> phase of touchdown
    lift: float = 0.09          # toe clearance in mid-swing (m)
    bob: float = 0.012          # girdle bounce (m); + highest at mid-stance (walk), - lowest (trot)
    hip_yaw: float = 4.0        # pelvis yaw amplitude (deg)
    chest_yaw: float = 3.0
    sway: float = 0.012         # lateral shift towards the supporting hind leg (m)
    roll: float = 1.2           # pelvis roll (deg)
    heel_off: float = 32.0      # foot pitch at toe-off (deg)
    swing_pitch: float = 48.0   # foot pitch while the paw is carried forward
    land_pitch: float = -4.0    # foot pitch at touchdown
    toe_follow: float = 0.6     # how much the toes follow the foot in the swing
    scapula: float = 0.9        # how much of the leg's swing angle the shoulder blade takes
    head_nod: float = 1.2       # neck pitch per step (deg)
    tail_sway: float = 6.0      # deg
    lean: float = 0.0           # constant trunk pitch (deg, + nose down)
    front_bias: float = 0.0     # shifts the front feet's stance forward (m)
    hind_bias: float = 0.0
    reach_limit: float = 0.985  # legs never straighten beyond this fraction of their length
    drop: float = None          # how far the body rides below its rest height; None: just enough

    @property
    def speed(self):
        return self.stance / (self.duty * self.period)


def foot_path(g, p):
    """Where a foot is at phase `p` after its touchdown: (dx, dz, foot pitch, toe pitch).

    dx is relative to the middle of the stance; pitches in degrees, + lifts the heel."""
    L, b = g.stance, g.duty
    if p < b:
        s = p / b
        if s < LAND_SPAN:
            pitch = g.land_pitch * (1.0 - smooth(s / LAND_SPAN))
        else:
            pitch = g.heel_off * smooth((s - (1.0 - HEEL_OFF_SPAN)) / HEEL_OFF_SPAN)
        return L * (0.5 - s), 0.0, pitch, 0.0
    s = (p - b) / (1.0 - b)
    # The swing starts and ends at the ground's speed, so the foot neither stabs the ground
    # nor is yanked off it; capped for very short stances, where that would overshoot.
    m = -L * min((1.0 - b) / b, 1.5)
    x = hermite(-0.5 * L, 0.5 * L, m, m, s)
    if s < 0.35:
        k = smooth(s / 0.35)
        pitch = g.heel_off + (g.swing_pitch - g.heel_off) * k
    else:
        k = smooth((s - 0.35) / 0.65)
        pitch = g.swing_pitch + (g.land_pitch - g.swing_pitch) * k
    toe = g.toe_follow * pitch * math.sin(math.pi * s)
    z = g.lift * math.sin(math.pi * s) ** 2
    return x, z, pitch, toe


def gait_pose(solver, g, u, drop):
    rig = solver.rig
    pose = Pose()
    b = g.duty
    ph = {k: (u - g.offsets[k]) % 1.0 for k in rig.legs}
    two_pi = 2.0 * math.pi

    dz_h = g.bob * math.cos(2.0 * two_pi * (ph["HL"] - 0.5 * b)) - drop
    dz_f = g.bob * math.cos(2.0 * two_pi * (ph["FL"] - 0.5 * b)) - drop
    trunk = abs(solver.head[rig.legs["FL"].upper][0] - solver.head[rig.body][0])
    pitch = -math.degrees(math.atan2(dz_f - dz_h, max(trunk, 1e-3)))
    support = math.cos(two_pi * (ph["HL"] - 0.5 * b))
    yaw_h = -g.hip_yaw * math.cos(two_pi * ph["HL"])
    yaw_c = -g.chest_yaw * math.cos(two_pi * ph["FL"])

    pose.move(rig.body, dy=g.sway * support, dz=dz_h)
    pose.r(rig.body, rx=g.roll * support, ry=pitch + g.lean, rz=yaw_h)
    chain = list(rig.spine) + ([rig.chest] if rig.chest else [])
    for bone in chain:
        pose.r(bone, rz=-yaw_h / len(chain))
    pose.r(rig.chest, rz=yaw_c)
    if rig.neck:
        nod = g.head_nod * math.cos(2.0 * two_pi * (ph["FL"] - 0.5 * b))
        pose.r(rig.neck[0], ry=-0.8 * pitch + nod, rz=-yaw_c)
    for i, bone in enumerate(rig.tail):
        swing = g.tail_sway * (0.6 + 0.4 * i) * math.sin(two_pi * (u - 0.1 * i))
        pose.r(bone, rz=swing - (0.5 * yaw_h if i == 0 else 0.0))

    for key, leg in rig.legs.items():
        rest = solver.toe_base(key)
        dx, dz, foot, toe = foot_path(g, ph[key])
        dx += g.hind_bias if _is_hind(key) else g.front_bias
        if leg.toes and toe:
            # A paw that hangs toes-down needs that much more clearance.
            v = solver.tail[leg.toes] - solver.head[leg.toes]
            dz += max(0.0, v[2] - (rot_y(toe) @ v)[2])
        pose.plant(key, rest + np.array([dx, 0.0, dz]), foot_pitch=foot, toe_pitch=toe)
        if leg.scapula:
            height = solver.head[leg.scapula][2] - rest[2]
            pose.r(leg.scapula, ry=-g.scapula * math.degrees(math.atan2(dx, height)))
    return pose


def fit_drop(solver, make_pose, limit, samples=48, most=0.4):
    """Smallest constant lowering of the body with which no leg has to straighten beyond
    `limit` anywhere in the cycle. A leg driven to full extension snaps ("knee pop")."""
    def fits(d):
        solver.reset_stats()
        for i in range(samples):
            solver.solve(make_pose(i / samples, d))
        return solver.extension <= limit
    if fits(0.0):
        return 0.0
    lo, hi = 0.0, most
    if not fits(hi):
        return hi
    for _ in range(14):
        mid = 0.5 * (lo + hi)
        if fits(mid):
            hi = mid
        else:
            lo = mid
    return hi


def gait_clip(solver, g):
    drop = g.drop
    if drop is None:
        drop = fit_drop(solver, lambda u, d: gait_pose(solver, g, u, d), g.reach_limit)
    events = sorted((g.offsets[k] % 1.0, "step") for k in solver.rig.legs)
    clip = Clip(g.name, g.period, lambda u: gait_pose(solver, g, u, drop), loop=True,
                speed=g.speed, events=events)
    clip.drop = drop
    return clip


# ---- standing -----------------------------------------------------------------------------

@dataclass
class HeadDown:
    """How the animal brings its muzzle to the ground; angles at full effort (deg)."""
    body_pitch: float = 14.0
    crouch: float = 0.05            # pelvis lowered (m)
    chest: float = 8.0
    neck: tuple = (34.0, 30.0)      # per neck bone
    head: float = 22.0
    splay: float = 0.0              # front feet moved apart, per side (m): giraffes


@dataclass
class Lie:
    """Sternal recumbency: on the brisket, forelegs forward, hind legs folded alongside."""
    drop: float                     # pelvis lowered (m)
    pitch: float = 0.0              # trunk pitch (deg, + nose down)
    hind_tuck: float = 0.10         # hock this far behind the hip joint (m)
    hind_out: float = 0.05          # hind feet moved outwards (m)
    front_out: float = 0.0
    tail_curl: float = 22.0         # the tail is laid on the ground, curled by this much per bone (deg)
    tail_floor: float = 0.04        # height of the tail's centre line when it lies (m)


@dataclass
class Params:
    gaits: list
    lie: Lie
    head_down: HeadDown = field(default_factory=HeadDown)
    eat_height: float = 0.07        # muzzle height when eating from the ground (m)
    drink_height: float = 0.03
    idle_period: float = 6.0
    look_yaw: float = 16.0
    idle_shift: float = 0.012
    tail_idle: float = 8.0
    ear_flick: float = 14.0
    chew_open: float = 9.0
    chews: int = 6
    sleep_neck: tuple = (26.0, 26.0)
    sleep_head: float = 14.0
    sleep_yaw: float = 16.0
    sleep_chin: float = 0.13
    call_neck: float = -9.0         # per neck bone (deg, - raises the head)
    call_head: float = -14.0
    call_open: float = 28.0
    call_duration: float = 2.6


def stand(solver, pose, front_splay=0.0):
    for key in solver.rig.legs:
        pos = solver.toe_base(key)
        if front_splay and not _is_hind(key):
            pos[1] += _side(key) * front_splay
        pose.plant(key, pos)
    return pose


def muzzle(solver, pose):
    """Position of the muzzle tip in a pose."""
    rig = solver.rig
    bone, point = rig.extra.get("muzzle", (None, None))
    if bone is None:
        bone = rig.jaw or rig.head
        point = solver.tail[bone]
    return solver.point(solver.solve(pose), bone, point)


def bisect_effort(height_at, target, what):
    """The smallest effort k in [0, 1] at which `height_at(k)` comes down to `target`."""
    steps = 50
    prev = 0.0
    for i in range(1, steps + 1):
        k = i / steps
        if height_at(k) <= target:
            lo, hi = prev, k
            for _ in range(16):
                mid = 0.5 * (lo + hi)
                if height_at(mid) > target:
                    lo = mid
                else:
                    hi = mid
            return hi
        prev = k
    best = min((height_at(i / steps), i / steps) for i in range(steps + 1))
    print(f"ZOOGEN warning: {what}: muzzle only reaches {best[0]:.3f} m, wanted {target:.3f} m")
    return best[1]


def head_down_pose(solver, hd, k):
    rig = solver.rig
    pose = stand(solver, Pose(), hd.splay * k)
    pose.move(rig.body, dz=-hd.crouch * k)
    pose.r(rig.body, ry=hd.body_pitch * k)
    pose.r(rig.chest, ry=hd.chest * k)
    for bone, a in zip(rig.neck, hd.neck):
        pose.r(bone, ry=a * k)
    pose.r(rig.head, ry=hd.head * k)
    return pose


def _ears(pose, rig, u, amount, times):
    for bone in rig.ears:
        f = sum(bump(u, t + (0.013 if bone.endswith("R") else 0.0), 0.03) for t in times)
        pose.r(bone, rz=_side(bone) * amount * f, ry=-0.4 * amount * f)


def idle_clip(solver, P):
    rig = solver.rig

    def pose_fn(u):
        a = 2.0 * math.pi * u
        breath = math.sin(2.0 * a)
        pose = stand(solver, Pose())
        pose.move(rig.body, dy=P.idle_shift * math.sin(a), dz=0.004 * breath)
        pose.r(rig.body, rx=0.7 * math.sin(a))
        pose.r(rig.chest, ry=-0.5 * breath)
        yaw = P.look_yaw * (0.75 * math.sin(a) + 0.25 * math.sin(2.0 * a + 1.0))
        pitch = 3.0 * math.sin(a + 2.1)
        for bone in rig.neck:
            pose.r(bone, rz=0.6 * yaw / len(rig.neck), ry=0.5 * pitch / len(rig.neck))
        pose.r(rig.head, rz=0.4 * yaw, ry=0.5 * pitch)
        for i, bone in enumerate(rig.tail):
            pose.r(bone, rz=P.tail_idle * (0.5 + 0.5 * i) * math.sin(2.0 * a - 0.7 * i))
        _ears(pose, rig, u, P.ear_flick, (0.22, 0.27, 0.71))
        return pose
    return Clip("idle", P.idle_period, pose_fn)


def eat_clip(solver, P, name="eat", height=None, duration=3.0):
    rig = solver.rig
    hd = P.head_down
    target = P.eat_height if height is None else height
    k0 = bisect_effort(lambda k: muzzle(solver, head_down_pose(solver, hd, k))[2], target, name)

    def pose_fn(u):
        a = 2.0 * math.pi * u
        pose = head_down_pose(solver, hd, k0 * (1.0 - 0.04 * (0.5 - 0.5 * math.cos(2.0 * a))))
        pose.r(rig.jaw, ry=P.chew_open * (0.5 - 0.5 * math.cos(P.chews * a)))
        pose.r(rig.head, rz=2.5 * math.sin(2.0 * a), rx=2.0 * math.sin(a))
        for i, bone in enumerate(rig.tail):
            pose.r(bone, rz=0.6 * P.tail_idle * (0.5 + 0.5 * i) * math.sin(a - 0.7 * i))
        _ears(pose, rig, u, P.ear_flick, (0.4,))
        return pose
    return Clip(name, duration, pose_fn)


def drink_clip(solver, P):
    rig = solver.rig
    hd = P.head_down
    k0 = bisect_effort(lambda k: muzzle(solver, head_down_pose(solver, hd, k))[2],
                       P.drink_height, "drink")

    def pose_fn(u):
        a = 2.0 * math.pi * u
        pose = head_down_pose(solver, hd, k0)
        pose.r(rig.jaw, ry=3.0 + 3.0 * math.sin(8.0 * a))
        pose.r(rig.head, ry=0.8 * math.sin(8.0 * a))
        for i, bone in enumerate(rig.tail):
            pose.r(bone, rz=0.5 * P.tail_idle * (0.5 + 0.5 * i) * math.sin(a - 0.7 * i))
        return pose
    return Clip("drink", 2.4, pose_fn)


def lie_pose(solver, L):
    """Sternal lying pose with the head carried as in the rest pose."""
    rig = solver.rig
    pose = Pose()
    pose.move(rig.body, dz=-L.drop)
    pose.r(rig.body, ry=L.pitch)
    joints = solver.solve(pose)         # forward kinematics tells where the shoulders and hips are
    for key, leg in rig.legs.items():
        side = _side(key)
        joint = joints[leg.upper][:3, 3]
        toe_rest, ankle_rest = solver.toe_base(key), solver.head[leg.foot]
        fd = toe_rest - ankle_rest
        flat = -math.degrees(math.atan2(-fd[2], fd[0]))      # lays the foot forward on the ground
        foot_len = float(np.hypot(fd[0], fd[2]))
        l1 = np.linalg.norm(solver.head[leg.lower] - solver.head[leg.upper])
        l2 = np.linalg.norm(ankle_rest - solver.head[leg.lower])
        ground = toe_rest[2]
        if _is_hind(key):
            ankle_x = joint[0] - L.hind_tuck
            y = toe_rest[1] + side * L.hind_out
            pole = (1.0, side * 0.5, 0.35)                   # knee forward, beside the belly
        else:
            # Elbow on the ground under the shoulder, forearm flat in front of it.
            dz = joint[2] - ground
            back = math.sqrt(max(l1 * l1 - dz * dz, 0.0)) if dz < l1 else 0.0
            ankle_x = joint[0] - back + (l2 if dz < l1 else 0.75 * l2)
            y = toe_rest[1] + side * L.front_out
            pole = (-1.0, 0.0, -0.4)
        pose.plant(key, (ankle_x + foot_len, y, ground), foot_pitch=flat, pole=pole)
    # A hanging tail would end up under the ground: lay it down, bone by bone.
    for bone in rig.tail:
        pose.r(bone, rz=L.tail_curl)

        def end_height(a):
            trial = Pose()
            trial.rot, trial.offset = dict(pose.rot), pose.offset
            trial.r(bone, ry=a)
            return solver.point(solver.solve(trial), bone, solver.tail[bone])[2]
        if end_height(0.0) < L.tail_floor:
            lo, hi = 0.0, 150.0
            for _ in range(16):
                mid = 0.5 * (lo + hi)
                if end_height(mid) < L.tail_floor:
                    lo = mid
                else:
                    hi = mid
            pose.r(bone, ry=hi)
    return pose


def rest_clip(solver, P):
    rig = solver.rig

    def pose_fn(u):
        a = 2.0 * math.pi * u
        breath = math.sin(2.0 * a)
        pose = lie_pose(solver, P.lie)
        pose.move(rig.body, dz=0.004 * breath)
        pose.r(rig.chest, ry=-0.6 * breath)
        yaw = 1.4 * P.look_yaw * (0.8 * math.sin(a) + 0.2 * math.sin(3.0 * a + 0.6))
        for bone in rig.neck:
            pose.r(bone, rz=0.6 * yaw / len(rig.neck), ry=-4.0)
        pose.r(rig.head, rz=0.4 * yaw, ry=3.0 + 2.0 * math.sin(a + 1.0))
        for i, bone in enumerate(rig.tail):
            flick = bump(u, 0.3, 0.08) - bump(u, 0.62, 0.06)
            pose.r(bone, rz=P.tail_idle * (0.8 + 0.8 * i) * flick)
        _ears(pose, rig, u, P.ear_flick, (0.12, 0.55, 0.6))
        return pose
    return Clip("rest", 8.0, pose_fn)


def sleep_clip(solver, P):
    rig = solver.rig

    def head_pose(k, breath=0.0):
        pose = lie_pose(solver, P.lie)
        pose.move(rig.body, dz=0.005 * breath)
        pose.r(rig.chest, ry=-0.7 * breath)
        for bone, ang in zip(rig.neck, P.sleep_neck):
            pose.r(bone, ry=ang * k, rz=P.sleep_yaw * k / len(rig.neck))
        pose.r(rig.head, ry=P.sleep_head * k, rx=8.0 * k)
        return pose
    k0 = bisect_effort(lambda k: muzzle(solver, head_pose(k))[2], P.sleep_chin, "sleep")

    def pose_fn(u):
        a = 2.0 * math.pi * u
        pose = head_pose(k0, math.sin(2.0 * a))
        _ears(pose, rig, u, 0.6 * P.ear_flick, (0.5,))
        return pose
    return Clip("sleep", 8.0, pose_fn)


def vocalize_clip(solver, P):
    rig = solver.rig

    def pose_fn(u):
        e = envelope(u, 0.22, 0.72)
        shake = math.sin(2.0 * math.pi * 9.0 * u) * e
        pose = stand(solver, Pose())
        pose.move(rig.body, dz=-0.01 * e)
        pose.r(rig.chest, ry=-2.0 * e)
        for bone in rig.neck:
            pose.r(bone, ry=P.call_neck * e)
        pose.r(rig.head, ry=P.call_head * e + 0.5 * shake)
        pose.r(rig.jaw, ry=P.call_open * e + 1.0 * shake)
        for i, bone in enumerate(rig.tail):
            pose.r(bone, ry=-6.0 * e)
        return pose
    return Clip("vocalize", P.call_duration, pose_fn, loop=False,
                events=[(0.22, "call")])


def standard_clips(solver, P):
    """The clip set every quadruped provides: idle, its gaits, eat, drink, rest, sleep,
    vocalize. The game looks clips up by these names."""
    clips = [idle_clip(solver, P)]
    clips += [gait_clip(solver, g) for g in P.gaits]
    clips += [eat_clip(solver, P), drink_clip(solver, P), rest_clip(solver, P),
              sleep_clip(solver, P), vocalize_clip(solver, P)]
    return clips
