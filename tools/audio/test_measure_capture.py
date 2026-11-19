#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The measuring of a capture, tested on signals whose figures are known.

Run in tools/audio: python3 -m unittest
"""
import math
import unittest

import numpy as np

import measure_capture as measure

RATE = 44100


def tone(seconds: float, amplitude: float, hertz: float = 440.0) -> np.ndarray:
    t = np.arange(round(seconds * RATE)) / RATE
    mono = amplitude * np.sin(2.0 * math.pi * hertz * t)
    return np.stack([mono, mono], axis=1)


class Levels(unittest.TestCase):
    def test_decibels_of_a_power(self):
        self.assertAlmostEqual(measure.db(1.0), 0.0)
        self.assertAlmostEqual(measure.db(0.01), -20.0)
        self.assertEqual(measure.db(0.0), -math.inf)

    def test_window_powers_are_mean_squares_per_channel(self):
        samples = np.concatenate([tone(0.5, 0.5), tone(0.5, 0.1)])
        samples[:, 1] *= 0.5
        powers = measure.window_powers(samples, RATE // 2)
        self.assertEqual(powers.shape, (2, 2))
        self.assertAlmostEqual(powers[0, 0], 0.125, places=3)
        self.assertAlmostEqual(powers[0, 1], 0.125 / 4.0, places=3)
        self.assertAlmostEqual(powers[1, 0], 0.005, places=4)
        self.assertEqual(measure.window_powers(samples[:10], 100).shape, (0, 2))

    def test_first_sound_skips_the_silence_of_loading(self):
        raw = np.zeros((1000, 2), dtype=np.int16)
        self.assertEqual(measure.first_sound(raw), 1000)
        raw[640, 1] = -3
        self.assertEqual(measure.first_sound(raw), 640)

    def test_clipped_counts_both_ends(self):
        raw = np.array([[0, 32767], [-32768, 5], [32766, -32767]], dtype=np.int16)
        self.assertEqual(measure.clipped(raw), 2)


class Gaps(unittest.TestCase):
    def test_a_gap_is_found_with_its_start_and_length(self):
        samples = np.concatenate([tone(1.0, 0.2), np.zeros((RATE // 2, 2)), tone(1.0, 0.2)])
        found = measure.silences(samples, RATE)
        self.assertEqual(len(found), 1)
        self.assertAlmostEqual(found[0][0], 1.0, places=1)
        self.assertAlmostEqual(found[0][1], 0.5, places=1)

    def test_a_short_gap_and_a_quiet_passage_are_not_silence(self):
        samples = np.concatenate([tone(1.0, 0.2), np.zeros((RATE // 5, 2)), tone(1.0, 0.004)])
        self.assertEqual(measure.silences(samples, RATE), [])

    def test_silence_in_one_ear_is_not_silence(self):
        samples = tone(1.0, 0.2)
        samples[:, 0] = 0.0
        self.assertEqual(measure.silences(samples, RATE), [])


class Starts(unittest.TestCase):
    def test_a_start_without_a_fade_is_a_step(self):
        samples = np.concatenate([tone(0.5, 0.001), tone(0.5, 0.3)])
        found = measure.level_steps(samples, RATE)
        self.assertEqual(len(found), 1)
        self.assertAlmostEqual(found[0][0], 0.5, places=2)
        self.assertGreater(found[0][1], 40.0)

    def test_a_fade_of_a_fifth_of_a_second_over_a_bed_is_none(self):
        bed = tone(1.5, 0.01, 150.0)
        loud = tone(1.0, 0.3)
        ramp = np.minimum(1.0, np.arange(len(loud)) / (0.2 * RATE))[:, None]
        bed[RATE // 2:] += loud * ramp
        self.assertEqual(measure.level_steps(bed, RATE), [])

    def test_a_jump_far_below_hearing_is_none(self):
        samples = np.concatenate([np.zeros((RATE // 2, 2)), tone(0.5, 0.001)])
        self.assertEqual(measure.level_steps(samples, RATE), [])


class Sequences(unittest.TestCase):
    def test_repeats_counts_neighbours_only(self):
        self.assertEqual(measure.repeats([]), 0)
        self.assertEqual(measure.repeats(["a", "b", "a", "b"]), 0)
        self.assertEqual(measure.repeats(["a", "a", "a", "b", "b"]), 3)

    def test_overlap_is_the_most_at_one_moment(self):
        self.assertEqual(measure.overlap_count([]), 0)
        self.assertEqual(measure.overlap_count([(0.0, 1.0), (2.0, 3.0)]), 1)
        self.assertEqual(measure.overlap_count([(0.0, 2.0), (1.0, 3.0), (1.5, 1.8), (2.5, 4.0)]), 3)
        # One that ends as the next begins does not overlap it.
        self.assertEqual(measure.overlap_count([(0.0, 1.0), (1.0, 2.0)]), 1)


class Log(unittest.TestCase):
    TEXT = ("11.250\tbeds\tambience/park_day.ogg\n"
            "12.000\tstate\thour=10.50\tvoices=5\twater=0.0100,-0.300,12.0\tfountain=0.1408,0.000,10.0\t"
            "people0=0.1100,0.310,7.0\tpeople1=0.0000,0.000,0.0\tgroups=4,12\n"
            "13.500\tcall\tmeerkat_chirp\tanimals/meerkat_chirp_2.ogg\tvocalize\tanimal=41\tgain=0.692\t"
            "pan=0.214\tvoice=9\n"
            "not a line of the log\n"
            "x\tstate\n")

    def test_lines_become_events(self):
        events = measure.parse_log(self.TEXT)
        self.assertEqual([e["kind"] for e in events], ["beds", "state", "call"])
        self.assertEqual(events[0]["bare"], ["ambience/park_day.ogg"])
        self.assertEqual(events[1]["hour"], 10.5)
        self.assertEqual(events[1]["groups"], "4,12")
        self.assertEqual(events[2]["bare"], ["meerkat_chirp", "animals/meerkat_chirp_2.ogg", "vocalize"])
        self.assertEqual(events[2]["gain"], 0.692)

    def test_the_looping_sources_of_a_state(self):
        state = measure.parse_log(self.TEXT)[1]
        triples = measure.follower_triples(state)
        self.assertEqual(triples["fountain"], (0.1408, 0.0, 10.0))
        self.assertEqual(triples["people0"], (0.11, 0.31, 7.0))
        figures = measure.follower_figures([state])
        self.assertEqual(figures["fountain"], {"heard_share": 1.0, "volume_max": 0.1408, "pan_abs_max": 0.0,
                                               "distance_min": 10.0})
        self.assertEqual(figures["people1"]["heard_share"], 0.0)
        self.assertEqual(figures["water"]["pan_abs_max"], 0.3)

    def test_the_first_logs_wrote_them_bare(self):
        old = measure.parse_log("12.0\tstate\thour=10.5\t0.0580\t-0.933\t12.1\t0.1500\t0.956\t13.7\t0\t0\t0\n")[0]
        triples = measure.follower_triples(old)
        self.assertEqual(sorted(triples), ["people0", "people1", "water"])
        self.assertEqual(triples["water"], (0.058, -0.933, 12.1))
        self.assertEqual(triples["people0"], (0.15, 0.956, 13.7))


class StepEnvelope(unittest.TestCase):
    def test_bursts_at_the_given_times_stand_out_by_their_level(self):
        rng = np.random.default_rng(3)
        samples = rng.normal(0.0, 0.01, (4 * RATE, 2))
        times = [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]
        for t in times:
            at = int(t * RATE)
            samples[at:at + RATE // 20] += rng.normal(0.0, 0.1, (RATE // 20, 2))
        found = measure.step_envelope(samples, times, RATE)
        self.assertEqual(found["epochs"], 6)
        self.assertEqual(len(found["envelope_db"]), 60)
        self.assertAlmostEqual(found["stand_out_db"], 20.0, delta=1.5)
        self.assertLess(found["peak_after_ms"], 50)

    def test_nothing_at_the_given_times_stands_out_by_nothing(self):
        rng = np.random.default_rng(4)
        samples = rng.normal(0.0, 0.01, (4 * RATE, 2))
        found = measure.step_envelope(samples, [0.3 * n for n in range(1, 12)], RATE)
        self.assertLess(found["stand_out_db"], 1.5)

    def test_times_too_near_the_ends_are_left_out(self):
        samples = np.zeros((RATE, 2))
        self.assertIsNone(measure.step_envelope(samples, [0.05, 0.9], RATE))
        self.assertEqual(measure.step_envelope(samples + 0.01, [0.05, 0.4, 0.9], RATE)["epochs"], 1)



class Following(unittest.TestCase):
    def test_a_mark_just_after_a_cause_counts(self):
        self.assertEqual(measure.following([1.12, 2.5, 3.0], [1.0, 2.9]), 2)

    def test_a_mark_ahead_of_every_cause_does_not(self):
        self.assertEqual(measure.following([0.5], [1.0]), 0)

    def test_no_causes_explain_nothing(self):
        self.assertEqual(measure.following([0.5, 0.6], []), 0)


if __name__ == "__main__":
    unittest.main()
