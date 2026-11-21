# SPDX-License-Identifier: MIT
"""Grey wolf adult and older-pup variant."""
from zoogen import quadruped as quad
from zoogen.geom import R
from zoogen.wild_quadruped import make_wild_quadruped


FEMALE_RELATIVE_HEIGHT = .95


def _refine_anatomy(spec, juvenile=False):
    """Replace the generic quadruped sections with a lean, deep-chested canid outline.

    The old adult skull doubled back from x=.53 to x=.45 before reaching the nose.  Besides
    making a rounded knot, that hid the broad wolf muzzle inside the cranium.  These sections
    are strictly rear-to-front and leave the skeleton and all gait contacts untouched.
    """
    height = spec.meta["shoulder_height_m"]
    length = spec.meta["body_length_m"]
    head_z = .91 * height
    nose_x = .50 * length
    muzzle_length = .66 if juvenile else 1.0
    skull_scale = 1.36 if juvenile else 1.12
    muzzle_width = 1.15 if juvenile else 1.10
    parts = {part.name: part for part in spec.parts}

    parts["torso"].rings = [
        R((-.48 * length, 0, .84 * height), .08 * height, .10 * height),
        R((-.31 * length, 0, .69 * height), .16 * height, .18 * height, .17 * height),
        R((-.12 * length, 0, .69 * height), .17 * height, .19 * height, .14 * height),
        R(( .03 * length, 0, .70 * height), .18 * height, .24 * height, .22 * height),
        R(( .14 * length, 0, .72 * height), .18 * height, .25 * height, .22 * height),
        R(( .24 * length, 0, .73 * height), .14 * height, .20 * height, .18 * height),
    ]
    parts["neck"].rings = [
        R((.15 * length, 0, .71 * height), .12 * height, .18 * height, .17 * height),
        R((.23 * length, 0, .78 * height), .10 * height, .15 * height, .15 * height),
        R((.285 * length, 0, .87 * height), .085 * height, .12 * height, .12 * height),
    ]
    parts["neck_ruff"].rings = [
        R((.12 * length, 0, .72 * height), .17 * height, .22 * height, .25 * height),
        R((.21 * length, 0, .79 * height), .15 * height, .19 * height, .23 * height),
        R((.29 * length, 0, .87 * height), .11 * height, .14 * height, .17 * height),
    ]

    parts["skull"].rings = [
        R((nose_x - .22 * length, 0, head_z), .075 * height * skull_scale,
          .105 * height * skull_scale),
        R((nose_x - .145 * length, 0, head_z - .005 * height),
          .105 * height * skull_scale, .115 * height * skull_scale, n=2.3),
        R((nose_x - .105 * length * muzzle_length, 0, head_z - .020 * height),
          .10 * height * skull_scale, .095 * height * skull_scale, n=2.4),
        R((nose_x - .10 * length * muzzle_length, 0, head_z - .045 * height),
          .08 * height * muzzle_width, .070 * height, n=2.5),
        R((nose_x - .055 * length * muzzle_length, 0, head_z - .052 * height),
          .065 * height * muzzle_width, .058 * height, n=2.5),
        R((nose_x - .018 * length, 0, head_z - .060 * height),
          .050 * height * muzzle_width, .045 * height, n=2.6),
    ]
    parts["jaw"].rings = [
        R((nose_x - .13 * length * muzzle_length, 0, head_z - .080 * height),
          .070 * height * muzzle_width, .032 * height),
        R((nose_x - .075 * length * muzzle_length, 0, head_z - .095 * height),
          .060 * height * muzzle_width, .028 * height),
        R((nose_x - .030 * length, 0, head_z - .105 * height),
          .050 * height * muzzle_width, .024 * height),
    ]

    # Wolves have relatively small erect ears with slightly rounded tips, not needle points.
    ear_x = nose_x - .20 * length
    parts["ear.L"].rings = [
        R((ear_x, .090 * height, head_z + .030 * height), .050 * height,
          .040 * height),
        R((ear_x - .010 * length, .110 * height, head_z + .115 * height),
          .045 * height, .045 * height),
        R((ear_x - .018 * length, .118 * height, head_z + .165 * height),
          .015 * height, .012 * height),
    ]
    eye = (nose_x - .155 * length, .075 * height, head_z + .015 * height)
    eye_size = .023 * height if juvenile else .018 * height
    parts["eye.L"].rings = [R(eye, eye_size, eye_size)]
    parts["nose"].rings = [
        R((nose_x - .015 * length, 0, head_z - .035 * height), .042 * height,
          .029 * height),
        R((nose_x + .010 * length, 0, head_z - .045 * height), .036 * height,
          .024 * height),
    ]

    # Keep the long digitigrade bones and large feet, but remove the spherical limb sleeves.
    limb_radii = {
        "front_leg.L": ((.070, .085), (.052, .060), (.042, .050), (.044, .038)),
        "hind_leg.L": ((.070, .085), (.065, .075), (.047, .055), (.044, .038)),
    }
    for name, radii in limb_radii.items():
        for ring, (width, depth) in zip(parts[name].rings, radii):
            ring.ry = width * height
            ring.up = ring.dn = depth * height

    brush = parts["tail_tuft"].rings
    for ring, (width, depth) in zip(brush,
                                    ((.065, .065), (.078, .072), (.050, .046), (.012, .012))):
        ring.ry = width * height
        ring.up = ring.dn = depth * height

    spec.meta["anatomy"] = ("Third-pass canid outline: monotonic broad muzzle, narrow deep "
                            "chest, abdominal tuck and lean digitigrade limbs")


def _female_shape(spec):
    """Give the adult female Yellowstone's modestly smaller, lighter outline."""
    factors = {"torso": .94, "neck": .94, "neck_ruff": .90,
               "skull": .95, "jaw": .95,
               "front_leg.L": .94, "hind_leg.L": .94,
               "front_paw.L": .94, "hind_paw.L": .94,
               "tail": .93, "tail_tuft": .92}
    for part in spec.parts:
        factor = factors.get(part.name)
        if factor is None:
            continue
        for ring in part.rings:
            ring.ry *= factor
            ring.up *= factor
            ring.dn *= factor
    spec.name = "wolf_female"
    spec.meta.update({"common_name": "Grey wolf female", "sex": "female",
                      "variant_of": "wolf", "variant": "female",
                      "relative_height": FEMALE_RELATIVE_HEIGHT,
                      "age_shape": "Adult female: slightly shorter and lighter than the male"})


def make_spec(juvenile=False, female=False):
    if juvenile and female:
        raise ValueError("juvenile and female wolf variants are mutually exclusive")
    height = .46 if juvenile else .8
    spec = make_wild_quadruped({
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
    _refine_anatomy(spec, juvenile)
    if female:
        _female_shape(spec)
    return spec
