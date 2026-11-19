# SPDX-License-Identifier: MIT
"""Analytic checks of audit metrics; these arrays never become audio assets."""
import math
import unittest

import numpy as np

from audit_audio import (MIX_OFFSET_LU, band_shares, call_gains, edge_silence, loop_delta, quiet_floor,
                         step_gains, target, window_powers)


class AuditTests(unittest.TestCase):
    def test_low_and_high_frequency_energy(self):
        rate = 44100
        times = np.arange(rate) / rate
        samples = (2 * np.sin(2 * np.pi * 100 * times)
                   + np.sin(2 * np.pi * 7000 * times)).reshape(-1, 1)
        low, high = band_shares(samples, rate)
        self.assertAlmostEqual(low, 80, places=8)
        self.assertAlmostEqual(high, 20, places=8)

    def test_stereo_does_not_cancel(self):
        samples = np.sin(2 * np.pi * 100 * np.arange(44100) / 44100)
        stereo = np.column_stack((samples, -samples))
        low, high = band_shares(stereo, 44100)
        self.assertAlmostEqual(low, 100, places=8)
        self.assertAlmostEqual(high, 0, places=8)

    def test_dc_and_nyquist_weights(self):
        samples = (1 + (-1.0) ** np.arange(1000)).reshape(-1, 1)
        low, high = band_shares(samples, 20000)
        self.assertAlmostEqual(low, 50)
        self.assertAlmostEqual(high, 50)

    def test_quiet_tenth_uses_energy_mean(self):
        samples = np.ones((1000, 1))
        samples[:50] = 0.01
        samples[50:100] = 0.02
        self.assertAlmostEqual(quiet_floor(samples, 1000), 10 * math.log10(0.00025))

    def test_partial_window_is_not_zero_padded(self):
        self.assertEqual(list(window_powers(np.ones((101, 1)), 50)), [1, 1, 1])

    def test_digital_silence_is_negative_infinity(self):
        self.assertEqual(quiet_floor(np.zeros((1000, 1)), 1000), -math.inf)

    def test_silence_edges(self):
        samples = np.ones((1000, 1))
        samples[:100] = 0
        samples[-150:] = 0
        self.assertEqual(edge_silence(samples, 1000), (100, 150))

    def test_zero_crossings_do_not_become_edge_silence(self):
        samples = np.sin(2 * np.pi * 1000 * np.arange(4410) / 44100).reshape(-1, 1)
        self.assertEqual(edge_silence(samples, 44100), (0, 0))

    def test_constant_loop_has_no_seam_delta(self):
        self.assertEqual(loop_delta(np.full((44100, 2), 0.2), 44100), 0)

    def test_call_gains_only_attenuate(self):
        loud, _ = target('lion_roar')
        rows = [{'kind': 'call', 'name': 'lion_roar', 'file': 'a', 'lufs': loud + MIX_OFFSET_LU + 4.0},
                {'kind': 'call', 'name': 'lion_roar', 'file': 'b', 'lufs': loud + MIX_OFFSET_LU - 6.0},
                {'kind': 'ambience', 'name': 'water', 'file': 'c', 'lufs': -23.0}]
        self.assertEqual(call_gains(rows), {'a': -4.0, 'b': 0.0})

    def test_steps_of_a_surface_are_levelled_to_the_quietest(self):
        rows = [{'kind': 'footstep', 'name': 'footstep_grass', 'file': 'g1', 'lufs': -27.6},
                {'kind': 'footstep', 'name': 'footstep_grass', 'file': 'g2', 'lufs': -20.6},
                {'kind': 'footstep', 'name': 'footstep_gravel', 'file': 'k1', 'lufs': -23.1},
                {'kind': 'footstep', 'name': 'footstep_gravel', 'file': 'k2', 'lufs': -24.8},
                {'kind': 'call', 'name': 'lion_roar', 'file': 'r', 'lufs': -11.7}]
        self.assertEqual(step_gains(rows), {'g1': 0.0, 'g2': -7.0, 'k1': -1.7, 'k2': 0.0})


if __name__ == '__main__':
    unittest.main()
