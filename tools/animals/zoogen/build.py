# SPDX-License-Identifier: MIT
"""Blender side of the generator: fuse parts into one skin, transfer skin weights,
unwrap, paint the coat texture, build the armature, export GLB and render previews."""
import json
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

from . import geom
from . import noise as nz


def to_blender(p):
    """animal space (x fwd, y left, z up) -> Blender (x right... -Y forward, Z up)."""
    p = np.asarray(p, dtype=np.float64)
    return np.stack([p[..., 1], -p[..., 0], p[..., 2]], axis=-1)


def from_blender(p):
    p = np.asarray(p, dtype=np.float64)
    return np.stack([-p[..., 1], p[..., 0], p[..., 2]], axis=-1)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.render.fps = 30
    return scene


def _mesh_object(name, verts, tris):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], [tuple(int(i) for i in t) for t in tris])
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def _activate(ob):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob


def _fuse(verts_b, tris, spec):
    """Voxel-remesh the union of all fused parts into one watertight skin."""
    ob = _mesh_object("fuse_src", verts_b, tris)
    _activate(ob)
    m = ob.modifiers.new("remesh", 'REMESH')
    m.mode = 'VOXEL'
    m.voxel_size = spec.voxel
    m.adaptivity = 0.0
    m.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier=m.name)
    if spec.smooth_iters > 0:
        s = ob.modifiers.new("smooth", 'LAPLACIANSMOOTH')
        s.iterations = spec.smooth_iters
        s.lambda_factor = 0.6
        s.use_volume_preserve = True
        bpy.ops.object.modifier_apply(modifier=s.name)
    ntri = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    if ntri > spec.target_tris:
        d = ob.modifiers.new("decimate", 'DECIMATE')
        d.ratio = spec.target_tris / ntri
        bpy.ops.object.modifier_apply(modifier=d.name)
    t = ob.modifiers.new("tri", 'TRIANGULATE')
    bpy.ops.object.modifier_apply(modifier=t.name)
    me = ob.data
    v = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", v)
    f = np.empty(len(me.polygons) * 3, dtype=np.int32)
    me.polygons.foreach_get("vertices", f)
    bpy.data.objects.remove(ob, do_unlink=True)
    return v.reshape(-1, 3), f.reshape(-1, 3).astype(np.int64)


def _transfer(src_v, src_t, src_attr, dst_v):
    """Nearest-surface transfer of per-vertex attributes (rows of src_attr)."""
    bvh = BVHTree.FromPolygons([tuple(v) for v in src_v], [tuple(int(i) for i in t) for t in src_t])
    out = np.zeros((len(dst_v), src_attr.shape[1]))
    for i, p in enumerate(dst_v):
        loc, _n, idx, _d = bvh.find_nearest(Vector(p))
        a, b, c = src_t[idx]
        pa, pb, pc = src_v[a], src_v[b], src_v[c]
        v0, v1, v2 = pb - pa, pc - pa, np.array(loc) - pa
        d00, d01, d11 = v0 @ v0, v0 @ v1, v1 @ v1
        d20, d21 = v2 @ v0, v2 @ v1
        den = d00 * d11 - d01 * d01
        if abs(den) < 1e-18:
            out[i] = src_attr[a]
            continue
        v = (d11 * d20 - d01 * d21) / den
        w = (d00 * d21 - d01 * d20) / den
        u = 1.0 - v - w
        u, v, w = np.clip([u, v, w], 0.0, 1.0)
        s = u + v + w
        out[i] = (u * src_attr[a] + v * src_attr[b] + w * src_attr[c]) / s
    return out


def _laplacian(attr, tris, iters, lam=0.5):
    """Smooth per-vertex rows over the mesh's edges."""
    e = np.concatenate([tris[:, [0, 1]], tris[:, [1, 2]], tris[:, [2, 0]]], axis=0)
    e = np.unique(np.sort(e, axis=1), axis=0)
    deg = np.zeros(len(attr))
    np.add.at(deg, e[:, 0], 1.0)
    np.add.at(deg, e[:, 1], 1.0)
    deg = np.maximum(deg, 1.0)
    a = attr.copy()
    for _ in range(iters):
        acc = np.zeros_like(a)
        np.add.at(acc, e[:, 0], a[e[:, 1]])
        np.add.at(acc, e[:, 1], a[e[:, 0]])
        a = (1.0 - lam) * a + lam * acc / deg[:, None]
    return a


def _limit_influences(w, k=4):
    idx = np.argsort(-w, axis=1)[:, :k]
    out = np.zeros_like(w)
    rows = np.arange(len(w))[:, None]
    out[rows, idx] = w[rows, idx]
    out[out < 0.02] = 0.0
    s = out.sum(axis=1, keepdims=True)
    s[s < 1e-9] = 1.0
    return out / s


class PaintContext:
    """What a species' ``paint`` callback sees: per-texel fields of the rest pose."""

    def __init__(self, size, tri_id, bary, tris, verts_animal, normals_animal, tags, tag_names,
                 weights, bone_names):
        self.size = size
        self.filled = tri_id >= 0
        self._tid = np.where(self.filled, tri_id, 0)
        self._bary = bary
        self._tris = tris
        self._tags = tags
        self._tag_names = tag_names
        self._weights = weights
        self._bone_names = bone_names
        self.P = self.sample(verts_animal)           # (H, W, 3) rest position, animal space
        n = self.sample(normals_animal)
        self.N = n / np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-9)
        self.x, self.y, self.z = self.P[..., 0], self.P[..., 1], self.P[..., 2]

    def sample(self, per_vertex):
        per_vertex = np.asarray(per_vertex, dtype=np.float64)
        tv = self._tris[self._tid]                    # (H, W, 3)
        if per_vertex.ndim == 1:
            return (self._bary * per_vertex[tv]).sum(axis=-1)
        return (self._bary[..., None] * per_vertex[tv]).sum(axis=-2)

    def tag(self, name):
        """Soft mask in [0,1] of the texels that came from parts tagged ``name``."""
        if name not in self._tag_names:
            return np.zeros(self.filled.shape)
        return self.sample(self._tags[:, self._tag_names.index(name)])

    def bones(self, *prefixes):
        """Soft mask: summed skin weight of the bones whose names start with a prefix."""
        cols = [i for i, b in enumerate(self._bone_names) if b.startswith(tuple(prefixes))]
        if not cols:
            return np.zeros(self.filled.shape)
        return self.sample(self._weights[:, cols].sum(axis=1))


def _rasterize(uv, size):
    """uv: (T, 3, 2) -> tri_id (H, W) int, bary (H, W, 3)."""
    tri_id = np.full((size, size), -1, dtype=np.int64)
    bary = np.zeros((size, size, 3))
    px = uv * size
    for t in range(len(px)):
        (x0, y0), (x1, y1), (x2, y2) = px[t]
        minx = max(int(math.floor(min(x0, x1, x2) - 1.0)), 0)
        maxx = min(int(math.ceil(max(x0, x1, x2) + 1.0)), size - 1)
        miny = max(int(math.floor(min(y0, y1, y2) - 1.0)), 0)
        maxy = min(int(math.ceil(max(y0, y1, y2) + 1.0)), size - 1)
        if maxx < minx or maxy < miny:
            continue
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-12:
            continue
        xs = np.arange(minx, maxx + 1) + 0.5
        ys = np.arange(miny, maxy + 1) + 0.5
        gx, gy = np.meshgrid(xs, ys)
        a = ((y1 - y2) * (gx - x2) + (x2 - x1) * (gy - y2)) / den
        b = ((y2 - y0) * (gx - x2) + (x0 - x2) * (gy - y2)) / den
        c = 1.0 - a - b
        # One texel of slack beyond the edges keeps bilinear filtering off the gutter.
        eps = 1.0 / max(math.sqrt(abs(den)), 1e-6)
        inside = (a >= -eps) & (b >= -eps) & (c >= -eps)
        strict = (a >= 0) & (b >= 0) & (c >= 0)
        sub_id = tri_id[miny:maxy + 1, minx:maxx + 1]
        take = strict | (inside & (sub_id < 0))
        if not take.any():
            continue
        sub_id[take] = t
        sub_b = bary[miny:maxy + 1, minx:maxx + 1]
        sub_b[take] = np.stack([a, b, c], axis=-1)[take]
    return tri_id, bary


def _dilate(rgb, filled, iters=8):
    rgb = rgb.copy()
    filled = filled.copy()
    for _ in range(iters):
        acc = np.zeros_like(rgb)
        cnt = np.zeros(filled.shape)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            f = np.roll(filled, (dy, dx), axis=(0, 1))
            acc += np.roll(rgb, (dy, dx), axis=(0, 1)) * f[..., None]
            cnt += f
        grow = (~filled) & (cnt > 0)
        rgb[grow] = acc[grow] / cnt[grow][:, None]
        filled |= grow
    if (~filled).any() and filled.any():
        rgb[~filled] = rgb[filled].mean(axis=0)
    return rgb


def build_mesh(spec):
    """Create the skinned mesh object (without armature). Returns (object, info)."""
    if spec.sculpt is not None and spec.sculpt.get("giraffe2", False):
        from .giraffe2 import build_giraffe
        return build_giraffe(spec)
    if spec.sculpt is not None and spec.sculpt.get("rigged", False):
        from .rigged import build_rigged
        return build_rigged(spec)
    if spec.sculpt is not None:
        from .sculpt import build_sculpt
        return build_sculpt(spec)
    bones = geom.expand_bones(spec.bones)
    bone_names = [b.name for b in bones]
    bidx = {n: i for i, n in enumerate(bone_names)}
    parts = geom.expand_parts(spec.parts)
    tag_names = sorted({p.tag for p in parts})

    def dense(wts, tag):
        w = np.zeros((len(wts), len(bone_names)))
        for i, d in enumerate(wts):
            for b, x in d.items():
                if b not in bidx:
                    raise KeyError(f"part weight names unknown bone '{b}'")
                w[i, bidx[b]] = x
        t = np.zeros((len(wts), len(tag_names)))
        t[:, tag_names.index(tag)] = 1.0
        return w, t

    fv, ft, fw, ftag = [], [], [], []
    hv, ht, hw, htag = [], [], [], []
    for p in parts:
        v, t, wts = geom.loft(p)
        w, tg = dense(wts, p.tag)
        V, T, W, G = (fv, ft, fw, ftag) if p.fuse else (hv, ht, hw, htag)
        off = sum(len(a) for a in V)
        V.append(v)
        T.append(t + off)
        W.append(w)
        G.append(tg)

    src_v = to_blender(np.concatenate(fv))
    src_t = np.concatenate(ft)
    src_attr = np.concatenate([np.concatenate(fw), np.concatenate(ftag)], axis=1)
    skin_v, skin_t = _fuse(src_v, src_t, spec)
    attr = _transfer(src_v, src_t, src_attr, skin_v)
    nb = len(bone_names)
    w = _laplacian(attr[:, :nb], skin_t, spec.meta.get("weight_smooth", 10))
    tags = _laplacian(attr[:, nb:], skin_t, spec.meta.get("tag_smooth", 3))
    verts, tris = skin_v, skin_t
    if hv:
        hvb = to_blender(np.concatenate(hv))
        tris = np.concatenate([tris, np.concatenate(ht) + len(verts)])
        verts = np.concatenate([verts, hvb])
        w = np.concatenate([w, np.concatenate(hw)])
        tags = np.concatenate([tags, np.concatenate(htag)])
    w = _limit_influences(w, 4)

    ob = _mesh_object(spec.name, verts, tris)
    me = ob.data
    if spec.validate_mesh:
        me.validate(verbose=False, clean_customdata=False)
    for poly in me.polygons:
        poly.use_smooth = True
    _activate(ob)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=0.004)
    bpy.ops.object.mode_set(mode='OBJECT')
    me = ob.data

    # Texture painting from per-texel rest-pose fields.
    nloops = len(me.loops)
    uv = np.empty(nloops * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    lv = np.empty(nloops, dtype=np.int32)
    me.loops.foreach_get("vertex_index", lv)
    ptris = lv.reshape(-1, 3).astype(np.int64)        # polygons are triangles, loops in order
    vn = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("normal", vn)
    vco = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", vco)
    size = spec.texture_size
    tri_id, bary = _rasterize(uv.reshape(-1, 3, 2), size)
    ctx = PaintContext(size, tri_id, bary, ptris, from_blender(vco.reshape(-1, 3)),
                       from_blender(vn.reshape(-1, 3)), tags, tag_names, w, bone_names)
    rgb = spec.paint(ctx) if spec.paint else np.full((size, size, 3), 0.6)
    rgb = _dilate(np.clip(rgb, 0.0, 1.0), ctx.filled)
    img = bpy.data.images.new(spec.name + "_albedo", size, size, alpha=False)
    rgba = np.concatenate([rgb, np.ones((size, size, 1))], axis=-1).astype(np.float32)
    img.pixels.foreach_set(rgba.ravel())
    img.pack()

    mat = bpy.data.materials.new(spec.name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = img
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = spec.meta.get("roughness", 0.85)
    bsdf.inputs["Metallic"].default_value = 0.0
    me.materials.append(mat)

    for i, name in enumerate(bone_names):
        vg = ob.vertex_groups.new(name=name)
        col = w[:, i]
        for vi in np.nonzero(col > 0.0)[0]:
            vg.add([int(vi)], float(col[vi]), 'REPLACE')
    info = {"bones": bones, "bone_names": bone_names, "triangles": int(len(me.polygons)),
            "vertices": int(len(verts)), "image": img}
    return ob, info


def build_armature(spec, bones):
    arm = bpy.data.armatures.new(spec.name + "_rig")
    ob = bpy.data.objects.new(spec.name + "_rig", arm)
    bpy.context.scene.collection.objects.link(ob)
    _activate(ob)
    bpy.ops.object.mode_set(mode='EDIT')
    ebs = {}
    for b in bones:
        eb = arm.edit_bones.new(b.name)
        eb.head = Vector(to_blender(b.head))
        eb.tail = Vector(to_blender(b.tail))
        if (eb.tail - eb.head).length < 1e-4:
            eb.tail = eb.head + Vector((0, 0, 0.02))
        eb.use_deform = b.deform
        ebs[b.name] = eb
    for b in bones:
        if b.parent:
            ebs[b.name].parent = ebs[b.parent]
            ebs[b.name].use_connect = False
    for eb in arm.edit_bones:
        eb.align_roll(Vector((1.0, 0.0, 0.0)))      # animal's left
    bpy.ops.object.mode_set(mode='OBJECT')
    return ob


def bind(mesh_ob, arm_ob):
    mesh_ob.parent = arm_ob
    m = mesh_ob.modifiers.new("armature", 'ARMATURE')
    m.object = arm_ob


def export_glb(path, mesh_ob, arm_ob, tangents=False, all_bones=False):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    mesh_ob.select_set(True)
    arm_ob.select_set(True)
    bpy.context.view_layer.objects.active = arm_ob
    bpy.ops.export_scene.gltf(
        filepath=path, export_format='GLB', use_selection=True, export_yup=True,
        export_apply=False, export_skins=True, export_animations=True,
        export_animation_mode='NLA_TRACKS', export_force_sampling=True,
        export_frame_range=False, export_anim_slide_to_zero=True,
        export_optimize_animation_size=True, export_def_bones=not all_bones,
        export_all_influences=False, export_image_format='AUTO',
        export_materials='EXPORT', export_tangents=tangents, export_normals=True,
        export_texcoords=True, export_morph=False, export_cameras=False,
        export_lights=False, export_extras=False)


def write_manifest(path, spec, info, clips):
    bones = info["bones"]
    lo = np.min([np.minimum(b.head, b.tail) for b in bones], axis=0)
    hi = np.max([np.maximum(b.head, b.tail) for b in bones], axis=0)
    data = {
        "name": spec.name,
        "generator": "tools/animals (zoogen)",
        "license": spec.meta.get("asset_license", "MIT"),
        "triangles": info["triangles"],
        "vertices": info["vertices"],
        "bones": len(bones),
        "texture": spec.texture_size,
        "skeleton_bounds_animal_space": {"min": [round(float(x), 4) for x in lo],
                                         "max": [round(float(x), 4) for x in hi]},
        "clips": clips,
        "meta": {k: v for k, v in spec.meta.items()
                 if isinstance(v, (int, float, str, list, dict, bool))},
    }
    with open(path, "w") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")
    return data
