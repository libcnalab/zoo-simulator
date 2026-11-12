# SPDX-License-Identifier: MIT
"""Deterministic numpy noise used for coat patterns and surface roughness."""
import numpy as np


def _hash3(ix, iy, iz, seed):
    h = (ix.astype(np.int64) * 374761393 + iy.astype(np.int64) * 668265263
         + iz.astype(np.int64) * 2147483647 + np.int64(seed) * 1274126177)
    h = (h ^ (h >> 13)) * 1274126177
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF).astype(np.float64) / float(0xFFFFFF)


def value_noise(p, freq=1.0, seed=0):
    """Smooth value noise in [0,1]; p has shape (..., 3)."""
    q = np.asarray(p, dtype=np.float64) * freq
    i = np.floor(q)
    f = q - i
    f = f * f * (3.0 - 2.0 * f)
    ix, iy, iz = i[..., 0], i[..., 1], i[..., 2]
    out = 0.0
    for dx in (0, 1):
        wx = f[..., 0] if dx else 1.0 - f[..., 0]
        for dy in (0, 1):
            wy = f[..., 1] if dy else 1.0 - f[..., 1]
            for dz in (0, 1):
                wz = f[..., 2] if dz else 1.0 - f[..., 2]
                out = out + wx * wy * wz * _hash3(ix + dx, iy + dy, iz + dz, seed)
    return out


def fbm(p, freq=1.0, octaves=4, seed=0, gain=0.5, lacunarity=2.0):
    """Fractal value noise, normalised to about [0,1]."""
    total = 0.0
    amp = 1.0
    norm = 0.0
    for o in range(octaves):
        total = total + amp * value_noise(p, freq, seed + o * 17)
        norm += amp
        amp *= gain
        freq *= lacunarity
    return total / norm


def worley(p, freq=1.0, seed=0, jitter=1.0):
    """Cellular noise: returns (f1, f2, cell_id) with distances in cell units."""
    q = np.asarray(p, dtype=np.float64) * freq
    i = np.floor(q)
    f = q - i
    shape = q.shape[:-1]
    f1 = np.full(shape, 1e9)
    f2 = np.full(shape, 1e9)
    cid = np.zeros(shape)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                cx, cy, cz = i[..., 0] + dx, i[..., 1] + dy, i[..., 2] + dz
                ox = 0.5 + jitter * (_hash3(cx, cy, cz, seed) - 0.5)
                oy = 0.5 + jitter * (_hash3(cx, cy, cz, seed + 101) - 0.5)
                oz = 0.5 + jitter * (_hash3(cx, cy, cz, seed + 211) - 0.5)
                d = np.sqrt((dx + ox - f[..., 0]) ** 2 + (dy + oy - f[..., 1]) ** 2
                            + (dz + oz - f[..., 2]) ** 2)
                closer = d < f1
                f2 = np.where(closer, f1, np.minimum(f2, d))
                cid = np.where(closer, _hash3(cx, cy, cz, seed + 307), cid)
                f1 = np.where(closer, d, f1)
    return f1, f2, cid


def smoothstep(a, b, x):
    t = np.clip((np.asarray(x, dtype=np.float64) - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def mix(a, b, t):
    """Blend colours/arrays; t broadcast over the trailing colour axis."""
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    t = np.asarray(t, dtype=np.float64)
    if t.ndim >= 1 and max(a.ndim, b.ndim) != t.ndim:
        t = t[..., None]              # per-texel factor against a colour axis
    return a * (1.0 - t) + b * t
