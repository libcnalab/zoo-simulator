# SPDX-License-Identifier: MIT
"""Checks of the procedural animation maths that need no Blender: the leg solver, the
gaits (no foot sliding, no over-extension, seamless loops) and the standing clips.

    python3 -m unittest discover -s tools/animals/tests
"""
import importlib
import json
import math
import os
import sys
import struct
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from zoogen import geom, quadruped  # noqa: E402
from zoogen.rig import Pose, Solver, quat_from_matrix, rot_x, rot_y, rot_z  # noqa: E402

CATS = ["lion", "lion_female", "lion_juvenile", "tiger"]
SPECIES = ["lion", "lion_female", "lion_juvenile", "tiger", "wolf", "wolf_juvenile",
           "meerkat_juvenile",
           "brown_bear", "polar_bear",
           "brown_bear_juvenile", "polar_bear_juvenile",
           "zebra", "zebra_female", "zebra_juvenile", "rhino", "camel", "elephant", "giraffe",
           "giraffe_female", "giraffe_juvenile", "meerkat",
           "penguin", "penguin_juvenile", "ostrich", "flamingo", "kangaroo"]


def make_solver(species):
    spec = importlib.import_module("species." + species).make_spec()
    bones = geom.expand_bones(spec.bones)
    rest = {}
    for b in bones:
        m = np.eye(4)
        m[:3, 3] = b.head
        rest[b.name] = m
    return spec, Solver(bones, rest, spec.rig)


def joints(world):
    return {n: m[:3, 3] for n, m in world.items()}


class RigTests(unittest.TestCase):
    def test_rotation_conventions(self):
        fwd, up, left = np.array([1.0, 0, 0]), np.array([0, 0, 1.0]), np.array([0, 1.0, 0])
        self.assertLess((rot_y(20) @ fwd)[2], 0.0)       # +ry: nose down
        self.assertGreater((rot_z(20) @ fwd)[1], 0.0)    # +rz: turn left
        self.assertGreater((rot_x(20) @ left)[2], 0.0)   # +rx: left side up
        self.assertTrue(np.allclose(rot_y(20) @ up, [math.sin(math.radians(20)), 0,
                                                     math.cos(math.radians(20))]))

    def test_quaternion(self):
        for m in (np.eye(3), rot_x(170), rot_y(-95) @ rot_z(40), rot_z(179.5), rot_x(180)):
            w, x, y, z = quat_from_matrix(m)
            back = np.array([
                [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
            self.assertTrue(np.allclose(back, m, atol=1e-9))

    def test_rest_pose_is_identity(self):
        for species in SPECIES:
            _, solver = make_solver(species)
            world = solver.solve(Pose())
            for n, m in solver.basis(world).items():
                self.assertTrue(np.allclose(m, np.eye(4), atol=1e-9), n)

    def test_planting_at_rest_changes_nothing(self):
        for species in SPECIES:
            _, solver = make_solver(species)
            world = solver.solve(quadruped.stand(solver, Pose()))
            self.assertAlmostEqual(solver.ik_error, 0.0)
            for n, p in joints(world).items():
                self.assertTrue(np.allclose(p, solver.head[n], atol=1e-6), n)
            for n, m in solver.basis(world).items():
                self.assertTrue(np.allclose(m, np.eye(4), atol=1e-6), n)

    def test_planted_feet_hold_when_the_body_moves(self):
        for species in SPECIES:
            _, solver = make_solver(species)
            rig = solver.rig
            pose = quadruped.stand(solver, Pose())
            pose.move(rig.body, dx=0.05, dy=0.03, dz=-0.08).r(rig.body, rx=3, ry=4, rz=-5)
            world = solver.solve(pose)
            self.assertAlmostEqual(solver.ik_error, 0.0)
            for key, leg in rig.legs.items():
                self.assertTrue(np.allclose(world[leg.toes][:3, 3], solver.toe_base(key), atol=1e-6))


class ClipTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = []
        for species in SPECIES:
            spec, solver = make_solver(species)
            cls.cases.append((species, solver, spec.clips(solver)))

    def test_clip_set(self):
        for species, _, clips in self.cases:
            names = [c.name for c in clips]
            for want in ("idle", "walk", "eat", "drink", "rest", "sleep", "vocalize"):
                self.assertIn(want, names, species)
            self.assertEqual(len(names), len(set(names)))

    def test_bone_lengths_never_change(self):
        for species, solver, clips in self.cases:
            for clip in clips:
                for i in range(12):
                    world = joints(solver.solve(clip.pose(i / 12)))
                    for n, p in solver.parent.items():
                        if p is None:
                            continue
                        moved = n in (solver.rig.body,)       # the body is translated on purpose
                        if moved:
                            continue
                        rest = np.linalg.norm(solver.head[n] - solver.head[p])
                        now = np.linalg.norm(world[n] - world[p])
                        self.assertAlmostEqual(rest, now, places=5, msg=f"{species}/{clip.name}/{n}")

    def test_feet_reach_their_targets(self):
        for species, solver, clips in self.cases:
            for clip in clips:
                solver.reset_stats()
                for i in range(24):
                    solver.solve(clip.pose(i / 24))
                self.assertLess(solver.ik_error, 1e-6, f"{species}/{clip.name}")
                self.assertLess(solver.extension, 0.99, f"{species}/{clip.name}")

    def test_loops_are_seamless_and_smooth(self):
        for species, solver, clips in self.cases:
            for clip in clips:
                n = max(16, int(round(clip.duration * 12)))
                prev = joints(solver.solve(clip.pose(0.0)))
                worst = 0.0
                for i in range(1, n + 1):
                    u = i / n
                    if clip.loop and i == n:
                        u = 0.0
                    cur = joints(solver.solve(clip.pose(u)))
                    worst = max(worst, max(np.linalg.norm(cur[k] - prev[k]) for k in cur))
                    prev = cur
                # No joint jumps: sampled movement must stay below the same 5 m/s bound.
                self.assertLess(worst, 5.0 * clip.duration / n, f"{species}/{clip.name}")
                if clip.loop:
                    a = joints(solver.solve(clip.pose(0.0)))
                    b = joints(solver.solve(clip.pose(1.0 - 1e-9)))
                    for k in a:
                        self.assertTrue(np.allclose(a[k], b[k], atol=1e-5), f"{species}/{clip.name}/{k}")

    def test_gaits_do_not_slide(self):
        for species, solver, clips in self.cases:
            for clip in clips:
                if clip.speed <= 0.0:
                    continue
                n = 48
                for key in getattr(clip, "contact_keys", solver.rig.legs):
                    leg = solver.rig.legs[key]
                    rest = solver.toe_base(key)
                    rest[2] = solver.rig.extra.get("contact_height", {}).get(key, rest[2])
                    on_ground = 0
                    for i in range(n):
                        u0, u1 = i / n, (i + 1) / n
                        p0 = solver.solve(clip.pose(u0))[leg.toes][:3, 3]
                        p1 = solver.solve(clip.pose(u1 % 1.0))[leg.toes][:3, 3]
                        if p0[2] > rest[2] + 1e-6 or p1[2] > rest[2] + 1e-6:
                            continue
                        on_ground += 1
                        # In the animal's frame a planted foot moves back at the ground speed.
                        v = (p1[0] - p0[0]) / (clip.duration / n)
                        self.assertAlmostEqual(v, -clip.speed, delta=0.02 * clip.speed + 1e-6,
                                               msg=f"{species}/{clip.name}/{key} at {u0:.3f}")
                        self.assertAlmostEqual(p0[1], rest[1], places=6)
                    self.assertGreater(on_ground, 0.2 * n)

    def test_gait_events_are_touchdowns(self):
        for species, solver, clips in self.cases:
            for clip in clips:
                if clip.speed <= 0.0:
                    continue
                self.assertEqual(len(clip.events), len(getattr(clip, "contact_keys", solver.rig.legs)))
                for u, name in clip.events:
                    self.assertEqual(name, "step")
                    self.assertTrue(0.0 <= u < 1.0)

    def test_nothing_sinks_into_the_ground(self):
        for species, solver, clips in self.cases:
            for clip in clips:
                for i in range(12):
                    world = joints(solver.solve(clip.pose(i / 12)))
                    for n, p in world.items():
                        if solver.parent[n] is None:
                            continue
                        self.assertGreater(p[2], 0.0, f"{species}/{clip.name}/{n}")

    def test_muzzle_reaches_food_and_water(self):
        for species, solver, clips in self.cases:
            by = {c.name: c for c in clips}
            eat = quadruped.muzzle(solver, by["eat"].pose(0.0))[2]
            drink = quadruped.muzzle(solver, by["drink"].pose(0.0))[2]
            if species.startswith("giraffe"):
                # Giraffes browse: their eat clip reaches up into the crowns, not down to the ground.
                self.assertGreater(eat, 1.5, species)
            else:
                self.assertLess(eat, 0.12, species)
            self.assertLess(drink, 0.06, species)
            self.assertGreater(drink, 0.0, species)


class AnatomyTests(unittest.TestCase):
    def test_meerkat_pup_has_juvenile_proportions_and_variant_metadata(self):
        adult = importlib.import_module("species.meerkat").make_spec()
        pup = importlib.import_module("species.meerkat_juvenile").make_spec()
        adult_parts = {part.name: part for part in adult.parts}
        pup_parts = {part.name: part for part in pup.parts}
        adult_height = adult.meta["shoulder_height_m"]
        pup_height = pup.meta["shoulder_height_m"]
        self.assertGreater(pup_parts["skull"].rings[1].ry / pup_height,
                           1.18 * adult_parts["skull"].rings[1].ry / adult_height)
        self.assertGreater(pup_parts["eye.L"].rings[0].ry / pup_height,
                           1.3 * adult_parts["eye.L"].rings[0].ry / adult_height)
        self.assertGreater(pup_parts["front_paw.L"].rings[-1].ry / pup_height,
                           1.15 * adult_parts["front_paw.L"].rings[-1].ry / adult_height)
        adult_face = (adult_parts["nose"].rings[-1].c[0] -
                      adult_parts["skull"].rings[0].c[0]) / adult.meta["body_length_m"]
        pup_face = (pup_parts["nose"].rings[-1].c[0] -
                    pup_parts["skull"].rings[0].c[0]) / pup.meta["body_length_m"]
        self.assertLess(pup_face, .75 * adult_face)
        self.assertEqual(pup.meta["variant_of"], "meerkat")
        self.assertEqual(pup.meta["variant"], "juvenile")
        self.assertAlmostEqual(pup.meta["relative_height"], pup_height / adult_height)
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        with open(os.path.join(root, "assets", "animals", "meerkat_juvenile.json")) as source:
            manifest = json.load(source)
        self.assertEqual(manifest["name"], "meerkat_juvenile")
        self.assertEqual(manifest["meta"], pup.meta)
        self.assertLessEqual(manifest["triangles"], 10000)
        clips = {clip["name"]: clip for clip in manifest["clips"]}
        self.assertEqual(set(clips), {"idle", "walk", "trot", "eat", "drink", "rest",
                                      "sleep", "vocalize", "sentry", "dig"})
        self.assertAlmostEqual(clips["walk"]["speed"], .42, delta=.02)
        self.assertAlmostEqual(clips["trot"]["speed"], 1.05, delta=.02)

    def test_wolf_pup_has_juvenile_proportions_and_variant_metadata(self):
        adult = importlib.import_module("species.wolf").make_spec()
        pup = importlib.import_module("species.wolf_juvenile").make_spec()
        adult_parts = {part.name: part for part in adult.parts}
        pup_parts = {part.name: part for part in pup.parts}
        adult_height = adult.meta["shoulder_height_m"]
        pup_height = pup.meta["shoulder_height_m"]
        self.assertGreater(pup_parts["skull"].rings[1].ry / pup_height,
                           1.2 * adult_parts["skull"].rings[1].ry / adult_height)
        self.assertGreater(pup_parts["front_paw.L"].rings[-1].ry / pup_height,
                           1.15 * adult_parts["front_paw.L"].rings[-1].ry / adult_height)
        adult_muzzle = adult_parts["skull"].rings[-1].c[0] - adult_parts["skull"].rings[2].c[0]
        pup_muzzle = pup_parts["skull"].rings[-1].c[0] - pup_parts["skull"].rings[2].c[0]
        self.assertLess(pup_muzzle / pup.meta["body_length_m"],
                        .8 * adult_muzzle / adult.meta["body_length_m"])
        self.assertEqual(pup.meta["variant_of"], "wolf")
        self.assertEqual(pup.meta["variant"], "juvenile")
        self.assertAlmostEqual(pup.meta["relative_height"], pup_height / adult_height)
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        with open(os.path.join(root, "assets", "animals", "wolf_juvenile.json")) as source:
            manifest = json.load(source)
        self.assertEqual(manifest["name"], "wolf_juvenile")
        self.assertEqual(manifest["meta"], pup.meta)
        self.assertLessEqual(manifest["triangles"], 17000)
        clips = {clip["name"]: clip for clip in manifest["clips"]}
        self.assertEqual(set(clips),
                         {"idle", "walk", "trot", "eat", "drink", "rest", "sleep", "vocalize"})
        self.assertAlmostEqual(clips["walk"]["speed"], .72, delta=.02)
        self.assertAlmostEqual(clips["trot"]["speed"], 1.65, delta=.02)

    def test_pillar_feet_have_flat_round_soles(self):
        from zoogen.anatomy import round_foot
        foot = round_foot("foot.L", 1.0, .5, .3, .28, "toes.L")
        vertices, _, _ = geom.loft(foot)
        self.assertAlmostEqual(float(vertices[:, 2].min()), .012)
        sole = vertices[np.abs(vertices[:, 2] - .012) < 1e-8]
        self.assertGreaterEqual(len(sole), foot.segments)
        distance = np.linalg.norm(sole[:-1, :2] - np.array([1.0, .5]), axis=1)
        self.assertTrue(np.allclose(distance, .3))

    def test_cloven_hooves_have_a_gap_and_mirror_weights(self):
        from zoogen.anatomy import split_foot
        digits = split_foot("hoof", 0, .4, .10, .22, .14, "fingers.L")
        left, _, _ = geom.loft(digits[0])
        right, _, _ = geom.loft(digits[1])
        self.assertLess(float(left[:, 1].max()), float(right[:, 1].min()))
        mirrored = geom.mirror_part(digits[0])
        self.assertEqual(mirrored.weights, {"fingers.R": 1.0})
        vertices, _, _ = geom.loft(mirrored)
        self.assertTrue(np.allclose(np.sort(vertices[:, 1]), np.sort(-left[:, 1])))

    def test_ungulate_leg_skins_overlap_their_foot_tops(self):
        for species, foot_name in (("camel", "pad"), ("giraffe", "hoof")):
            spec = importlib.import_module("species." + species).make_spec()
            for end in ("front", "hind"):
                leg = next(part for part in spec.parts if part.name == end + "_leg.L")
                digits = [part for part in spec.parts if part.name.startswith(end + "_" + foot_name)]
                skin_vertices = geom.loft(leg)[0]
                foot_vertices = np.concatenate([geom.loft(part)[0] for part in digits])
                self.assertLess(float(skin_vertices[:, 2].min()), float(foot_vertices[:, 2].max()),
                                species + "/" + end)
                # Remeshing shrinks thin contacts: bury the ankle across both digit centres.
                ankle = leg.rings[-1]
                self.assertLess(float(ankle.c[2]) + 2 * spec.voxel,
                                float(foot_vertices[:, 2].max()), species + "/" + end)
                for digit in digits:
                    self.assertGreater(ankle.ry, abs(float(digit.rings[0].c[1] - ankle.c[1])),
                                       species + "/" + end)

    def test_sentinel_has_planted_hind_feet_and_a_grounded_tail(self):
        for species, scale in (("meerkat", 1.0), ("meerkat_juvenile", .68)):
            spec, solver = make_solver(species)
            idle = next(clip for clip in spec.clips(solver) if clip.name == "idle")
            for phase in (0, .25, .5, .75):
                world = solver.solve(idle.pose(phase))
                for key in ("HL", "HR"):
                    toe = world[spec.rig.legs[key].toes][:3, 3]
                    self.assertAlmostEqual(float(toe[2]), float(solver.toe_base(key)[2]))
                tip = solver.point(world, "tail4", np.array([-.332, 0, .062]) * scale)
                self.assertAlmostEqual(float(tip[2]), .003 * scale, places=6)
                pelvis = world["hips"][:3, 3]
                shoulder = world["chest"][:3, 3]
                self.assertGreater(float(shoulder[2] - pelvis[2]), .12 * scale)
                self.assertLess(abs(float(shoulder[0] - pelvis[0])), .04 * scale)


BIPEDS = [species for species in SPECIES if species in ("penguin", "ostrich", "flamingo", "kangaroo")]


class BipedTests(unittest.TestCase):
    def test_height_metadata_and_geometry_agree_with_simulation(self):
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        for species in BIPEDS:
            spec, _ = make_solver(species)
            with open(os.path.join(root, "data", "species", species + ".json")) as source:
                data = json.load(source)
            self.assertEqual(spec.meta["shoulder_height_m"], data["shoulderHeightM"])
            vertices = (spec.sculpt['vertices'] if spec.sculpt is not None else
                        np.concatenate([geom.loft(part)[0] for part in geom.expand_parts(spec.parts)]))
            self.assertAlmostEqual(float(vertices[:, 2].max()), data["shoulderHeightM"],
                                   delta=.03 * data["shoulderHeightM"], msg=species)
            self.assertLessEqual(len(geom.expand_bones(spec.bones)), 60)

    def test_manifest_budgets_and_ground_speeds(self):
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        budgets = {"penguin": (6000, 12000), "ostrich": (12000, 24000),
                   "flamingo": (8000, 16000), "kangaroo": (12000, 24000)}
        for species in BIPEDS:
            with open(os.path.join(root, "data", "species", species + ".json")) as source:
                data = json.load(source)
            with open(os.path.join(root, "assets", "animals", species + ".json")) as source:
                manifest = json.load(source)
            self.assertGreaterEqual(manifest["triangles"], budgets[species][0])
            self.assertLessEqual(manifest["triangles"], budgets[species][1])
            self.assertEqual(manifest["texture"], 1024)
            clips = {clip["name"]: clip for clip in manifest["clips"]}
            # Activity clips beside the standard eight: tests/test_activity_clips.py.
            activity = {"penguin": {"preen", "huddle"}, "flamingo": {"preen", "filter_feed", "huddle"},
                        "ostrich": {"dust_bathe"}}
            self.assertEqual(set(clips),
                             {"idle", "walk", "trot", "eat", "drink", "rest", "sleep", "vocalize"}
                             | ({"swim", "float"} if species == "penguin" else set())
                             | activity.get(species, set()))
            self.assertAlmostEqual(clips["walk"]["speed"], data["walkSpeed"],
                                   delta=.1 * data["walkSpeed"])
            self.assertGreaterEqual(clips["trot"]["speed"], 2 * clips["walk"]["speed"])
            self.assertLessEqual(clips["trot"]["speed"], data["runSpeed"])

    def test_walking_birds_always_have_a_supporting_foot(self):
        for species in BIPEDS:
            if species == "kangaroo":
                continue
            spec, solver = make_solver(species)
            walk = next(clip for clip in spec.clips(solver) if clip.name == "walk")
            for index in range(120):
                world = solver.solve(walk.pose(index / 120))
                contacts = [world[leg.toes][2, 3] <= solver.toe_base(key)[2] + 1e-7
                            for key, leg in spec.rig.legs.items()]
                self.assertTrue(any(contacts), f"{species} at {index / 120}")

    def test_full_pose_loops_close_including_rotations(self):
        for species in BIPEDS:
            spec, solver = make_solver(species)
            for clip in spec.clips(solver):
                if not clip.loop:
                    continue
                first = solver.solve(clip.pose(0))
                last = solver.solve(clip.pose(1 - 1e-9))
                for name in first:
                    self.assertTrue(np.allclose(first[name], last[name], atol=1e-6),
                                    f"{species}/{clip.name}/{name}")

    def test_dense_gait_samples_have_reach_margin_and_feet_stay_on_their_side(self):
        for species in BIPEDS:
            spec, solver = make_solver(species)
            for clip in spec.clips(solver):
                if clip.name not in ("walk", "trot"):
                    continue
                solver.reset_stats()
                for index in range(120):
                    world = solver.solve(clip.pose(index / 120))
                    for key, leg in spec.rig.legs.items():
                        side = 1 if key.endswith("L") else -1
                        self.assertGreater(side * world[leg.toes][1, 3], 0, f"{species}/{clip.name}/{key}")
                self.assertLess(solver.ik_error, 1e-6, f"{species}/{clip.name}")
                self.assertLess(solver.extension, .99, f"{species}/{clip.name}")


    def test_signature_biped_anatomy(self):
        for species in BIPEDS:
            spec, _ = make_solver(species)
            if species in ("ostrich", "flamingo"):
                self.assertGreaterEqual(len(spec.rig.neck), 5 if species == "flamingo" else 4)
            if species == "penguin":
                for side in ("L", "R"):
                    expected = 3 if spec.sculpt is not None and spec.sculpt.get('birdscan') else 2
                    self.assertEqual(len(spec.rig.extra["wings"][side]), expected)
            if species == "kangaroo":
                self.assertGreaterEqual(len(spec.rig.tail), 5)
                self.assertEqual(len(spec.rig.ears), 2)

    def test_flamingo_one_leg_and_inverted_feeding(self):
        if "flamingo" not in BIPEDS:
            return
        spec, solver = make_solver("flamingo")
        clips = {clip.name: clip for clip in spec.clips(solver)}
        for name in ("idle", "rest", "sleep"):
            for phase in (0, .25, .5, .75):
                world = solver.solve(clips[name].pose(phase))
                self.assertAlmostEqual(world[spec.rig.legs["L"].toes][2, 3], solver.toe_base("L")[2])
                self.assertGreater(world[spec.rig.legs["R"].toes][2, 3], .6)
        for name in ("eat", "drink"):
            world = solver.solve(clips[name].pose(0))
            delta = world[spec.rig.head][:3, :3] @ solver.rest[spec.rig.head][:3, :3].T
            self.assertLess((delta @ np.array([0, 0, 1]))[2], 0, name)

    def test_kangaroo_hop_is_synchronous_with_flight(self):
        if "kangaroo" not in BIPEDS:
            return
        spec, solver = make_solver("kangaroo")
        hop = next(clip for clip in spec.clips(solver) if clip.name == "trot")
        flight = 0
        stance = 0
        for index in range(120):
            world = solver.solve(hop.pose(index / 120))
            left = world[spec.rig.legs["L"].toes][:3, 3]
            right = world[spec.rig.legs["R"].toes][:3, 3]
            self.assertTrue(np.allclose(left[[0, 2]], right[[0, 2]], atol=1e-8))
            if left[2] > solver.toe_base("L")[2] + .02:
                flight += 1
            if abs(left[2] - solver.toe_base("L")[2]) < 1e-7:
                stance += 1
        self.assertGreater(flight, 30)
        self.assertGreater(stance, 30)

    def test_kangaroo_crawl_supports_and_tail_do_not_slide(self):
        if "kangaroo" not in BIPEDS:
            return
        spec, solver = make_solver("kangaroo")
        walk = next(clip for clip in spec.clips(solver) if clip.name == "walk")
        tail_contacts = 0
        count = 120
        for index in range(count):
            phase = index / count
            world = solver.solve(walk.pose(phase))
            next_world = solver.solve(walk.pose(((index + 1) / count) % 1))
            heights = spec.rig.extra["contact_height"]
            supports = [abs(world[leg.toes][2, 3] - heights[key]) < 1e-6
                        for key, leg in spec.rig.legs.items()]
            self.assertTrue(any(supports), str(phase))
            left = world[spec.rig.legs["L"].toes][:3, 3]
            right = world[spec.rig.legs["R"].toes][:3, 3]
            self.assertTrue(np.allclose(left[[0, 2]], right[[0, 2]], atol=1e-8))
            tip = solver.point(world, spec.rig.tail[-1], spec.rig.extra["support_tail_tip"])
            next_tip = solver.point(next_world, spec.rig.tail[-1], spec.rig.extra["support_tail_tip"])
            if abs(tip[2] - .030) < 1e-6 and abs(next_tip[2] - .030) < 1e-6:
                tail_contacts += 1
                self.assertAlmostEqual((next_tip[0] - tip[0]) / (walk.duration / count),
                                       -walk.speed, delta=.02 * walk.speed)
                self.assertAlmostEqual(tip[1], 0, places=6)
                for key in ("FL", "FR"):
                    self.assertAlmostEqual(world[spec.rig.legs[key].toes][2, 3], heights[key], places=6)
        self.assertGreater(tail_contacts, .5 * count)


    def test_exported_body_positions_match_procedural_postures(self):
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        for species in BIPEDS + CATS:
            with open(os.path.join(root, "assets", "animals", species + ".glb"), "rb") as source:
                magic, version, _ = struct.unpack("<III", source.read(12))
                self.assertEqual(magic, 0x46546C67)
                self.assertEqual(version, 2)
                length, _ = struct.unpack("<II", source.read(8))
                document = json.loads(source.read(length))
                length, _ = struct.unpack("<II", source.read(8))
                binary = source.read(length)
            # Sculpt cats have separate source maps; their material contract is checked below.
            if species not in CATS:
                self.assertEqual(len(document["images"]), 1, species)
                texture = document["images"][0]
                self.assertEqual(texture["mimeType"], "image/png", species)
                texture_view = document["bufferViews"][texture["bufferView"]]
                texture_offset = texture_view.get("byteOffset", 0)
                self.assertEqual(binary[texture_offset:texture_offset + 8], b"\x89PNG\r\n\x1a\n")
                self.assertEqual(struct.unpack_from(">II", binary, texture_offset + 16), (1024, 1024))
            with open(os.path.join(root, "assets", "animals", species + ".json")) as source:
                manifest = json.load(source)
            triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                            for mesh in document["meshes"] for primitive in mesh["primitives"])
            self.assertEqual(triangles, manifest["triangles"], species)
            spec, solver = make_solver(species)
            body = next(index for index, node in enumerate(document["nodes"])
                        if node.get("name") == spec.rig.body)
            clips = {clip.name: clip for clip in spec.clips(solver)}
            self.assertEqual({animation["name"] for animation in document["animations"]}, set(clips))
            for animation in document["animations"]:
                channel = next(channel for channel in animation["channels"]
                               if channel["target"] == {"node": body, "path": "translation"})
                accessor = document["accessors"][animation["samplers"][channel["sampler"]]["output"]]
                view = document["bufferViews"][accessor["bufferView"]]
                offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
                position = np.array(struct.unpack_from("<3f", binary, offset))
                expected = solver.solve(clips[animation["name"]].pose(0))[spec.rig.body][:3, 3]
                # Animal XYZ becomes YZX in glTF after Blender's two coordinate transforms.
                # Blender stores rest joints as floats; long-chain reach fitting adds micrometre differences.
                self.assertTrue(np.allclose(position, expected[[1, 2, 0]], atol=2e-5),
                                f"{species}/{animation['name']}")


    def test_kangaroo_feeds_on_four_contacts_and_rests_on_an_elbow(self):
        if "kangaroo" not in BIPEDS:
            return
        spec, solver = make_solver("kangaroo")
        clips = {clip.name: clip for clip in spec.clips(solver)}
        for name in ("eat", "drink"):
            world = solver.solve(clips[name].pose(0))
            for key, leg in spec.rig.legs.items():
                self.assertAlmostEqual(world[leg.toes][2, 3], spec.rig.extra["contact_height"][key])
        world = solver.solve(clips["rest"].pose(0))
        self.assertLess(world["forearm.R"][2, 3], .06)
        self.assertGreater(world[spec.rig.head][2, 3], .35)



class SculptCatTests(unittest.TestCase):
    def test_prepared_weights_and_bind_contract(self):
        from zoogen.bigcat import SOURCE
        with np.load(SOURCE, allow_pickle=False) as data:
            weights = data["weights"]
            self.assertTrue(np.isfinite(weights).all())
            self.assertTrue((weights >= 0).all())
            self.assertTrue(((weights > 0).sum(axis=1) <= 4).all())
            self.assertTrue(np.allclose(weights.sum(axis=1), 1, atol=1e-6))
            self.assertEqual(len(data["vertices"]), len(weights))
            self.assertEqual(data["uv"].shape, (len(data["triangles"]), 3, 2))
            self.assertGreaterEqual(data["triangles"].min(), 0)
            self.assertLess(data["triangles"].max(), len(weights))
            self.assertAlmostEqual(data["vertices"][:, 2].min(), 0, places=6)
            names = data["bone_names"].tolist()
            for index, name in enumerate(names):
                if name.endswith(".L"):
                    other = names.index(name[:-1] + "R")
                    self.assertTrue(np.allclose(data["heads"][index] * [1, -1, 1],
                                                data["heads"][other], atol=1e-6))
        self.assertLess(sum(path.stat().st_size for path in SOURCE.parent.iterdir() if path.is_file()),
                        14_000_000)

    def test_all_four_rest_paws_are_grounded(self):
        for species in CATS:
            spec, _ = make_solver(species)
            data = spec.sculpt
            names = data["bone_names"].tolist()
            for side in (".L", ".R"):
                for chain in (("hand", "fingers"), ("foot", "toes")):
                    mask = data["weights"][:, [names.index(name + side) for name in chain]].sum(axis=1) > .8
                    sole = data["vertices"][mask, 2].min()
                    self.assertGreaterEqual(sole, -1e-5)
                    self.assertLess(sole, .005, f"{species}/{chain}/{side}: floating paw")

    def test_recumbent_bones_clear_ground_and_chest_lies_down(self):
        for species in CATS:
            spec, solver = make_solver(species)
            bones = geom.expand_bones(spec.bones)
            for clip in spec.clips(solver):
                if clip.name not in ("rest", "sleep", "bathe"):
                    continue
                for index in range(24):
                    world = solver.solve(clip.pose(index / 24))
                    limit = .48 if clip.name == "bathe" else .45
                    self.assertLess(world[spec.rig.chest][2, 3], limit * spec.meta["shoulder_height_m"],
                                    f"{species}/{clip.name}/chest")
                    for bone in bones:
                        if not bone.deform:
                            continue
                        for point in (bone.head, bone.tail):
                            self.assertGreaterEqual(solver.point(world, bone.name, point)[2], -.02,
                                                    f"{species}/{clip.name}/{bone.name}")

    def test_recumbent_skin_clears_ground(self):
        for species in CATS:
            spec, solver = make_solver(species)
            data = spec.sculpt
            vertices = data["vertices"]
            for clip in spec.clips(solver):
                if clip.name not in ("rest", "sleep", "bathe"):
                    continue
                for phase in (0, .125, .25, .375, .5, .625, .75, .875):
                    world = solver.solve(clip.pose(phase))
                    posed = np.zeros_like(vertices)
                    for index, name in enumerate(data["bone_names"].tolist()):
                        matrix = world[name]
                        posed += data["weights"][:, index, None] * (
                            (vertices - solver.head[name]) @ matrix[:3, :3].T + matrix[:3, 3])
                    self.assertGreaterEqual(posed[:, 2].min(), -.002, f"{species}/{clip.name}/{phase}")

    def test_lateral_walk_footfalls(self):
        for species in CATS:
            spec, solver = make_solver(species)
            walk = next(clip for clip in spec.clips(solver) if clip.name == "walk")
            contacts = []
            previous = None
            for index in range(-1, 121):
                world = solver.solve(walk.pose((index / 120) % 1))
                current = {key: abs(world[leg.toes][2, 3] - solver.toe_base(key)[2]) < 1e-8
                           for key, leg in spec.rig.legs.items()}
                if previous is not None and index < 120:
                    contacts.extend(key for key in current if current[key] and not previous[key])
                previous = current
            self.assertEqual(contacts, ["HL", "FL", "HR", "FR"], species)

    def test_cat_manifests_and_export_budgets(self):
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        for species in CATS:
            with open(os.path.join(root, "assets", "animals", species + ".json")) as source:
                manifest = json.load(source)
            self.assertLessEqual(manifest["triangles"], 24000 if species == "lion_juvenile" else 42000 if species == "lion" else 34000)
            self.assertLessEqual(manifest["bones"], 60)
            self.assertEqual(manifest["texture"], 1024)
            self.assertEqual(manifest["license"], "CC-BY-4.0")
            clips = {clip["name"]: clip for clip in manifest["clips"]}
            expected = {"idle", "walk", "trot", "eat", "drink", "rest", "sleep", "vocalize"}
            if species == "tiger":
                expected.add("bathe")
            self.assertEqual(set(clips), expected)
            expected_height = dict(lion=1.15, lion_female=1.05, lion_juvenile=.45, tiger=1.0)[species]
            self.assertAlmostEqual(manifest["meta"]["shoulder_height_m"], expected_height)
            if species.startswith("lion_"):
                self.assertEqual(manifest["meta"]["variant_of"], "lion")
                self.assertEqual(manifest["meta"]["variant"], species[5:])
            if species == "lion_female":
                self.assertAlmostEqual(manifest["meta"]["relative_height"], .91)
            if species == "lion_juvenile":
                self.assertAlmostEqual(clips["walk"]["speed"], .6, delta=.02)
                self.assertAlmostEqual(clips["trot"]["speed"], 1.5, delta=.02)
            with open(os.path.join(root, "assets", "animals", species + ".glb"), "rb") as source:
                header = source.read(12)
                self.assertEqual(header[:4], b"glTF")
                length, _ = struct.unpack("<II", source.read(8))
                document = json.loads(source.read(length))
                length, _ = struct.unpack("<II", source.read(8))
                binary = source.read(length)
            self.assertLessEqual(os.path.getsize(os.path.join(root, "assets", "animals", species + ".glb")),
                                 4_500_000 if species == "lion_juvenile" else 7_000_000)
            opaque = [material for material in document["materials"]
                      if material.get("alphaMode", "OPAQUE") == "OPAQUE"]
            self.assertEqual(len(opaque), 1)
            self.assertIn("normalTexture", opaque[0])
            self.assertIn("occlusionTexture", opaque[0])
            self.assertGreaterEqual(opaque[0]["pbrMetallicRoughness"]["roughnessFactor"], .75)
            self.assertLessEqual(opaque[0]["pbrMetallicRoughness"]["roughnessFactor"], .9)
            self.assertEqual(len(document["images"]), 4)
            self.assertEqual(sorted(image["mimeType"] for image in document["images"]),
                             ["image/jpeg", "image/jpeg", "image/jpeg", "image/png"])
            for mesh in document["meshes"]:
                for primitive in mesh["primitives"]:
                    self.assertIn("TANGENT", primitive["attributes"])
            hair = [material for material in document["materials"]
                    if material.get("alphaMode") == "MASK"]
            self.assertEqual(len(hair), 1)
            self.assertTrue(hair[0]["doubleSided"])
            self.assertAlmostEqual(hair[0].get("alphaCutoff", .5), .5)
            triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                            for mesh in document["meshes"] for primitive in mesh["primitives"])
            self.assertEqual(triangles, manifest["triangles"])
            self.assertEqual({animation["name"] for animation in document["animations"]}, set(clips))


if __name__ == "__main__":
    unittest.main()
