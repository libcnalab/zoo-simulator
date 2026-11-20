# SPDX-License-Identifier: MIT
"""Pure-numpy surface maps shared by the procedural animal builders."""

import numpy as np


def height_to_normal(height, strength=2.0):
    """Turn a UV-space height field into an OpenGL tangent-space normal RGB image."""
    height = np.asarray(height, dtype=np.float64)
    if height.ndim != 2:
        raise ValueError("surface height must be a two-dimensional array")
    dv, du = np.gradient(height)
    normal = np.stack([-du * strength, -dv * strength, np.ones_like(height)], axis=-1)
    normal /= np.maximum(np.linalg.norm(normal, axis=-1, keepdims=True), 1e-12)
    return normal * 0.5 + 0.5
