#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The mix of src/game/Soundscape.cpp as figures: what the game asks the audio engine for,
computed without a sound device, so that it can be tested and tabulated.

The constants are the ones of `namespace mix` in Soundscape.cpp; test_mix_model.py reads that
file and fails when one of them differs. Run from the repository root for the tables that
docs/AUDIO.md quotes:

    python3 tools/audio/mix_model.py

Nothing here is a judgement of how anything sounds: nobody has listened to this game. The
reasons given are distances and levels, which can be measured.
"""
import math

# A source abeam is heard this far to one side at most (1 = one ear only). CNA's pan law leaves
# the far ear 1 - |pan| of the level: 0.6 is 8 dB between the ears, the far ear never silent.
PAN_LIMIT = 0.6

# How a call carries by its kind: (full level within, metres; reach, metres). The habitats are
# 30 to 60 m across and the zoo some 600 m: a quiet sound (growl, grunt, chirp, huff, snort,
# hiss) is heard across its own habitat and at the next path, a mid one (bray, whinny, bark,
# call, rumble) two habitats on, a loud one (roar, howl, trumpet) over a third of the zoo.
CARRY = {"quiet": (8.0, 60.0), "mid": (10.0, 110.0), "loud": (14.0, 220.0)}
LOUD_ENDINGS = ("_roar", "_howl", "_trumpet")
MID_ENDINGS = ("_bray", "_whinny", "_bark", "_call", "_rumble")
CALL_ROLLOFF = 1.0  # half the pressure with each doubling of the distance: a source in the open
EDGE = 0.2          # the engine fades a source to nothing over the last fifth of its reach

CALL_PITCH = 0.05        # octaves each way: 3.5 %
CALL_LEVEL_FLOOR = 0.92  # the lowest level of a call, of 1: 0.7 dB
STEP_PITCH = 0.05
STEP_LEVEL_FLOOR = 0.88  # 1.1 dB
STEPS_CATEGORY = 1.0
STEP_WALK = 0.7          # a walker's step against a runner's: 3.1 dB lower
# The visitor's body (src/game/Walker.{hpp,cpp}): one footfall and one sway of the eyes each
# stride, at the walk and at the run alike. test_mix_model.py reads these from Walker.
STRIDE = 1.5             # metres
WALK_SPEED = 4.4         # m/s
RUN_SPEED = 8.8          # m/s

BED_FADE = 1.2      # per second: 0.58 s to half, 1.9 s to nine tenths
LEAVES_CALM = 0.25
BIRDSONG = 0.85    # the day bed at full daylight, after its rumble below 120 Hz was taken out
CROWD_FULL = 30.0   # people in a square of ten metres at whom the murmur plays as recorded
CROWD_FEWEST = 3.0
SOFT_CHANCE = 0.5
SOFT_QUIET = 25.0
SOFT_GAP = 4.0
FOUNTAIN_REFERENCE = 4.0
FOUNTAIN_REACH = 45.0

WATER = {"reference": 2.5, "radius": 26.0, "rolloff": 1.0}
FOUNTAIN = {"reference": FOUNTAIN_REFERENCE, "radius": FOUNTAIN_REACH, "rolloff": 1.0}
CROWD = {"reference": 5.0, "radius": 55.0, "rolloff": 0.9}
CATEGORY = {"ambience": 0.55, "people": 0.6, "animals": 1.0, "footsteps": STEPS_CATEGORY}

# The names of the constants in Soundscape.cpp and their values here.
CPP = {
    "kPanLimit": PAN_LIMIT,
    "kQuietReference": CARRY["quiet"][0], "kQuietReach": CARRY["quiet"][1],
    "kMidReference": CARRY["mid"][0], "kMidReach": CARRY["mid"][1],
    "kLoudReference": CARRY["loud"][0], "kLoudReach": CARRY["loud"][1],
    "kCallRolloff": CALL_ROLLOFF, "kCallPitch": CALL_PITCH, "kCallLevelFloor": CALL_LEVEL_FLOOR,
    "kStepPitch": STEP_PITCH, "kStepLevelFloor": STEP_LEVEL_FLOOR, "kStepsCategory": STEPS_CATEGORY,
    "kStepWalk": STEP_WALK, "kBedFade": BED_FADE, "kLeavesCalm": LEAVES_CALM,
    "kBirdsong": BIRDSONG,
    "kCrowdFull": CROWD_FULL, "kCrowdFewest": CROWD_FEWEST, "kSoftChance": SOFT_CHANCE,
    "kSoftQuiet": SOFT_QUIET, "kSoftGap": SOFT_GAP, "kFountainReference": FOUNTAIN_REFERENCE,
    "kFountainReach": FOUNTAIN_REACH,
}

# The mix before brief G7 (commit ed7a3b12's parent state of Soundscape.cpp), for comparison:
# reach by the call's ending, full level within 7 % of it, rolloff 0.85.
OLD_REACH = {"_roar": 170.0, "_howl": 190.0, "_trumpet": 170.0, "_bray": 90.0, "_whinny": 90.0,
             "_bark": 80.0, "_call": 70.0, "_rumble": 70.0}
OLD_QUIET_REACH = 45.0
OLD_REFERENCE_SHARE = 0.07
OLD_ROLLOFF = 0.85


def decibels(amplitude: float) -> float:
    return 20.0 * math.log10(amplitude) if amplitude > 0.0 else -math.inf


def spatial_volume(distance: float, reference: float, radius: float, rolloff: float, edge: float = EDGE) -> float:
    """cna-engine's CnaEngine::Audio::Spatialize, the volume part: nothing at the radius and
    beyond, the whole within the reference distance, (reference / distance) ^ rolloff between,
    and a linear fade over the last `edge` share of the radius."""
    if not distance < radius:
        return 0.0
    volume = 1.0 if distance <= reference else (reference / distance) ** rolloff
    edge_start = radius * (1.0 - min(1.0, max(0.0, edge)))
    if distance > edge_start and radius > edge_start:
        volume *= (radius - distance) / (radius - edge_start)
    return max(0.0, min(1.0, volume))


def pan_gains(pan: float) -> tuple[float, float]:
    """(left, right) amplitude of a mono source at pan -1..1 as CNA mixes it: the ear the
    sound leans to keeps it whole, the other gets 1 - |pan|."""
    pan = max(-1.0, min(1.0, pan))
    return (1.0, 1.0 + pan) if pan <= 0.0 else (1.0 - pan, 1.0)


def carry_class(name: str) -> str:
    if name.endswith(LOUD_ENDINGS):
        return "loud"
    if name.endswith(MID_ENDINGS):
        return "mid"
    return "quiet"


def carry_of(name: str) -> tuple[float, float]:
    """(reference, reach) of a call by its name, as Soundscape.cpp's CarryOf."""
    return CARRY[carry_class(name)]


def call_volume(name: str, distance: float) -> float:
    reference, reach = carry_of(name)
    return spatial_volume(distance, reference, reach, CALL_ROLLOFF)


def old_call_volume(name: str, distance: float) -> float:
    reach = next((metres for ending, metres in OLD_REACH.items() if name.endswith(ending)), OLD_QUIET_REACH)
    return spatial_volume(distance, reach * OLD_REFERENCE_SHARE, reach, OLD_ROLLOFF)


def apparent_pan(side: float, distance: float, near: float = 0.0) -> float:
    """The pan the engine is given for a source whose direction has the component `side`
    (-1..1) along the listener's right, `distance` metres away: Soundscape.cpp's Apparent.
    Within `near` metres a source is around the ears rather than beside them."""
    side = max(-1.0, min(1.0, side))
    closeness = min(1.0, distance / near) if near > 0.0 else 1.0
    return side * PAN_LIMIT * closeness


def crowd_level(people: float) -> float:
    """The murmur's level for so many people in a square of ten metres: twice the people are
    twice the power (3 dB), not twice the pressure; fewer than CROWD_FEWEST make none."""
    if people < CROWD_FEWEST:
        return 0.0
    return min(1.0, math.sqrt(people / CROWD_FULL))


def bed_level(wanted: float, seconds: float, start: float = 0.0, frame: float = 1.0 / 60.0) -> float:
    """The level of the air or the leaves `seconds` after `wanted` changed, frame by frame as
    Soundscape::Lay eases it (never more than 0.05 s in one step)."""
    level = start
    for _ in range(round(seconds / frame)):
        level += (wanted - level) * (1.0 - math.exp(-BED_FADE * min(frame, 0.05)))
    return level


def air_levels(daylight: float, wind: float) -> tuple[float, float]:
    """(birdsong, leaves) wanted for the sky's daylight and wind, both 0..1."""
    return BIRDSONG * max(0.0, min(1.0, daylight)), max(0.0, min(1.0, LEAVES_CALM + wind))


def steps_per_second(speed: float, stride: float = STRIDE) -> float:
    """Footsteps a second: Soundscape::Step plays one each stride the body reports."""
    return speed / stride


def pick(last: int, count: int, chance: float) -> int:
    """Soundscape::Pick: any take but the last one played, each as likely as the other."""
    if count <= 1:
        return 0
    choices = count if last < 0 else count - 1
    index = min(int(chance * choices), choices - 1)
    if last >= 0 and index >= last:
        index += 1
    return index


DISTANCES = (5, 10, 20, 30, 45, 60, 80, 110, 150, 200)


def distance_table() -> str:
    def cell(volume: float) -> str:
        return "-" if volume <= 0.0 else f"{decibels(volume):.1f}"
    lines = ["| Source | " + " | ".join(f"{d} m" for d in DISTANCES) + " |",
             "|---|" + "---:|" * len(DISTANCES)]
    rows = [("quiet call (growl, grunt, chirp), before", lambda d: old_call_volume("x_growl", d)),
            ("quiet call, now", lambda d: call_volume("x_growl", d)),
            ("mid call (bray), before", lambda d: old_call_volume("x_bray", d)),
            ("mid call (rumble, call), before", lambda d: old_call_volume("x_call", d)),
            ("mid call, now", lambda d: call_volume("x_bray", d)),
            ("loud call (roar), before", lambda d: old_call_volume("x_roar", d)),
            ("loud call, now", lambda d: call_volume("x_roar", d)),
            ("pond or pool water", lambda d: spatial_volume(d, **WATER)),
            ("fountain", lambda d: spatial_volume(d, **FOUNTAIN)),
            ("crowd of 30 in a square", lambda d: spatial_volume(d, **CROWD))]
    for label, volume in rows:
        lines.append(f"| {label} | " + " | ".join(cell(volume(d)) for d in DISTANCES) + " |")
    return "\n".join(lines) + "\n"


def crowd_table() -> str:
    counts = (2, 3, 6, 12, 17, 30, 60)
    return ("| People in the square | " + " | ".join(str(c) for c in counts) + " |\n|---|" + "---:|" * len(counts) +
            "\n| Level, dB of the recording | " +
            " | ".join("-" if crowd_level(c) <= 0.0 else f"{decibels(crowd_level(c)):.1f}" for c in counts) + " |\n")


if __name__ == "__main__":
    print("Level against distance, dB of the take's own level (- = not heard):\n")
    print(distance_table())
    print("The murmur against the number of people:\n")
    print(crowd_table())
