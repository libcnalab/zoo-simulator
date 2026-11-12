# SPDX-License-Identifier: MIT
"""Checks of the procedural animation maths that need no Blender: the leg solver, the
gaits (no foot sliding, no over-extension, seamless loops) and the standing clips.

    python3 -m unittest discover -s tools/animals/tests
"""
import importlib
import math
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from zoogen import geom, quadruped  # noqa: E402
from zoogen.rig import Pose, Solver, quat_from_matrix, rot_x, rot_y, rot_z  # noqa: E402

SPECIES = ["lion"]


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
                for i in range(24):
                    world = joints(solver.solve(clip.pose(i / 24)))
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
                for i in range(48):
                    solver.solve(clip.pose(i / 48))
                self.assertLess(solver.ik_error, 1e-6, f"{species}/{clip.name}")
                self.assertLess(solver.extension, 0.99, f"{species}/{clip.name}")

    def test_loops_are_seamless_and_smooth(self):
        for species, solver, clips in self.cases:
            for clip in clips:
                n = max(24, int(round(clip.duration * 30)))
                prev = joints(solver.solve(clip.pose(0.0)))
                worst = 0.0
                for i in range(1, n + 1):
                    u = i / n
                    if clip.loop and i == n:
                        u = 0.0
                    cur = joints(solver.solve(clip.pose(u)))
                    worst = max(worst, max(np.linalg.norm(cur[k] - prev[k]) for k in cur))
                    prev = cur
                # No joint jumps: at 30 Hz nothing in these clips moves faster than 5 m/s.
                self.assertLess(worst, 5.0 / 30.0, f"{species}/{clip.name}")
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
                n = 120
                for key, leg in solver.rig.legs.items():
                    rest = solver.toe_base(key)
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
                self.assertEqual(len(clip.events), len(solver.rig.legs))
                for u, name in clip.events:
                    self.assertEqual(name, "step")
                    self.assertTrue(0.0 <= u < 1.0)

    def test_nothing_sinks_into_the_ground(self):
        for species, solver, clips in self.cases:
            for clip in clips:
                for i in range(24):
                    world = joints(solver.solve(clip.pose(i / 24)))
                    for n, p in world.items():
                        if solver.parent[n] is None:
                            continue
                        self.assertGreater(p[2], 0.0, f"{species}/{clip.name}/{n}")

    def test_muzzle_reaches_food_and_water(self):
        for species, solver, clips in self.cases:
            by = {c.name: c for c in clips}
            eat = quadruped.muzzle(solver, by["eat"].pose(0.0))[2]
            drink = quadruped.muzzle(solver, by["drink"].pose(0.0))[2]
            self.assertLess(eat, 0.12, species)
            self.assertLess(drink, 0.06, species)
            self.assertGreater(drink, 0.0, species)


if __name__ == "__main__":
    unittest.main()
