# SPDX-License-Identifier: MIT
"""Elephant-only surface corrections; prepared source pixels remain unchanged."""
import numpy as np


def linear(rgb):
    return np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)


def srgb(rgb):
    return np.where(rgb <= .0031308, rgb * 12.92, 1.055 * np.maximum(rgb, 0) ** (1 / 2.4) - .055)


def ear_uv(data, shape):
    """Planar coordinates keep the shallow, thick source ears free of UV stretching."""
    vertices = data['vertices']
    ear = data['tags'][:, data['tag_names'].tolist().index('ear')] > .5
    faces = np.all(ear[data['triangles']], axis=1)
    points = vertices[data['triangles'][faces]]
    # Both ears sample the same continuous photographic flank patch. The atlas
    # tile is resampled at 246 source texels/metre, comparable to the torso.
    minimum = np.array([np.abs(vertices[ear, 1]).min(), vertices[ear, 2].min()]) - .025
    extent = np.array([np.abs(vertices[ear, 1]).max(), vertices[ear, 2].max()]) + .025 - minimum
    shape['ear_extent'] = extent.tolist()
    coords = (np.stack([np.abs(points[..., 1]), points[..., 2]], axis=-1) - minimum) / extent
    data['uv'][faces] = np.array([1320, 8]) / 2048 + coords * np.array([512, 1024]) / 2048



def resample_ears(pixels, shape):
    """Reuse actual body wrinkles, including their matching tangent normal detail."""
    size = pixels.shape[0]
    # Work in bottom-up image coordinates, as used by Blender and prepared UVs.
    y0, y1 = round(0 * size / 2048), round(1040 * size / 2048)
    x0, x1 = round(1312 * size / 2048), round(1840 * size / 2048)
    y, x = np.mgrid[y0:y1, x0:x1]
    u = (x * 2048 / size - 1320) / 512
    v = (y * 2048 / size - 8) / 1024
    extent = shape['ear_extent']
    donor_x = np.clip(((.315 + .12 * extent[0] * u) * size).astype(int), 0, size-1)
    donor_y = np.clip(((.115 + .12 * extent[1] * v) * size).astype(int), 0, size-1)
    pixels[y0:y1, x0:x1] = pixels[donor_y, donor_x]


def correct_texture(pixels, role, shape):
    """Colour is transformed in linear light; AO remains linear scalar data."""
    rgba = pixels.copy()
    rgb = rgba[..., :3]
    size = len(rgb)
    if role != 'occlusion':
        resample_ears(rgba, shape)
    y, x = np.mgrid[:size, :size] / (size / 2048)
    skin = (y < 1040) & (x < 1840)
    ivory = (y >= 1040) & (y < 1568) & (x < 528)
    if role == 'base':
        light = linear(rgb)
        lum = light @ np.array([.2126, .7152, .0722])
        # Retain the photographed wrinkle contrast, while lifting dusty skin.
        value = shape.get('skin_floor', .105) + shape.get('skin_contrast', 1.45) * lum
        color = value[..., None] * np.array([1.065, .992, .920])
        rgb[skin] = srgb(np.clip(color[skin], 0, 1))
        # Keep subtle source ivory variation, without its baked brown shadowing.
        ivory_value = .53 + .22 * np.clip((y - 1160) / 320, 0, 1) + .04 * np.clip(lum / .35, 0, 1)
        ivory_color = ivory_value[..., None] * np.array([1.04, .985, .85])
        rgb[ivory] = srgb(np.clip(ivory_color[ivory], 0, 1))
    elif role == 'normal':
        rgb[ivory] = [.5, .5, 1]
        if shape.get('calf_skin'):
            rgb[skin, :2] = .5 + .48 * (rgb[skin, :2] - .5)
    else:
        # Preserve deep creases but remove broad, baked environmental shadowing.
        rgb[:] = 1 - .65 * (1 - rgb) ** 1.5
        rgb[ivory] = 1
        ear = (y < 1040) & (x >= 1312) & (x < 1840)
        rgb[ear] = .96
    return rgba
