# SPDX-License-Identifier: MIT
"""Procedural posing: forward kinematics with axis-intuitive deltas plus analytic
leg IK. Pure numpy (no bpy), so it can be tested outside Blender.

All maths is in animal space (+X forward, +Y left, +Z up, metres).

A pose is described by

* a delta rotation per bone, expressed in animal axes and carried along by the
  parent's accumulated rotation (``rot_y(+a)`` on the head pitches the nose
  *down*, ``rot_z(+a)`` turns left, ``rot_x(+a)`` raises the left side),
* an optional translation per bone (the body's bob and sway),
* and, per leg, an optional foot target: where the toe base is in animal space
  and how the foot and the toes are oriented there. The two long leg bones are
  then solved analytically, so a planted foot stays planted whatever the body
  above it does.
"""
import math
from dataclasses import dataclass, field

import numpy as np

EPS = 1e-9


def rot_x(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]], dtype=np.float64)


def rot_y(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]], dtype=np.float64)


def rot_z(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]], dtype=np.float64)


def euler(rx=0.0, ry=0.0, rz=0.0):
    """Yaw, then pitch, then roll, in degrees (animal axes)."""
    return rot_z(rz) @ rot_y(ry) @ rot_x(rx)


def unit(v, fallback=(1.0, 0.0, 0.0)):
    v = np.asarray(v, dtype=np.float64)
    n = np.linalg.norm(v)
    if n < EPS:
        return np.asarray(fallback, dtype=np.float64)
    return v / n


def frame(u, w):
    """Orthonormal basis (columns): e1 along u, e2 along w made perpendicular, e3 = e1 x e2."""
    e1 = unit(u)
    w = np.asarray(w, dtype=np.float64)
    e2 = w - np.dot(w, e1) * e1
    if np.linalg.norm(e2) < 1e-6:
        alt = np.array([0.0, 1.0, 0.0]) if abs(e1[1]) < 0.9 else np.array([0.0, 0.0, 1.0])
        e2 = alt - np.dot(alt, e1) * e1
    e2 = unit(e2)
    return np.stack([e1, e2, np.cross(e1, e2)], axis=1)


@dataclass
class Leg:
    """One leg: the two solved bones, the foot (metapodial) and optionally the toes."""
    upper: str
    lower: str
    foot: str
    toes: str = None
    scapula: str = None


@dataclass
class FootTarget:
    pos: np.ndarray                 # toe base (head of the toes bone; of the foot's tail without toes)
    foot: np.ndarray = None         # 3x3 absolute delta of the foot bone (None: identity = rest orientation)
    toes: np.ndarray = None         # 3x3 absolute delta of the toes (None: identity = flat as in rest)
    pole: np.ndarray = None         # direction the knee/elbow leans to (None: as in the rest pose)


class Pose:
    def __init__(self):
        self.rot = {}       # bone -> 3x3 delta
        self.offset = {}    # bone -> translation (animal space)
        self.feet = {}      # leg key -> FootTarget

    def r(self, bone, rx=0.0, ry=0.0, rz=0.0):
        """Adds a rotation (degrees) to a bone; several calls compose."""
        if bone is None:
            return self
        m = euler(rx, ry, rz)
        self.rot[bone] = self.rot[bone] @ m if bone in self.rot else m
        return self

    def move(self, bone, dx=0.0, dy=0.0, dz=0.0):
        d = np.array([dx, dy, dz], dtype=np.float64)
        self.offset[bone] = self.offset.get(bone, 0.0) + d
        return self

    def plant(self, leg, pos, foot_pitch=0.0, toe_pitch=0.0, pole=None, foot_roll=0.0):
        """Foot target with the foot/toes pitched about the lateral axis (+ lifts the heel)."""
        self.feet[leg] = FootTarget(np.asarray(pos, dtype=np.float64),
                                    rot_x(foot_roll) @ rot_y(foot_pitch) if (foot_pitch or foot_roll) else None,
                                    rot_y(toe_pitch) if toe_pitch else None,
                                    None if pole is None else np.asarray(pole, dtype=np.float64))
        return self


@dataclass
class Rig:
    """Which bones play which role. Generic clip generators read these."""
    body: str                               # the bone that carries the trunk (pelvis)
    legs: dict                              # key ("FL", "FR", "HL", "HR" / "L", "R") -> Leg
    spine: list = field(default_factory=list)   # from the pelvis forward, without the body bone
    chest: str = None
    neck: list = field(default_factory=list)
    head: str = None
    jaw: str = None
    tail: list = field(default_factory=list)
    ears: list = field(default_factory=list)
    extra: dict = field(default_factory=dict)   # species specific roles (trunk, wings, ...)


class Solver:
    """Turns a Pose into one animal-space 4x4 matrix per bone."""

    def __init__(self, bones, rest, rig):
        """bones: [geom.Bone] (expanded); rest: name -> 4x4 rest matrix in animal space."""
        self.rig = rig
        self.parent = {b.name: b.parent for b in bones}
        self.order = self._sorted([b.name for b in bones])
        self.rest = {k: np.asarray(v, dtype=np.float64) for k, v in rest.items()}
        self.head = {n: self.rest[n][:3, 3].copy() for n in self.order}
        self.tail = {b.name: np.asarray(b.tail, dtype=np.float64) for b in bones}
        self.leg_of = {}
        for key, leg in rig.legs.items():
            self.leg_of[leg.upper] = key
        self.reset_stats()

    def reset_stats(self):
        """Clears what solve() records: how far a foot target was out of reach (metres) and
        the largest leg extension asked for (ankle distance / summed bone lengths)."""
        self.ik_error = 0.0
        self.extension = 0.0

    def _sorted(self, names):
        out, done = [], set()
        pending = list(names)
        while pending:
            rest = []
            for n in pending:
                p = self.parent[n]
                if p is None or p in done:
                    out.append(n)
                    done.add(n)
                else:
                    rest.append(n)
            if len(rest) == len(pending):
                raise ValueError("bone hierarchy has a cycle or a missing parent: %s" % rest)
            pending = rest
        return out

    # ---- queries on the rest pose -------------------------------------------------
    def toe_base(self, key):
        leg = self.rig.legs[key]
        return (self.head[leg.toes] if leg.toes else self.tail[leg.foot]).copy()

    def joint(self, bone):
        return self.head[bone].copy()

    # ---- solving ---------------------------------------------------------------------
    def solve(self, pose):
        G, H = {}, {}
        ident = np.eye(3)
        solved = set()
        for n in self.order:
            if n in solved:
                continue
            p = self.parent[n]
            if p is None:
                Gp, Hp, rp = ident, np.zeros(3), np.zeros(3)
            else:
                Gp, Hp, rp = G[p], H[p], self.head[p]
            head = Hp + Gp @ (self.head[n] - rp)
            if n in pose.offset:
                head = head + pose.offset[n]
            key = self.leg_of.get(n)
            if key is not None and key in pose.feet:
                self._solve_leg(key, pose.feet[key], Gp, head, G, H)
                leg = self.rig.legs[key]
                solved.update([leg.upper, leg.lower, leg.foot] + ([leg.toes] if leg.toes else []))
                continue
            G[n] = Gp @ pose.rot[n] if n in pose.rot else Gp
            H[n] = head
        out = {}
        for n in self.order:
            m = np.eye(4)
            m[:3, :3] = G[n] @ self.rest[n][:3, :3]
            m[:3, 3] = H[n]
            out[n] = m
        return out

    def _solve_leg(self, key, target, Gp, hip, G, H):
        leg = self.rig.legs[key]
        Hr, Kr, Ar = self.head[leg.upper], self.head[leg.lower], self.head[leg.foot]
        Tr = self.toe_base(key)
        Gfoot = np.eye(3) if target.foot is None else target.foot
        Gtoes = np.eye(3) if target.toes is None else target.toes
        ankle = target.pos + Gfoot @ (Ar - Tr)
        l1, l2 = np.linalg.norm(Kr - Hr), np.linalg.norm(Ar - Kr)
        d = ankle - hip
        dist = np.linalg.norm(d)
        self.extension = max(self.extension, dist / (l1 + l2))
        reach = min(max(dist, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.9995)
        dhat = unit(d, (0, 0, -1))
        if abs(reach - dist) > 1e-9:
            self.ik_error = max(self.ik_error, abs(reach - dist))
            ankle = hip + dhat * reach
        # The bend direction: as in the rest pose (carried by the body), unless told.
        rest_axis = unit(Ar - Hr, (0, 0, -1))
        b_rest = (Kr - Hr) - np.dot(Kr - Hr, rest_axis) * rest_axis
        if np.linalg.norm(b_rest) < 1e-5:
            b_rest = np.array([1.0, 0.0, 0.0])
        b0 = target.pole if target.pole is not None else Gp @ b_rest
        b = b0 - np.dot(b0, dhat) * dhat
        if np.linalg.norm(b) < 1e-6:
            b = np.array([1.0, 0.0, 0.0]) - dhat[0] * dhat
        b = unit(b)
        a = (l1 * l1 - l2 * l2 + reach * reach) / (2.0 * reach)
        h = math.sqrt(max(l1 * l1 - a * a, 0.0))
        knee = hip + dhat * a + b * h
        n_rest = np.cross(rest_axis, unit(b_rest))
        n_pose = np.cross(dhat, b)
        G[leg.upper] = frame(knee - hip, n_pose) @ frame(Kr - Hr, n_rest).T
        G[leg.lower] = frame(ankle - knee, n_pose) @ frame(Ar - Kr, n_rest).T
        G[leg.foot] = Gfoot
        H[leg.upper], H[leg.lower], H[leg.foot] = hip, knee, ankle
        if leg.toes:
            G[leg.toes] = Gtoes
            H[leg.toes] = ankle - Gfoot @ (Ar - Tr)

    def basis(self, world):
        """Per-bone pose matrix relative to the rest pose and the parent (Blender's matrix_basis)."""
        out = {}
        for n in self.order:
            p = self.parent[n]
            if p is None:
                out[n] = np.linalg.inv(self.rest[n]) @ world[n]
            else:
                rest_local = np.linalg.inv(self.rest[p]) @ self.rest[n]
                pose_local = np.linalg.inv(world[p]) @ world[n]
                out[n] = np.linalg.inv(rest_local) @ pose_local
        return out

    def point(self, world, bone, rest_point):
        """Where a point rigidly attached to `bone` (given in the rest pose) is in a solved pose."""
        local = np.linalg.inv(self.rest[bone]) @ np.append(np.asarray(rest_point, dtype=np.float64), 1.0)
        return (world[bone] @ local)[:3]


def quat_from_matrix(m):
    """Unit quaternion (w, x, y, z) of a 3x3 rotation matrix."""
    m = np.asarray(m, dtype=np.float64)
    t = m[0, 0] + m[1, 1] + m[2, 2]
    if t > 0.0:
        s = math.sqrt(t + 1.0) * 2.0
        q = ((0.25 * s), (m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s)
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = math.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2.0
        q = ((m[2, 1] - m[1, 2]) / s, 0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s)
    elif m[1, 1] > m[2, 2]:
        s = math.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2.0
        q = ((m[0, 2] - m[2, 0]) / s, (m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s)
    else:
        s = math.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2.0
        q = ((m[1, 0] - m[0, 1]) / s, (m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s)
    q = np.array(q, dtype=np.float64)
    return q / np.linalg.norm(q)


@dataclass
class Clip:
    name: str
    duration: float                 # seconds
    pose: object                    # callable(u in [0, 1]) -> Pose
    loop: bool = True
    speed: float = 0.0              # ground speed (m/s) at which the feet do not slide; 0: stationary
    events: list = field(default_factory=list)   # [(u, name)], e.g. footfalls


def hermite(p0, p1, m0, m1, s):
    s2, s3 = s * s, s * s * s
    return (2 * s3 - 3 * s2 + 1) * p0 + (s3 - 2 * s2 + s) * m0 + (-2 * s3 + 3 * s2) * p1 + (s3 - s2) * m1


def smooth(s):
    s = min(max(s, 0.0), 1.0)
    return s * s * (3.0 - 2.0 * s)


def bump(u, center, width):
    """Smooth 0..1..0 pulse around `center` (cyclic in u)."""
    d = abs((u - center + 0.5) % 1.0 - 0.5) / max(width, 1e-6)
    return 0.0 if d >= 1.0 else 0.5 * (1.0 + math.cos(math.pi * d))


def envelope(u, attack, release):
    """0 -> 1 over [0, attack], 1 until release, back to 0 at u = 1."""
    if u < attack:
        return smooth(u / attack)
    if u > release:
        return smooth((1.0 - u) / (1.0 - release))
    return 1.0
