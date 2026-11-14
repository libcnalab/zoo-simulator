# SPDX-License-Identifier: MIT
"""Localized juvenile surface relaxation; skeleton and paw contacts stay intact."""
import numpy as np


def relax(vertices, edges, counts, mask, strength):
    mean = np.zeros_like(vertices)
    np.add.at(mean, edges[:, 0], vertices[edges[:, 1]])
    mean /= np.maximum(counts[:, None], 1)
    vertices += strength * mask[:, None] * (mean - vertices)


def soften_cub(data):
    names = data['bone_names'].tolist()
    muscle = data['weights'][:, [i for i, name in enumerate(names)
                                if name.startswith(('scapula', 'upper_arm', 'thigh'))]].sum(axis=1)
    mask = np.clip(muscle * 1.5, 0, 1)
    vertices = data['vertices'].copy()
    triangles = data['triangles']
    edges = np.concatenate([triangles[:, [0, 1]], triangles[:, [1, 2]], triangles[:, [2, 0]]])
    edges = np.concatenate([edges, edges[:, ::-1]])
    counts = np.bincount(edges[:, 0], minlength=len(vertices))
    for _ in range(120):
        relax(vertices, edges, counts, mask, .35)
    lower = data['weights'][:, [i for i, name in enumerate(names)
                               if name.startswith(('forearm', 'shin'))]].sum(axis=1)
    # Alternating smoothing retains the short leg's volume while removing adult tendon relief.
    for _ in range(45):
        relax(vertices, edges, counts, lower, .45)
        relax(vertices, edges, counts, lower, -.46)
    data['vertices'] = vertices
