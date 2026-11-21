# SPDX-License-Identifier: MIT
"""The activity clips beside the standard eight (zoogen/activity.py): the meerkat's sentry duty
and digging, preening and filter-feeding birds, dust-bathing, wallowing and ruminant chewing.
What each must show, and that the manifests map activities to clips the models really carry.

    python3 -m unittest discover -s tools/animals/tests
"""
import json
import os
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from test_clips import make_solver  # noqa: E402
from zoogen import quadruped  # noqa: E402
from zoogen.activity import held  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]
# Activity names as the save format writes them (src/core/zoo/animals/Animals.cpp).
ACTIVITIES = {"idle", "wander", "forage", "eat", "drink", "rest", "sleep", "play", "socialize",
              "vocalize", "pace", "shelter", "break_out", "loose", "crated", "browse", "ruminate",
              "bathe", "dust_bathe", "preen", "sentinel", "dig", "filter_feed", "patrol", "huddle"}
# The clips a still animal may play (AnimalRenderer's still poses).
STILL_CLIPS = {"idle", "eat", "drink", "rest", "sleep", "sentry", "dig", "preen", "dust_bathe",
               "ruminate", "filter_feed", "bathe", "huddle", "browse", "nuzzle", "groom"}
EXPECTED = {
    "brown_bear": {"bathe": "bathe"},
    "brown_bear_juvenile": {"bathe": "bathe"},
    "polar_bear": {"bathe": "bathe"},
    "polar_bear_juvenile": {"bathe": "bathe"},
    "camel": {"ruminate": "ruminate"},
    "camel_female": {"ruminate": "ruminate"},
    "camel_juvenile": {"ruminate": "ruminate"},
    "meerkat": {"sentinel": "sentry", "dig": "dig"},
    "meerkat_juvenile": {"sentinel": "sentry", "dig": "dig"},
    "penguin": {"preen": "preen", "huddle": "huddle"},
    "penguin_juvenile": {"preen": "preen", "huddle": "huddle"},
    "flamingo": {"huddle": "huddle", "preen": "preen", "filter_feed": "filter_feed"},
    "flamingo_juvenile": {"huddle": "huddle", "preen": "preen",
                            "filter_feed": "filter_feed"},
    "ostrich": {"dust_bathe": "dust_bathe"},
    "ostrich_female": {"dust_bathe": "dust_bathe"},
    "ostrich_juvenile": {"dust_bathe": "dust_bathe"},
    "elephant": {"dust_bathe": "dust_bathe", "bathe": "dust_bathe"},
    "elephant_female": {"dust_bathe": "dust_bathe", "bathe": "dust_bathe"},
    "elephant_juvenile": {"dust_bathe": "dust_bathe", "bathe": "dust_bathe"},
    "rhino": {"bathe": "bathe"},
    "rhino_juvenile": {"bathe": "bathe"},
    "tiger": {"bathe": "bathe"},
    "giraffe": {"browse": "browse", "ruminate": "ruminate"},
    "giraffe_female": {"browse": "browse", "ruminate": "ruminate"},
    "giraffe_juvenile": {"browse": "browse", "ruminate": "ruminate"},
    "lion": {"socialize": "nuzzle"},
    "lion_female": {"socialize": "nuzzle"},
    "lion_juvenile": {"socialize": "nuzzle"},
    "kangaroo": {"socialize": "groom"},
    "kangaroo_female": {"socialize": "groom"},
    "kangaroo_juvenile": {"socialize": "groom"},
    "zebra": {"sentinel": "sentry"},
    "zebra_female": {"sentinel": "sentry"},
    "zebra_juvenile": {"sentinel": "sentry"},
}


def clips_of(species):
    spec, solver = make_solver(species)
    return spec, solver, {clip.name: clip for clip in spec.clips(solver)}


def posed_skin(solver, world, data):
    points = np.zeros_like(data["vertices"])
    for index, name in enumerate(data["bone_names"].tolist()):
        rotation = world[name][:3, :3] @ solver.rest[name][:3, :3].T
        points += data["weights"][:, index, None] * (
            (data["vertices"] - solver.head[name]) @ rotation.T + world[name][:3, 3])
    return points


class HeldKeyTests(unittest.TestCase):
    def test_holds_eases_and_wraps(self):
        keys = [(0.0, 0.0), (0.5, 10.0, .1)]
        self.assertEqual(held(0.2, keys), 0.0)
        self.assertEqual(held(0.39, keys), 0.0)
        self.assertAlmostEqual(held(0.45, keys), 5.0)
        self.assertEqual(held(0.7, keys), 10.0)
        self.assertAlmostEqual(held(0.97, keys), 5.0)       # back to the first key over .06
        self.assertAlmostEqual(held(0.0, keys), held(1.0 - 1e-12, keys), places=6)
        self.assertTrue(np.allclose(held(0.45, [(0.0, (0, 2)), (0.5, (10, 4), .1)]), (5, 3)))


class ManifestTests(unittest.TestCase):
    def test_activity_clips_name_known_activities_and_existing_clips(self):
        for species, expected in EXPECTED.items():
            spec, _, clips = clips_of(species)
            mapping = spec.meta["activity_clips"]
            self.assertEqual(mapping, expected, species)
            manifest = json.loads((ROOT / "assets/animals" / (species + ".json")).read_text())
            self.assertEqual(manifest["meta"]["activity_clips"], expected, species)
            exported = {clip["name"]: clip for clip in manifest["clips"]}
            for activity, clip in mapping.items():
                self.assertIn(activity, ACTIVITIES, species)
                self.assertIn(clip, STILL_CLIPS, species)
                self.assertIn(clip, clips, species)
                self.assertIn(clip, exported, species)
                self.assertEqual(exported[clip]["speed"], 0, species)
                self.assertTrue(exported[clip]["loop"], species)


class MeerkatTests(unittest.TestCase):
    def test_sentry_stands_tall_on_planted_feet_and_the_tail(self):
        for species, scale in (("meerkat", 1.0), ("meerkat_juvenile", .68)):
            spec, solver, clips = clips_of(species)
            idle_head = max(solver.solve(clips["idle"].pose(u / 12))["head"][2, 3]
                            for u in range(12))
            yaws = []
            for index in range(36):
                world = solver.solve(clips["sentry"].pose(index / 36))
                for key in ("HL", "HR"):
                    toe = world[spec.rig.legs[key].toes][:3, 3]
                    self.assertTrue(np.allclose(toe[[1, 2]], solver.toe_base(key)[[1, 2]], atol=1e-6))
                tip = solver.point(world, "tail4", np.array([-.332, 0, .062]) * scale)
                self.assertAlmostEqual(float(tip[2]), .003 * scale, places=5)
                pelvis, shoulder = world["hips"][:3, 3], world["chest"][:3, 3]
                self.assertGreater(float(shoulder[2] - pelvis[2]), .13 * scale)
                self.assertLess(abs(float(shoulder[0] - pelvis[0])), .03 * scale)
                # Taller than the sitting idle: up on the hind feet, not on the haunches.
                self.assertGreater(world["head"][2, 3], idle_head + .04 * scale)
                muzzle = solver.point(world, "jaw", solver.tail["jaw"]) - world["head"][:3, 3]
                yaws.append(np.degrees(np.arctan2(muzzle[1], muzzle[0])))
            # The watch turns to both sides.
            self.assertGreater(max(yaws), 35)
            self.assertLess(min(yaws), -35)

    def test_dig_scratches_with_the_fore_paws_nose_low(self):
        for species, scale in (("meerkat", 1.0), ("meerkat_juvenile", .68)):
            spec, solver, clips = clips_of(species)
            lifted = {"FL": 0, "FR": 0}
            travel = {"FL": [], "FR": []}
            for index in range(90):
                pose = clips["dig"].pose(index / 90)
                world = solver.solve(pose)
                for key in ("HL", "HR"):
                    self.assertAlmostEqual(world[spec.rig.legs[key].toes][2, 3],
                                           solver.toe_base(key)[2])
                for key in lifted:
                    toe = world[spec.rig.legs[key].toes][:3, 3]
                    lifted[key] += toe[2] > solver.toe_base(key)[2] + .01 * scale
                    travel[key].append(toe[0])
                self.assertLess(quadruped.muzzle(solver, pose)[2], .10 * scale)
            for key in lifted:
                self.assertGreater(lifted[key], 10, key)
                self.assertGreater(np.ptp(travel[key]), .04 * scale, key)


class PreenTests(unittest.TestCase):
    def test_bill_works_on_the_plumage(self):
        # Moments when the bird holds a preening posture, and how close the bill tip must be
        # to the body's skin (the scan's feathers): it touches, it does not stand off.
        moments = {"penguin": (.15, .25, .47, .73, .8), "penguin_juvenile": (.15, .47, .8),
                   "flamingo": (.15, .35, .55, .75),
                   "flamingo_juvenile": (.15, .35, .55, .75)}
        for species, phases in moments.items():
            spec, solver, clips = clips_of(species)
            data = spec.sculpt
            names = data["bone_names"].tolist()
            head = data["weights"][:, [i for i, n in enumerate(names)
                                       if n.startswith(("neck", "head", "jaw"))]].sum(axis=1) > .3
            for phase in phases:
                pose = clips["preen"].pose(phase)
                world = solver.solve(pose)
                tip = quadruped.muzzle(solver, pose)
                body = posed_skin(solver, world, data)[~head]
                distance = np.linalg.norm(body - tip, axis=1).min()
                limit = (.03 * spec.meta["relative_height"]
                         if species == "flamingo_juvenile" else .03
                         if species == "flamingo" else
                         .035 * spec.meta["shoulder_height_m"] / .65)
                self.assertLess(distance, limit, f"{species} at {phase}")
                for key, leg in spec.rig.legs.items():
                    self.assertAlmostEqual(world[leg.toes][2, 3], solver.toe_base(key)[2], places=6)

    def test_preening_starts_and_ends_upright(self):
        for species in ("penguin", "flamingo", "flamingo_juvenile"):
            _, solver, clips = clips_of(species)
            start = solver.solve(clips["preen"].pose(0))
            idle = solver.solve(clips["idle"].pose(0)) if species == "penguin" else None
            if idle is not None:
                self.assertTrue(np.allclose(start["head"][:3, 3], idle["head"][:3, 3], atol=.01))
            self.assertGreater(start["head"][2, 3], .8 * solver.head["head"][2])


class FeedingAndRuminationTests(unittest.TestCase):
    def test_filter_feeding_bill_sweeps_at_the_water_surface(self):
        for species, scale in (("flamingo", 1.), ("flamingo_juvenile", .86)):
            spec, solver, clips = clips_of(species)
            tips = []
            for index in range(60):
                pose = clips["filter_feed"].pose(index / 60)
                world = solver.solve(pose)
                tips.append(quadruped.muzzle(solver, pose))
                for key, leg in spec.rig.legs.items():
                    self.assertAlmostEqual(world[leg.toes][2, 3],
                                           solver.toe_base(key)[2], places=6)
            tips = np.asarray(tips)
            self.assertLess(tips[:, 2].max(), .08)
            self.assertGreater(np.ptp(tips[:, 1]), .20 * scale)

    def test_ruminants_stand_and_grind_the_jaw(self):
        for species in ("camel", "camel_female", "camel_juvenile", "giraffe", "giraffe_female",
                        "giraffe_juvenile"):
            spec, solver, clips = clips_of(species)
            jaw = []
            for index in range(60):
                pose = clips["ruminate"].pose(index / 60)
                world = solver.solve(pose)
                for key, leg in spec.rig.legs.items():
                    self.assertAlmostEqual(world[leg.toes][2, 3], solver.toe_base(key)[2], places=6)
                jaw.append(world[spec.rig.jaw][:3, :3])
            self.assertGreater(max(np.linalg.norm(matrix - jaw[0]) for matrix in jaw), .04, species)


class SocialTests(unittest.TestCase):
    def test_lions_nuzzle_side_to_side_on_four_planted_paws(self):
        for species in ("lion", "lion_female", "lion_juvenile"):
            spec, solver, clips = clips_of(species)
            height = spec.meta["shoulder_height_m"]
            muzzle = []
            for index in range(48):
                pose = clips["nuzzle"].pose(index / 48)
                world = solver.solve(pose)
                muzzle.append(quadruped.muzzle(solver, pose))
                for key, leg in spec.rig.legs.items():
                    self.assertTrue(np.allclose(world[leg.toes][:3, 3], solver.toe_base(key),
                                                atol=1e-6), species + "/" + key)
            muzzle = np.asarray(muzzle)
            self.assertGreater(np.ptp(muzzle[:, 1]), .20 * height, species)
            self.assertGreater(np.ptp(muzzle[:, 2]), .04 * height, species)

    def test_kangaroos_alternate_hands_at_the_muzzle_on_planted_hind_feet(self):
        for species in ("kangaroo", "kangaroo_female", "kangaroo_juvenile"):
            spec, solver, clips = clips_of(species)
            height = spec.meta["shoulder_height_m"]
            tips = {"L": [], "R": []}
            distances = {"L": [], "R": []}
            for index in range(64):
                pose = clips["groom"].pose(index / 64)
                world = solver.solve(pose)
                muzzle = quadruped.muzzle(solver, pose)
                for key in ("L", "R"):
                    toe = world[spec.rig.legs[key].toes][:3, 3]
                    self.assertTrue(np.allclose(toe, solver.toe_base(key), atol=1e-6),
                                    species + "/" + key)
                    hand = "fingers." + key
                    tip = solver.point(world, hand, solver.tail[hand])
                    tips[key].append(tip)
                    distances[key].append(np.linalg.norm(tip - muzzle))
            for key in ("L", "R"):
                self.assertLess(min(distances[key]), .10 * height, species + "/" + key)
                self.assertGreater(np.ptp(np.asarray(tips[key])[:, 2]), .25 * height,
                                   species + "/" + key)


class BatheTests(unittest.TestCase):
    def test_wallowing_species_lie_low_and_roll_their_bodies(self):
        species_names = ("brown_bear", "brown_bear_juvenile", "polar_bear",
                         "polar_bear_juvenile", "rhino", "rhino_juvenile", "tiger")
        for species in species_names:
            spec, solver, clips = clips_of(species)
            hips, chest_heights = [], []
            for index in range(60):
                world = solver.solve(clips["bathe"].pose(index / 60))
                hips.append(world[spec.rig.body][:3, :3])
                chest_heights.append(world[spec.rig.chest][2, 3])
            self.assertGreater(max(np.linalg.norm(matrix - hips[0]) for matrix in hips), .08, species)
            self.assertLess(max(chest_heights), .75 * spec.meta["shoulder_height_m"], species)


class HuddleAndWatchTests(unittest.TestCase):
    def test_penguins_huddle_compactly_without_moving_their_feet(self):
        for species in ("penguin", "penguin_juvenile"):
            spec, solver, clips = clips_of(species)
            bodies = []
            for index in range(60):
                pose = clips["huddle"].pose(index / 60)
                world = solver.solve(pose)
                bodies.append(world[spec.rig.body][:3, :3])
                for key, leg in spec.rig.legs.items():
                    self.assertAlmostEqual(world[leg.toes][2, 3], solver.toe_base(key)[2], places=6)
            self.assertGreater(max(np.linalg.norm(matrix - bodies[0]) for matrix in bodies), .02, species)

    def test_zebras_watch_with_four_planted_hooves_and_a_turning_head(self):
        for species in ("zebra", "zebra_female", "zebra_juvenile"):
            spec, solver, clips = clips_of(species)
            heads = []
            for index in range(60):
                pose = clips["sentry"].pose(index / 60)
                world = solver.solve(pose)
                heads.append(world[spec.rig.head][:3, :3])
                for key, leg in spec.rig.legs.items():
                    self.assertAlmostEqual(world[leg.toes][2, 3], solver.toe_base(key)[2], places=6)
            self.assertGreater(max(np.linalg.norm(matrix - heads[0]) for matrix in heads), .25, species)

    def test_flamingo_huddle_is_compact_on_two_planted_legs(self):
        for species, scale in (("flamingo", 1.), ("flamingo_juvenile", .86)):
            spec, solver, clips = clips_of(species)
            idle_head = solver.solve(clips["idle"].pose(0))[spec.rig.head][2, 3]
            heads = []
            for index in range(60):
                world = solver.solve(clips["huddle"].pose(index / 60))
                heads.append(world[spec.rig.head][:3, 3])
                for key, leg in spec.rig.legs.items():
                    self.assertAlmostEqual(world[leg.toes][2, 3],
                                           solver.toe_base(key)[2], places=6)
            heads = np.asarray(heads)
            self.assertLess(heads[:, 2].max(), idle_head - .08 * scale)
            self.assertGreater(np.ptp(heads[:, 1]), .04 * scale)


class BrowsingTests(unittest.TestCase):
    def test_giraffes_reach_high_and_strip_leaves_side_to_side(self):
        for species in ("giraffe", "giraffe_female", "giraffe_juvenile"):
            spec, solver, clips = clips_of(species)
            muzzles, jaws = [], []
            for index in range(72):
                pose = clips["browse"].pose(index / 72)
                world = solver.solve(pose)
                muzzles.append(quadruped.muzzle(solver, pose))
                jaws.append(world[spec.rig.jaw][:3, :3])
                for key, leg in spec.rig.legs.items():
                    self.assertAlmostEqual(world[leg.toes][2, 3], solver.toe_base(key)[2], places=6)
            muzzles = np.asarray(muzzles)
            self.assertGreater(muzzles[:, 2].min(), .6 * spec.meta["shoulder_height_m"], species)
            self.assertGreater(np.ptp(muzzles[:, 1]), .025 * spec.meta["shoulder_height_m"], species)
            self.assertGreater(max(np.linalg.norm(matrix - jaws[0]) for matrix in jaws), .04, species)


class DustBatheTests(unittest.TestCase):
    def test_ostrich_sits_and_sweeps_the_ground_on_both_sides(self):
        for species in ("ostrich", "ostrich_female", "ostrich_juvenile"):
            spec, solver, clips = clips_of(species)
            rest = solver.solve(clips["rest"].pose(0))
            sides = []
            for index in range(60):
                pose = clips["dust_bathe"].pose(index / 60)
                world = solver.solve(pose)
                self.assertAlmostEqual(world["hips"][2, 3], rest["hips"][2, 3], places=6)
                tip = quadruped.muzzle(solver, pose)
                self.assertGreater(tip[2], .03)
                if tip[2] < .2:
                    sides.append(np.sign(tip[1]))
            self.assertIn(1, sides, species)
            self.assertIn(-1, sides, species)

    def test_elephants_gather_and_throw_over_the_head(self):
        for species in ("elephant", "elephant_female", "elephant_juvenile"):
            spec, solver, clips = clips_of(species)
            height = spec.meta["shoulder_height_m"]
            clip = clips["dust_bathe"]
            lowest, tossed = 1.0, {1: 0, -1: 0}
            for index in range(64):
                world = solver.solve(clip.pose(index / 64))
                tip = solver.point(world, "trunk10", solver.tail["trunk10"])
                lowest = min(lowest, tip[2] / height)
                if tip[2] > world["head"][2, 3]:
                    tossed[int(np.sign(tip[1]))] += 1
                if index % 8 == 0:
                    points = posed_skin(solver, world, spec.sculpt)
                    self.assertGreaterEqual(points[:, 2].min(), -.02, f"{species} at {index}")
            self.assertLess(lowest, .06, species)
            self.assertGreater(tossed[1], 2, species)
            self.assertGreater(tossed[-1], 2, species)


if __name__ == "__main__":
    unittest.main()
