# SPDX-License-Identifier: MIT
"""Dedicated anatomy and coat variants for the three new wild canids."""
from zoogen.geom import R
from zoogen.quadruped import HeadDown
from zoogen.wild_quadruped import make_wild_quadruped

from species.wolf import _refine_anatomy


_KINDS = {
    "dingo": {
        "common_name": "Dingo",
        "scientific_name": "Canis familiaris (dingo)",
        "height": .55,
        "length": 1.00,
        "walk_speed": 1.05,
        "run_speed": 5.4,
    },
    "african_wild_dog": {
        "common_name": "African wild dog",
        "scientific_name": "Lycaon pictus",
        "height": .75,
        "length": 1.10,
        "walk_speed": 1.15,
        "run_speed": 6.7,
    },
    "dhole": {
        "common_name": "Dhole",
        "scientific_name": "Cuon alpinus",
        "height": .50,
        "length": .90,
        "walk_speed": 1.00,
        "run_speed": 5.8,
    },
}


def _reshape(spec, variant, juvenile):
    parts = {part.name: part for part in spec.parts}
    height = spec.meta["shoulder_height_m"]
    length = spec.meta["body_length_m"]

    # These species are shorter-coated than a northern grey wolf. Retaining a smaller fused
    # neck mass preserves the canid silhouette without presenting all three as recoloured wolves.
    ruff_scale = {"dingo": .55, "african_wild_dog": .42, "dhole": .68}[variant]
    for ring in parts["neck_ruff"].rings:
        ring.ry *= ruff_scale
        ring.up *= ruff_scale
        ring.dn *= ruff_scale

    if variant == "dingo":
        # Broad skull, long tapered muzzle and prominent pricked ears.
        for ring in parts["skull"].rings[:3]:
            ring.ry *= 1.08
        for ring in parts["ear.L"].rings:
            ring.ry *= 1.12
            ring.up *= 1.10
            ring.dn *= 1.10
    elif variant == "african_wild_dog":
        # The huge rounded ears and narrow waist are the painted dog's strongest landmarks.
        ear_x = .50 * length - .20 * length
        head_z = .91 * height
        parts["ear.L"].rings = [
            R((ear_x, .105 * height, head_z + .025 * height), .085 * height, .060 * height),
            R((ear_x - .010 * length, .135 * height, head_z + .135 * height),
              .095 * height, .075 * height),
            R((ear_x - .016 * length, .142 * height, head_z + .205 * height),
              .062 * height, .050 * height),
        ]
        for ring in parts["torso"].rings[1:4]:
            ring.ry *= .88
        for ring in parts["front_leg.L"].rings + parts["hind_leg.L"].rings:
            ring.ry *= .90
    else:
        # Dholes have a shorter, heavier muzzle than wolves and softly rounded ears.
        for ring in parts["skull"].rings[2:]:
            ring.c[0] -= .025 * length
            ring.ry *= 1.08
        tip = parts["ear.L"].rings[-1]
        tip.ry = .025 * height
        tip.up = tip.dn = .020 * height

    spec.meta["anatomy"] = {
        "dingo": "Lean short coat, broad head, tapered muzzle and prominent pricked ears",
        "african_wild_dog": "Long-legged painted dog with narrow waist and oversized rounded ears",
        "dhole": "Compact red canid with broad short muzzle, rounded ears and dark brush tail",
    }[variant]
    if juvenile:
        spec.meta["age_shape"] = "Older pup: shorter muzzle and body, broader skull and paws"


def make_canid(variant, juvenile=False):
    values = _KINDS[variant]
    relative = .61 if juvenile else 1.0
    name = variant + ("_juvenile" if juvenile else "")
    config = {
        "name": name,
        "kind": "wolf",
        "coat_variant": variant,
        "common_name": values["common_name"],
        "scientific_name": values["scientific_name"],
        "height": values["height"] * relative,
        "length": values["length"] * (.64 if juvenile else 1.0),
        "walk_speed": values["walk_speed"] * (.66 if juvenile else 1.0),
        "trot_speed": values["run_speed"] * (.25 if juvenile and variant == "dhole"
                                                else .34 if juvenile else .43),
        "walk_period": .72 if juvenile else 1.05,
        "trot_period": .46 if juvenile else .58,
        "walk_lift": .045 if juvenile else .07,
        "fast_lift": .04 if juvenile and variant == "dhole" else .07 if juvenile else .11,
        "lie_drop": .24 if juvenile else .40,
        "hind_tuck": .05 if juvenile else .09,
        "hind_out": .04 if juvenile else .065,
        "skull_scale": 1.28 if juvenile else 1.0,
        "muzzle_width_scale": 1.10 if juvenile else 1.0,
        "muzzle_length_scale": .68 if juvenile else 1.0,
        "ear_scale": 1.13 if juvenile else 1.0,
        "eye_scale": 1.25 if juvenile else 1.0,
        "paw_scale": 1.18 if juvenile else 1.0,
        "juvenile": juvenile,
        "target_tris": 14500 if juvenile else 17500,
        "voxel": .007 if juvenile else .009,
        "meta": ({"variant_of": variant, "variant": "juvenile",
                  "relative_height": relative} if juvenile else {}),
    }
    if variant == "african_wild_dog":
        # Long legs need a deeper crouch than the generic canid pose for the muzzle to meet
        # ground forage and water instead of hovering above them.
        config["head_down"] = HeadDown(body_pitch=30.0, crouch=.27 * relative, chest=21.0,
                                        neck=(70.0, 60.0), head=31.0)
        config["drink_height"] = .06
    spec = make_wild_quadruped(config)
    _refine_anatomy(spec, juvenile)
    _reshape(spec, variant, juvenile)
    return spec
