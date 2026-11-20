# SPDX-License-Identifier: MIT
"""Grey wolf adult and older-pup variant."""
from zoogen import quadruped as quad
from zoogen.wild_quadruped import make_wild_quadruped


def make_spec(juvenile=False):
    height = .46 if juvenile else .8
    return make_wild_quadruped({
        "name": "wolf_juvenile" if juvenile else "wolf",
        "kind": "wolf",
        "common_name": "Grey wolf",
        "scientific_name": "Canis lupus",
        "height": height,
        "length": .78 if juvenile else 1.5,
        "walk_speed": .72 if juvenile else 1.2,
        "trot_speed": 1.65 if juvenile else 2.4,
        "walk_period": .78 if juvenile else 1.15,
        "trot_period": .48 if juvenile else .62,
        "walk_lift": .05 if juvenile else .08,
        "fast_lift": .075 if juvenile else .12,
        "lie_drop": .27 if juvenile else .48,
        "hind_tuck": .06 if juvenile else .10,
        "hind_out": .04 if juvenile else .07,
        "head_down": (quad.HeadDown(body_pitch=24, crouch=.07, chest=17,
                                     neck=(58, 48), head=25)
                      if juvenile else None),
        "idle_shift": .007 if juvenile else .012,
        "idle_bob": .0025 if juvenile else .004,
        "skull_scale": 1.28 if juvenile else 1,
        "muzzle_width_scale": 1.10 if juvenile else 1,
        "muzzle_length_scale": .66 if juvenile else 1,
        "ear_scale": 1.16 if juvenile else 1,
        "eye_scale": 1.28 if juvenile else 1,
        "paw_scale": 1.22 if juvenile else 1,
        "juvenile": juvenile,
        "target_tris": 15000 if juvenile else 18000,
        "voxel": .008 if juvenile else .010,
        "meta": ({"variant_of": "wolf", "variant": "juvenile",
                  "relative_height": height / .8,
                  "age_shape": "Older pup: shorter muzzle and body, broader skull and paws"}
                 if juvenile else {}),
    })
