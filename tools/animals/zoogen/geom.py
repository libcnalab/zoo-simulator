# SPDX-License-Identifier: MIT
"""Lofted body parts: the anatomical building blocks of a generated animal.

A part is a chain of elliptical cross-sections ("rings") in animal space
(+X forward, +Y left, +Z up).  Rings are interpolated with a Catmull-Rom spline
and lofted into a closed tube.  Every ring carries skin weights, derived from the
bone chain the part hangs on, so the fused body can be skinned without guessing.
"""
from dataclasses import dataclass, field

import numpy as np

from . import noise as nz


def v3(p):
    return np.asarray(p, dtype=np.float64)


def unit(v):
    v = np.asarray(v, dtype=np.float64)
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


@dataclass
class Ring:
    """One cross-section. ``ry`` is the half extent along the side axis; ``up`` and
    ``dn`` are the half extents along the other axis (dorsal/ventral for a horizontal
    chain, front/back for a leg).  ``n`` is the superellipse exponent (2 = ellipse)."""
    c: np.ndarray
    ry: float
    up: float
    dn: float = None
    n: float = 2.0

    def __post_init__(self):
        self.c = v3(self.c)
        if self.dn is None:
            self.dn = self.up


@dataclass
class Part:
    name: str
    rings: list
    chain: list = None            # [(bone_name, head_xyz, tail_xyz), ...] used for weights
    weights: dict = None          # fixed weights {bone: w} (overrides chain)
    segments: int = 20
    subdiv: int = 4               # interpolated rings between authored rings
    cap_start: str = "round"      # round | flat | point
    cap_end: str = "round"
    fuse: bool = True             # joined into the voxel-remeshed skin
    tag: str = "body"             # texture mask name
    side_ref: tuple = (0.0, 1.0, 0.0)
    rough: tuple = None           # (amplitude_m, frequency) surface clumping (manes, fur)
    cap_scale: float = 1.0
    blend: float = 1.0            # how far chain weights blend across joints (0 = rigid)
    mirror: bool = False          # also create the right-side (-Y) copy; bones .L -> .R
    radius_scale: float = 1.0     # uniform fattening/thinning of every ring (tuning aid)


def R(c, ry, up, dn=None, n=2.0):
    return Ring(c, ry, up, dn, n)


def _catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2
                  + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)


def _resample(rings, subdiv):
    """Catmull-Rom interpolate ring centres and radii."""
    if subdiv <= 1 or len(rings) < 2:
        return list(rings)
    arr = np.array([[*r.c, r.ry, r.up, r.dn, r.n] for r in rings])
    out = []
    n = len(arr)
    for i in range(n - 1):
        p0 = arr[max(i - 1, 0)]
        p1 = arr[i]
        p2 = arr[i + 1]
        p3 = arr[min(i + 2, n - 1)]
        for k in range(subdiv):
            t = k / subdiv
            q = _catmull(p0, p1, p2, p3, t)
            # Radii must not overshoot below zero or wobble past the authored values.
            lo = np.minimum(p1[3:6], p2[3:6])
            hi = np.maximum(p1[3:6], p2[3:6])
            q[3:6] = np.clip(q[3:6], lo * 0.97, hi * 1.03)
            q[6] = p1[6] * (1 - t) + p2[6] * t
            out.append(Ring(q[0:3], q[3], q[4], q[5], q[6]))
    last = arr[-1]
    out.append(Ring(last[0:3], last[3], last[4], last[5], last[6]))
    return out


def chain_weights(p, chain, blend=1.0):
    """Skin weights of point ``p`` for a bone chain [(name, head, tail), ...].

    The weight of a bone peaks along its middle and cross-fades with its
    neighbours around the joints, which is what makes elbows and spines bend
    smoothly.  ``blend`` scales the width of the cross-fade (fraction of the
    shorter adjoining bone)."""
    p = v3(p)
    best = None
    for i, (_, h, t) in enumerate(chain):
        h, t = v3(h), v3(t)
        d = t - h
        L2 = float(d @ d)
        s = 0.0 if L2 < 1e-12 else float((p - h) @ d) / L2
        sc = min(max(s, 0.0), 1.0)
        dist = np.linalg.norm(p - (h + d * sc))
        if best is None or dist < best[0] - 1e-9:
            best = (dist, i, s)
    _, i, s = best
    name, h, t = chain[i]
    L = np.linalg.norm(v3(t) - v3(h))
    w = {name: 1.0}
    half = 0.5 * blend
    if blend > 1e-6:
        if s < half and i > 0:
            Lp = np.linalg.norm(v3(chain[i - 1][2]) - v3(chain[i - 1][1]))
            reach = half * min(L, Lp) / max(L, 1e-9)
            if s < reach:
                k = 0.5 * (1.0 - max(s, -reach) / reach)   # 0.5 at the joint, 0 at reach
                w = {name: 1.0 - k, chain[i - 1][0]: k}
        elif s > 1.0 - half and i < len(chain) - 1:
            Ln = np.linalg.norm(v3(chain[i + 1][2]) - v3(chain[i + 1][1]))
            reach = half * min(L, Ln) / max(L, 1e-9)
            if s > 1.0 - reach:
                k = 0.5 * (1.0 - min(1.0 - s, reach) / reach)
                k = min(max(k, 0.0), 0.5)
                w = {name: 1.0 - k, chain[i + 1][0]: k}
    return w


def _ring_points(ring, tangent, side_ref, segments):
    t = unit(tangent)
    s = v3(side_ref) - t * float(v3(side_ref) @ t)
    if np.linalg.norm(s) < 1e-6:
        s = np.array([1.0, 0.0, 0.0]) - t * t[0]
    s = unit(s)
    o = np.cross(t, s)
    ang = np.linspace(0.0, 2.0 * np.pi, segments, endpoint=False)
    ca, sa = np.cos(ang), np.sin(ang)
    e = 2.0 / ring.n
    px = np.sign(ca) * np.abs(ca) ** e * ring.ry
    rz = np.where(sa >= 0, ring.up, ring.dn)
    pz = np.sign(sa) * np.abs(sa) ** e * rz
    return ring.c[None, :] + px[:, None] * s[None, :] + pz[:, None] * o[None, :]


def _cap_rings(ring, tangent, sign, style, scale):
    """Extra rings that close a tube end with a rounded or pointed tip."""
    if style == "flat":
        return []
    depth = min(ring.ry, 0.5 * (ring.up + ring.dn)) * scale
    out = []
    steps = (0.35, 0.65, 0.88) if style == "round" else (0.4, 0.75)
    for a in steps:
        if style == "round":
            off, rad = np.sin(a * np.pi / 2), np.cos(a * np.pi / 2)
        else:
            off, rad = a * 2.2, 1.0 - a
        out.append(Ring(ring.c + unit(tangent) * sign * depth * off,
                        ring.ry * rad, ring.up * rad, ring.dn * rad, ring.n))
    return out


def loft(part):
    """Return (verts Nx3, tris Mx3, weights list[dict], ring_param N) for a part."""
    rings = _resample(part.rings, part.subdiv)
    if part.radius_scale != 1.0:
        k = part.radius_scale
        rings = [Ring(r.c, r.ry * k, r.up * k, r.dn * k, r.n) for r in rings]
    n = len(rings)
    tangents = []
    for i in range(n):
        a = rings[max(i - 1, 0)].c
        b = rings[min(i + 1, n - 1)].c
        tangents.append(unit(b - a))
    start_caps = _cap_rings(rings[0], tangents[0], -1.0, part.cap_start, part.cap_scale)[::-1]
    end_caps = _cap_rings(rings[-1], tangents[-1], +1.0, part.cap_end, part.cap_scale)
    all_rings = start_caps + rings + end_caps
    all_tan = [tangents[0]] * len(start_caps) + tangents + [tangents[-1]] * len(end_caps)
    seg = part.segments
    verts = []
    wts = []
    for ring, tan in zip(all_rings, all_tan):
        pts = _ring_points(ring, tan, part.side_ref, seg)
        if part.rough:
            amp, freq = part.rough
            d = pts - ring.c[None, :]
            k = 1.0 + amp * (nz.fbm(pts, freq, 3, seed=11) - 0.5) * 2.0 \
                / max(1e-6, float(np.mean(np.linalg.norm(d, axis=1))))
            pts = ring.c[None, :] + d * k[:, None]
        verts.append(pts)
        if part.weights is not None:
            w = dict(part.weights)
        else:
            w = chain_weights(ring.c, part.chain, part.blend)
        wts.extend([w] * seg)
    verts = np.concatenate(verts, axis=0)
    nr = len(all_rings)
    tris = []
    for i in range(nr - 1):
        for j in range(seg):
            a = i * seg + j
            b = i * seg + (j + 1) % seg
            c = (i + 1) * seg + (j + 1) % seg
            d = (i + 1) * seg + j
            tris.append((a, c, d))
            tris.append((a, b, c))
    # Close both ends with a pole vertex.
    def close(ring_index, ring, tan, sign, flip):
        nonlocal verts
        depth = 0.0
        if (part.cap_start if sign < 0 else part.cap_end) != "flat":
            depth = min(ring.ry, 0.5 * (ring.up + ring.dn)) * 0.6
        pole = ring.c + unit(tan) * sign * depth
        idx = len(verts)
        verts = np.vstack([verts, pole[None, :]])
        wts.append(wts[ring_index * seg])
        for j in range(seg):
            a = ring_index * seg + j
            b = ring_index * seg + (j + 1) % seg
            tris.append((idx, b, a) if flip else (idx, a, b))
    close(0, all_rings[0], all_tan[0], -1.0, True)
    close(nr - 1, all_rings[-1], all_tan[-1], +1.0, False)
    return verts, np.array(tris, dtype=np.int64), wts


def mirror_name(name):
    if name.endswith(".L"):
        return name[:-2] + ".R"
    return name


def mirror_part(part):
    """Right-side copy of a left-side part (reflect Y, swap .L bones for .R)."""
    def my(p):
        p = v3(p).copy()
        p[1] = -p[1]
        return p
    rings = [Ring(my(r.c), r.ry, r.up, r.dn, r.n) for r in part.rings]
    chain = None
    if part.chain is not None:
        chain = [(mirror_name(b), my(h), my(t)) for b, h, t in part.chain]
    weights = None
    if part.weights is not None:
        weights = {mirror_name(b): w for b, w in part.weights.items()}
    name = mirror_name(part.name) if part.name.endswith(".L") else part.name + ".R"
    sr = (part.side_ref[0], part.side_ref[1], part.side_ref[2])
    if abs(sr[1]) < 0.999:      # a custom side axis is mirrored too
        sr = (sr[0], -sr[1], sr[2])
    return Part(name=name, rings=rings, chain=chain, weights=weights,
                segments=part.segments, subdiv=part.subdiv, cap_start=part.cap_start,
                cap_end=part.cap_end, fuse=part.fuse, tag=part.tag, side_ref=sr,
                rough=part.rough, cap_scale=part.cap_scale, blend=part.blend, mirror=False,
                radius_scale=part.radius_scale)


def expand_parts(parts):
    out = []
    for p in parts:
        out.append(p)
        if p.mirror:
            out.append(mirror_part(p))
    return out


@dataclass
class Bone:
    name: str
    parent: str
    head: np.ndarray
    tail: np.ndarray
    deform: bool = True


@dataclass
class Spec:
    """Everything the builder needs for one species."""
    name: str
    bones: list                       # [Bone]; left bones named *.L are mirrored
    parts: list                       # [Part]
    paint: object = None              # callable(ctx) -> rgb float array (H, W, 3)
    texture_size: int = 1024
    voxel: float = 0.012              # voxel remesh size in metres
    target_tris: int = 16000
    smooth_iters: int = 6
    meta: dict = field(default_factory=dict)   # copied into the manifest
    rig: object = None                # rig.Rig: which bones play which role
    clips: object = None              # callable(rig.Solver) -> [rig.Clip]
    validate_mesh: bool = False       # remove duplicate/invalid faces before UV and export


def expand_bones(bones):
    out = []
    for b in bones:
        out.append(b)
        if b.name.endswith(".L"):
            h, t = v3(b.head).copy(), v3(b.tail).copy()
            h[1], t[1] = -h[1], -t[1]
            out.append(Bone(mirror_name(b.name), mirror_name(b.parent) if b.parent else None,
                            h, t, b.deform))
    return out
