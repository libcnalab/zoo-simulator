#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The mix logic of src/game/Soundscape.cpp, tested without a sound device.

mix_model.py holds the figures and the arithmetic; the first tests here read Soundscape.cpp
and fail when the game and the model disagree, the rest state what the mix must do whatever
the figures are. Run in tools/audio: python3 -m unittest
"""
import math
import pathlib
import re
import unittest

import mix_model as mix

SOURCE = pathlib.Path(__file__).resolve().parents[2] / "src" / "game" / "Soundscape.cpp"


def cpp_constants(text: str) -> dict[str, float]:
    return {name: float(value) for name, value in
            re.findall(r"constexpr (?:float|int) (k\w+) = (-?[\d.]+)f?;", text)}


def cpp_endings(text: str, array: str) -> tuple[str, ...]:
    body = re.search(array + r" = \{([^}]*)\}", text)
    return tuple(re.findall(r'"(_\w+)"', body.group(1))) if body else ()


def cpp_falloff(text: str, name: str) -> dict[str, str]:
    body = re.search(name + r"\{(.*?)\};", text, re.S)
    return dict(re.findall(r"\.(\w+) = ([\w:.]+?)f?[,}\s]", body.group(1) + " ")) if body else {}


class GameAndModelAgree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = SOURCE.read_text()
        cls.constants = cpp_constants(cls.text)

    def test_every_constant_of_the_mix_is_the_models(self):
        block = self.text[self.text.index("namespace mix {"):self.text.index("}  // namespace mix")]
        in_game = cpp_constants(block)
        self.assertEqual(sorted(in_game), sorted(mix.CPP))
        for name, value in mix.CPP.items():
            self.assertAlmostEqual(in_game[name], value, places=6, msg=name)

    def test_the_calls_that_carry_are_the_same(self):
        self.assertEqual(cpp_endings(self.text, "kLoud"), mix.LOUD_ENDINGS)
        self.assertEqual(cpp_endings(self.text, "kMid"), mix.MID_ENDINGS)

    def test_water_and_crowd_fall_off_the_same(self):
        for name, model in (("kWaterFalloff", mix.WATER), ("kCrowdFalloff", mix.CROWD)):
            in_game = cpp_falloff(self.text, name)
            self.assertEqual({key: float(value) for key, value in in_game.items()}, model, name)
        fountain = cpp_falloff(self.text, "kFountainFalloff")
        self.assertEqual(fountain["reference"], "mix::kFountainReference")
        self.assertEqual(fountain["radius"], "mix::kFountainReach")
        self.assertEqual(float(fountain["rolloff"]), mix.FOUNTAIN["rolloff"])

    def test_the_categories_are_the_same(self):
        in_game = dict(re.findall(r'SetCategoryVolume\("(\w+)", ([\w:.]+?)f?\)', self.text))
        self.assertEqual(in_game.pop("footsteps"), "mix::kStepsCategory")
        self.assertEqual({key: float(value) for key, value in in_game.items()},
                         {key: value for key, value in mix.CATEGORY.items() if key != "footsteps"})

    def test_nothing_sounds_by_the_clock(self):
        # The call timer of the first soundscape (untilCall_) played a call every 7 to 18 s
        # whatever the animals did.
        self.assertNotIn("untilCall_", self.text)
        self.assertIn("AnimalActivity::Vocalize", self.text)


class Distance(unittest.TestCase):
    def test_a_call_never_grows_with_distance_and_never_exceeds_the_take(self):
        for name in ("lion_roar", "penguin_bray", "meerkat_chirp"):
            before = 1.0
            for tenth in range(1, 2500):
                volume = mix.call_volume(name, tenth / 10.0)
                self.assertLessEqual(volume, before + 1e-9, (name, tenth))
                before = volume

    def test_a_call_ends_without_a_step(self):
        # Walking at 1.6 m/s one moves 2.7 cm a frame: at the reach the level is nothing, and
        # a metre inside it still under a hundredth of the take (-40 dB).
        for kind, (_, reach) in mix.CARRY.items():
            self.assertEqual(mix.spatial_volume(reach, *mix.CARRY[kind], mix.CALL_ROLLOFF), 0.0)
            self.assertLess(mix.spatial_volume(reach - 1.0, *mix.CARRY[kind], mix.CALL_ROLLOFF), 0.012, kind)
            self.assertGreater(mix.spatial_volume(reach - 1.0, *mix.CARRY[kind], mix.CALL_ROLLOFF), 0.0)

    def test_half_the_pressure_with_each_doubling(self):
        self.assertAlmostEqual(mix.decibels(mix.call_volume("lion_roar", 28.0)) -
                               mix.decibels(mix.call_volume("lion_roar", 56.0)), 6.02, places=1)

    def test_a_roar_carries_and_a_chirp_does_not(self):
        self.assertGreater(mix.call_volume("lion_roar", 150.0), 0.0)
        self.assertGreater(mix.call_volume("wolf_howl", 150.0), 0.0)
        self.assertEqual(mix.call_volume("meerkat_chirp", 150.0), 0.0)
        self.assertEqual(mix.call_volume("brown_bear_growl", 61.0), 0.0)
        for metres in (20.0, 40.0, 59.0):
            self.assertGreater(mix.call_volume("lion_roar", metres), mix.call_volume("penguin_bray", metres))
            self.assertGreater(mix.call_volume("penguin_bray", metres), mix.call_volume("meerkat_chirp", metres))

    def test_a_quiet_call_is_heard_across_its_own_habitat(self):
        # The habitats are 30 to 60 m across: from the fence to the middle is 15 to 30 m. Before,
        # a growl was 14 to 17 dB down there, and nothing at 45 m.
        for metres in (15.0, 30.0):
            now = mix.decibels(mix.call_volume("brown_bear_growl", metres))
            before = mix.decibels(mix.old_call_volume("brown_bear_growl", metres))
            self.assertGreater(now, -12.0)
            self.assertGreater(now - before, 4.5)
        self.assertGreater(mix.call_volume("brown_bear_growl", 45.0), 0.0)
        self.assertEqual(mix.old_call_volume("brown_bear_growl", 45.0), 0.0)

    def test_names_fall_into_their_classes(self):
        self.assertEqual(mix.carry_class("elephant_trumpet"), "loud")
        self.assertEqual(mix.carry_class("elephant_rumble"), "mid")
        self.assertEqual(mix.carry_class("flamingo_call"), "mid")
        self.assertEqual(mix.carry_class("zebra_bark"), "mid")
        self.assertEqual(mix.carry_class("tiger_chuff"), "quiet")
        self.assertEqual(mix.carry_class("growl"), "quiet")

    def test_the_fountain_is_heard_before_the_pond(self):
        self.assertGreater(mix.spatial_volume(30.0, **mix.FOUNTAIN), 0.0)
        self.assertEqual(mix.spatial_volume(30.0, **mix.WATER), 0.0)
        self.assertEqual(mix.spatial_volume(45.0, **mix.FOUNTAIN), 0.0)


class Sides(unittest.TestCase):
    def test_no_source_is_in_one_ear_only(self):
        for side in (-1.0, -0.5, 0.0, 0.3, 1.0):
            for distance in (0.5, 3.0, 10.0, 200.0):
                for near in (0.0, 6.0, 10.0):
                    pan = mix.apparent_pan(side, distance, near)
                    self.assertLessEqual(abs(pan), mix.PAN_LIMIT + 1e-9)
                    left, right = mix.pan_gains(pan)
                    self.assertGreaterEqual(min(left, right), 1.0 - mix.PAN_LIMIT - 1e-9)

    def test_the_side_is_kept(self):
        self.assertLess(mix.apparent_pan(-0.7, 20.0), 0.0)
        self.assertGreater(mix.apparent_pan(0.7, 20.0), 0.0)
        self.assertEqual(mix.apparent_pan(0.0, 20.0), 0.0)
        self.assertGreater(mix.apparent_pan(1.0, 20.0), mix.apparent_pan(0.5, 20.0))

    def test_a_source_one_stands_in_is_around_the_ears(self):
        self.assertAlmostEqual(mix.apparent_pan(1.0, 0.0, 6.0), 0.0)
        self.assertAlmostEqual(mix.apparent_pan(1.0, 3.0, 6.0), mix.PAN_LIMIT / 2.0)
        self.assertAlmostEqual(mix.apparent_pan(1.0, 6.0, 6.0), mix.PAN_LIMIT)
        self.assertAlmostEqual(mix.apparent_pan(1.0, 60.0, 6.0), mix.PAN_LIMIT)

    def test_the_pan_law_keeps_the_near_ear_whole(self):
        self.assertEqual(mix.pan_gains(0.0), (1.0, 1.0))
        self.assertEqual(mix.pan_gains(-1.0), (1.0, 0.0))
        self.assertEqual(mix.pan_gains(1.0), (0.0, 1.0))
        left, right = mix.pan_gains(mix.PAN_LIMIT)
        self.assertAlmostEqual(mix.decibels(right) - mix.decibels(left), 7.96, places=1)


class Crowd(unittest.TestCase):
    def test_twice_the_people_are_three_decibels(self):
        self.assertAlmostEqual(mix.decibels(mix.crowd_level(12)) - mix.decibels(mix.crowd_level(6)), 3.01, places=2)

    def test_a_couple_is_no_crowd_and_a_full_square_is_the_recording(self):
        self.assertEqual(mix.crowd_level(2), 0.0)
        self.assertGreater(mix.crowd_level(3), 0.0)
        self.assertEqual(mix.crowd_level(mix.CROWD_FULL), 1.0)
        self.assertEqual(mix.crowd_level(800), 1.0)

    def test_more_people_are_never_quieter(self):
        levels = [mix.crowd_level(count) for count in range(0, 80)]
        self.assertEqual(levels, sorted(levels))


class Beds(unittest.TestCase):
    def test_a_bed_starts_from_nothing(self):
        self.assertLess(mix.bed_level(1.0, 1.0 / 60.0), 0.03)
        self.assertLess(mix.bed_level(1.0, 0.1), 0.12)
        self.assertGreater(mix.bed_level(1.0, 3.0), 0.95)

    def test_a_long_frame_is_no_step(self):
        # A frame of half a second (loading) moves a bed no farther than one of 0.05 s.
        self.assertLess(mix.bed_level(1.0, 0.5, frame=0.5), 0.06)

    def test_birds_by_day_and_none_at_night(self):
        self.assertEqual(mix.air_levels(1.0, 0.15)[0], mix.BIRDSONG)
        self.assertEqual(mix.air_levels(0.0, 0.15)[0], 0.0)
        self.assertAlmostEqual(mix.air_levels(0.0, 0.15)[1], 0.4)
        self.assertEqual(mix.air_levels(0.3, 2.0)[1], 1.0)


class Takes(unittest.TestCase):
    def test_no_take_twice_in_a_row(self):
        for count in (2, 3, 5):
            for last in range(count):
                picks = {mix.pick(last, count, step / 1000.0) for step in range(1000)}
                self.assertEqual(picks, set(range(count)) - {last})

    def test_the_first_take_is_any_take_and_a_single_take_is_itself(self):
        self.assertEqual({mix.pick(-1, 3, step / 100.0) for step in range(100)}, {0, 1, 2})
        self.assertEqual(mix.pick(0, 1, 0.7), 0)
        self.assertEqual(mix.pick(-1, 3, 0.999999), 2)

    def test_the_others_are_equally_likely(self):
        picks = [mix.pick(1, 5, (step + 0.5) / 4000.0) for step in range(4000)]
        self.assertEqual([picks.count(index) for index in range(5)], [1000, 0, 1000, 1000, 1000])

    def test_variation_stays_within_a_few_percent(self):
        self.assertLessEqual(2.0 ** mix.CALL_PITCH - 1.0, 0.04)
        self.assertLessEqual(2.0 ** mix.STEP_PITCH - 1.0, 0.04)
        self.assertGreaterEqual(mix.decibels(mix.CALL_LEVEL_FLOOR), -1.0)
        self.assertGreaterEqual(mix.decibels(mix.STEP_LEVEL_FLOOR), -1.5)


WALKER = pathlib.Path(__file__).resolve().parents[2] / "src" / "game"
SOURCES = pathlib.Path(__file__).resolve().parent / "sources.json"

# The activities of HeardDoing (Soundscape.cpp) and the words in a recording's description
# ("heard" in sources.json) that say it was made during that activity.
DOING_WORDS = {"FilterFeed": ("forag", "feed"), "Forage": ("forag", "feed")}


class CallsTiedToWhatWasRecorded(unittest.TestCase):
    def test_a_doing_is_heard_only_where_the_recording_is_of_it(self):
        import json
        text = SOURCE.read_text()
        table = re.search(r"kHeardDoing = \{\{(.*?)\}\};", text, re.S)
        self.assertIsNotNone(table)
        pairs = re.findall(r'\{"(\w+)", zoo::AnimalActivity::(\w+)\}', table.group(1))
        self.assertTrue(pairs)
        entries = json.loads(SOURCES.read_text())
        entries = entries.get("files", entries) if isinstance(entries, dict) else entries
        for species, activity in pairs:
            heard = [e["heard"].lower() for e in entries if e.get("species") == species and e["kind"] == "call"]
            self.assertTrue(heard, species)
            for description in heard:
                self.assertTrue(any(word in description for word in DOING_WORDS[activity]),
                                f"{species}: {description}")


class Steps(unittest.TestCase):
    def test_the_body_is_the_walkers(self):
        header = (WALKER / "Walker.hpp").read_text()
        body = (WALKER / "Walker.cpp").read_text()
        speeds = dict(re.findall(r"constexpr float (k\w+Speed) = ([\d.]+)f;", header))
        self.assertAlmostEqual(float(speeds["kWalkSpeed"]), mix.WALK_SPEED)
        self.assertAlmostEqual(float(speeds["kRunSpeed"]), mix.RUN_SPEED)
        stride = re.search(r"settings\.strideLength = ([\d.]+)f;", body)
        self.assertAlmostEqual(float(stride.group(1)), mix.STRIDE)

    def test_every_stride_is_heard(self):
        # Soundscape::Step plays on every stride the body reports, walking or running: the
        # steps keep time with the sway of the eyes, which has one dip a stride.
        text = SOURCE.read_text()
        step = text[text.index("void Soundscape::Step("):text.index("void Soundscape::Update(")]
        # Before the take is picked the only way out is a foot off the map.
        self.assertEqual(step[:step.index("Pick(")].count("return;"), 1)
        self.assertAlmostEqual(mix.steps_per_second(mix.WALK_SPEED), 2.93, places=2)
        self.assertAlmostEqual(mix.steps_per_second(mix.RUN_SPEED), 5.87, places=2)

    def test_a_walkers_step_is_the_lighter(self):
        self.assertLess(mix.STEP_WALK, 1.0)
        self.assertGreater(mix.decibels(mix.STEP_WALK), -6.0)


if __name__ == "__main__":
    unittest.main()
