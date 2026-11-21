# SPDX-License-Identifier: MIT
"""Greater capybara with a blunt muzzle, high-set senses and partly webbed feet."""
import math
from functools import partial

import numpy as np

from zoogen import noise as nz
from zoogen import quadruped as quad
from zoogen.geom import Part, R
from zoogen.rig import Clip, Pose
from zoogen.wild_quadruped import make_wild_quadruped


def _body(spec, h, length, pup):
    parts = {part.name: part for part in spec.parts}
    spec.parts.remove(parts["tail"])
    spec.bones = [bone for bone in spec.bones if not bone.name.startswith("tail")]
    spec.rig.tail = []

    def ring(x, z, width, depth, exponent=2):
        return R((x * length, 0, z * h), width * h, depth * h, n=exponent)

    # The broad rump is the tallest part of the back. Blend the leg roots under the
    # torso to keep a continuous outline all the way to the short, thick neck.
    parts["torso"].rings = [
        ring(-.47, .66, .13, .17), ring(-.39, .70, .27, .31),
        ring(-.25, .70, .32, .36), ring(-.06, .67, .33, .36),
        ring(.11, .68, .30, .32), ring(.23, .72, .21, .24)]
    parts["front_leg.L"].rings[0].c[2] = .80 * h
    parts["front_leg.L"].rings[1].c[2] = .78 * h
    parts["neck"].rings = [ring(.14, .70, .20, .23), ring(.21, .78, .21, .22),
                           ring(.29, .88, .17, .18)]
    head_scale = 1.12 if pup else 1
    parts["skull"].rings = [
        ring(.22, .86, .14 * head_scale, .18), ring(.30, .91, .205 * head_scale, .21),
        ring(.39, .88, .196 * head_scale, .19, 2.5),
        ring(.48, .825, .16, .145, 3), ring(.515, .83, .12, .115, 3)]
    parts["jaw"].rings = [ring(.31, .76, .17, .09), ring(.43, .73, .15, .08),
                          ring(.50, .75, .111, .055)]
    # The end of a capybara muzzle is broad and furred. Only the two nostril
    # openings are dark. A continuous black nose pad gives the face a canine look.
    spec.parts.remove(parts["nose"])
    for name, x, y, z, width, depth, tag in [
            ("nostril.L", .500, .105, .910, .024, .012, "nostril"),
            ("eye.L", .347, .182 * head_scale, 1.000, .033 if pup else .029,
             .030 if pup else .026, "eye")]:
        rings = [R((x * length, y * h, z * h), width * h, depth * h)]
        if name in parts:
            parts[name].rings = rings
            parts[name].side_ref = (1, 0, 0)
            parts[name].cap_scale = .60
        else:
            spec.parts.append(Part(name, rings, weights={"head": 1.0}, tag=tag, segments=12,
                                   mirror=True, fuse=False, side_ref=(1, 0, 0), cap_scale=.4))
    spec.parts.append(Part(
        "mouth_corner.L",
        [R((.432 * length, .160 * h, .753 * h), .009 * h, .006 * h),
         R((.462 * length, .155 * h, .749 * h), .012 * h, .008 * h),
         R((.495 * length, .126 * h, .750 * h), .010 * h, .007 * h),
         R((.512 * length, .091 * h, .753 * h), .006 * h, .004 * h)],
        weights={"jaw": 1.0}, tag="lip", mirror=True, fuse=False, segments=8, subdiv=2))
    parts["ear.L"].rings = [
        R((.264 * length, .132 * h, 1.002 * h), .058 * h, .044 * h),
        R((.248 * length, .155 * h, 1.086 * h), .057 * h, .048 * h),
        R((.246 * length, .157 * h, 1.126 * h), .026 * h, .023 * h)]

    # The flattened pad extends between the digit bases. The tips fan out free of the web.
    for front, count in [(True, 4), (False, 3)]:
        name = "front" if front else "hind"
        bone = "fingers.L" if front else "toes.L"
        foot = parts[name + "_paw.L"]
        x, y, _ = foot.rings[-1].c
        for r in foot.rings:
            r.up = r.dn = .035 * h
            r.c[2] = .046 * h
        for i in range(count):
            side = i - .5 * (count - 1)
            reach = (.12 - .019 * abs(side)) * h
            spec.parts.append(Part(
                f"{name}_digit_{i + 1}.L",
                [R((x - .015 * h, y + .032 * h * side, .049 * h), .027 * h, .030 * h),
                 R((x + .048 * h, y + .045 * h * side, .038 * h), .021 * h, .025 * h),
                 R((x + reach, y + .053 * h * side, .022 * h), .017 * h, .016 * h)],
                weights={bone: 1.0}, tag="digit", segments=10, mirror=True, fuse=False))


def _paint(ctx):
    grain = nz.fbm(ctx.P * np.array([.60, 1.20, 1.65]), 60, 3, seed=263)
    belly = nz.smoothstep(.05, -.76, ctx.N[..., 2])
    coat = nz.mix(np.array([.43, .26, .135]), np.array([.63, .45, .27]), .64 * belly)
    coat = coat * (.79 + .40 * grain)[..., None]
    coat = nz.mix(coat, np.array([.24, .155, .10]), .56 * ctx.tag("ear"))
    coat = nz.mix(coat, np.array([.19, .125, .086]), ctx.tag("lip"))
    coat = nz.mix(coat, np.array([.22, .16, .105]), .72 * ctx.tag("digit"))
    return nz.mix(coat, np.array([.045, .030, .019]),
                  np.clip(3 * ctx.tag("eye") + 2 * ctx.tag("nostril"), 0, 1))


def _fur(ctx):
    hair = nz.fbm(ctx.P * np.array([.48, 1.13, 1.7]), 88, 3, seed=269)
    bare = np.clip(ctx.tag("eye") * 3 + ctx.tag("lip") + ctx.tag("nostril"), 0, 1)
    return .5 + (hair - .5) * (1 - bare)


def _paddle(solver, height, resting, phase):
    angle = math.tau * phase
    effort = .20 if resting else 1.0
    pose = Pose().move("hips", dz=.01 * height * math.sin(2 * angle))
    pose.r("neck1", ry=-8).r("head", ry=-5)
    pose.r("hips", rx=effort * math.sin(angle))
    for key in solver.rig.legs:
        front = key.startswith("F")
        left = key.endswith("L")
        stroke = angle + (0 if left else math.pi) + (.70 * math.pi if front else 0)
        amplitude = (.15 if front else .21) * effort
        foot = solver.toe_base(key)
        foot += height * np.array([
            -.05 + amplitude * math.sin(stroke),
            (1 if left else -1) * (.025 + .025 * effort * math.cos(stroke)),
            .24 + .07 * effort * math.cos(stroke)])
        pose.plant(key, foot, foot_pitch=-10 + effort * 20 * math.sin(stroke), toe_pitch=-7)
    return pose


def make_spec(juvenile=False):
    height, length = (.32, .60) if juvenile else (.60, 1.17)
    spec = make_wild_quadruped(dict(
        name="capybara_juvenile" if juvenile else "capybara", kind="capybara",
        common_name="Capybara", scientific_name="Hydrochoerus hydrochaeris",
        height=height, length=length, walk_pattern="pace",
        walk_speed=.44 if juvenile else .71, trot_speed=.96 if juvenile else 1.57,
        walk_period=.60 if juvenile else .85, trot_period=.40 if juvenile else .56,
        walk_lift=.080 * height, fast_lift=.13 * height, fast_swing_pitch=18,
        walk_drop=(.03 if juvenile else .055) * height, fast_drop=.16 * height, idle_shift=.012 * height,
        idle_bob=.005 * height, lie_drop=.55 * height, hind_tuck=.14 * height,
        hind_out=.09 * height, head_down=quad.HeadDown(body_pitch=20, crouch=.16 * height,
                                                      chest=15, neck=(48, 38), head=24),
        voxel=.006 if juvenile else .008, target_tris=15500 if juvenile else 18000))
    _body(spec, height, length, juvenile)
    spec.paint, spec.surface_height, spec.surface_strength = _paint, _fur, 1.12
    spec.meta.update(anatomy="arched barrel rump, blunt muzzle, high senses, webbed four/three toes",
                     swim=dict(clip="swim", depth_m=.87 * height),
                     float=dict(clip="float", depth_m=.87 * height))
    if juvenile:
        spec.meta.update(variant_of="capybara", variant="juvenile", relative_height=height / .60)
    land_clips = spec.clips

    def clips(solver):
        result = land_clips(solver)
        for name, duration, speed, resting in [("swim", 1.22, 1.10 * height, False),
                                               ("float", 2.8, 0.0, True)]:
            clip = Clip(name, duration, partial(_paddle, solver, height, resting), speed=speed)
            clip.contact_keys = []
            result.append(clip)
        return result

    spec.clips = clips
    return spec
