// SPDX-License-Identifier: MIT
// Damaged saves. Whatever happened to a save file, loading it ends in one of two ways: an error
// with a message, or a zoo that lives on (steps, hashes, saves and loads again). Never a crash,
// and never a zoo that was accepted and then breaks. The sanitizer build (build-asan/) is where
// these tests earn their keep: a read past an array in an accepted zoo is silent in Release.
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "TestData.hpp"
#include "zoo/base/Rng.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/sim/ZooSave.hpp"
#include "zoo/sim/ZooSaveJson.hpp"

namespace zoo {
namespace {
using Json = nlohmann::json;

/// How long an accepted zoo has to live: a game minute and a half, enough for every animal and
/// visitor to take decisions and for routes to be walked.
constexpr int kLifeTicks = 240;

std::unique_ptr<Zoo> SavedZoo() {
    ZooConfig config;
    config.name = "Damage Me";
    config.seed = 11;
    auto zoo = std::make_unique<Zoo>(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(*zoo);
    // Past the first morning: visitors are in, animals are in the middle of things.
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(kTicksPerDay) / 2 + 777; ++i) {
        zoo->Step();
    }
    return zoo;
}

struct Outcome {
    int refused = 0;
    int accepted = 0;
};

/// The one rule of this file. Returns true when the document was refused.
bool RefusedOrAlive(const Json& document, const std::string& what, Outcome& outcome) {
    std::string error;
    std::unique_ptr<Zoo> zoo =
        ZooFromJson(document, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    if (zoo == nullptr) {
        EXPECT_FALSE(error.empty()) << what << ": refused without a message";
        ++outcome.refused;
        return true;
    }
    ++outcome.accepted;
    for (int i = 0; i < kLifeTicks; ++i) {
        zoo->Step();
    }
    (void)zoo->StateHash();
    (void)zoo->MeanWelfare();
    // What the game accepted and played on, it must be able to save and to read again.
    const Json again = ZooToJson(*zoo);
    error.clear();
    EXPECT_NE(ZooFromJson(again, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr)
        << what << ": accepted, played, saved, and then refused: " << error;
    return false;
}

/// "/zoo/animals/animals/3/route/0/1" -> "/zoo/animals/animals/#/route/#/#".
std::string Shape(const std::string& pointer) {
    std::string shape;
    std::size_t i = 0;
    while (i < pointer.size()) {
        const std::size_t next = pointer.find('/', i + 1);
        const std::string part = pointer.substr(i, next == std::string::npos ? next : next - i);
        const bool index = part.size() > 1 && part.find_first_not_of("0123456789", 1) == std::string::npos;
        shape += index ? "/#" : part;
        if (next == std::string::npos) {
            break;
        }
        i = next;
    }
    return shape;
}

/// Wrong values for one leaf: other types, and numbers no writer of saves produces.
std::vector<Json> Replacements(const Json& leaf) {
    if (leaf.is_boolean()) {
        return {Json(7), Json("true"), Json(nullptr)};
    }
    if (leaf.is_string()) {
        return {Json(""), Json("nonsense"), Json(3), Json::array()};
    }
    if (leaf.is_number_float()) {
        return {Json(-1.0e9), Json(1.0e9), Json(1.0e30), Json(-0.5), Json("1.0"), Json(nullptr)};
    }
    if (leaf.is_number()) {
        return {Json(-1),     Json(0),   Json(1),   Json(65536),   Json(4294967295U),
                Json(1.0e18), Json(2.5), Json("7"), Json(nullptr), Json::object()};
    }
    // null, empty array, empty object
    return {Json(1), Json("x"), Json(true)};
}

TEST(ZooSaveDamageTests, EveryKindOfValueReplacedByAWrongOne) {
    const std::unique_ptr<Zoo> zoo = SavedZoo();
    Json document = ZooToJson(*zoo);
    ASSERT_GT(zoo->Guests().Count(), 0);
    ASSERT_GT(zoo->AnimalCount(), 0);

    // One representative of every kind of leaf: the first, the middle and the last of its kind.
    std::map<std::string, std::vector<std::string>> kinds;
    const Json flat = document.flatten();
    for (auto item = flat.begin(); item != flat.end(); ++item) {
        kinds[Shape(item.key())].push_back(item.key());
    }
    Outcome outcome;
    int cases = 0;
    for (const auto& [shape, pointers] : kinds) {
        std::vector<std::size_t> picks{0};
        if (pointers.size() > 2) {
            picks.push_back(pointers.size() / 2);
        }
        if (pointers.size() > 1) {
            picks.push_back(pointers.size() - 1);
        }
        // The map's long arrays are read by one loop each: one damaged element says it all.
        if (shape.starts_with("/zoo/world/heights") || shape.starts_with("/zoo/world/tiles") ||
            shape.starts_with("/zoo/world/fences") || shape.starts_with("/zoo/stand_off")) {
            picks.resize(1);
        }
        for (const std::size_t pick : picks) {
            const Json::json_pointer pointer(pointers[pick]);
            if (!document.contains(pointer)) {
                continue;  // flatten() names an empty array or object by itself: nothing below it
            }
            const Json original = document[pointer];
            for (const Json& wrong : Replacements(original)) {
                if (wrong == original) {
                    continue;
                }
                document[pointer] = wrong;
                RefusedOrAlive(document, pointers[pick] + " = " + wrong.dump(), outcome);
                ++cases;
            }
            document[pointer] = original;
        }
    }
    // The document is whole again.
    Outcome whole;
    EXPECT_FALSE(RefusedOrAlive(document, "the undamaged save", whole));
    std::cout << "DAMAGE kinds=" << kinds.size() << " cases=" << cases << " refused=" << outcome.refused
              << " accepted=" << outcome.accepted << '\n';
    EXPECT_GT(outcome.refused, cases / 4);
}

TEST(ZooSaveDamageTests, WholePartsMissingOrOfTheWrongType) {
    const std::unique_ptr<Zoo> zoo = SavedZoo();
    const Json saved = ZooToJson(*zoo);
    Outcome outcome;
    // The parts ZooSaveTests.DamagedSavesFailWithoutCrashing does not reach.
    for (const char* key :
         {"visitors", "stand_off", "notifications", "next_notification", "calendar_shift"}) {
        for (const Json& wrong :
             {Json("nonsense"), Json(-3), Json::array({1, 2, 3}), Json::object(), Json(nullptr)}) {
            Json damaged = saved;
            damaged["zoo"][key] = wrong;
            RefusedOrAlive(damaged, std::string(key) + " = " + wrong.dump(), outcome);
        }
        Json damaged = saved;
        damaged["zoo"].erase(key);
        RefusedOrAlive(damaged, std::string(key) + " erased", outcome);
    }
    // Lists emptied, doubled, and cut in the middle of an entry.
    for (const char* list :
         {"/zoo/animals/animals", "/zoo/animals/care", "/zoo/visitors/visitors", "/zoo/visitors/litter",
          "/zoo/objects/buildings", "/zoo/objects/props", "/zoo/world/heights", "/zoo/world/tiles",
          "/zoo/world/fences_h", "/zoo/notifications", "/zoo/stand_off"}) {
        const Json::json_pointer pointer(list);
        ASSERT_TRUE(saved.contains(pointer)) << list;
        ASSERT_TRUE(saved[pointer].is_array()) << list;
        Json damaged = saved;
        damaged[pointer] = Json::array();
        RefusedOrAlive(damaged, std::string(list) + " emptied", outcome);
        if (saved[pointer].empty()) {
            continue;
        }
        damaged = saved;
        for (const Json& item : saved[pointer]) {
            damaged[pointer].push_back(item);
        }
        RefusedOrAlive(damaged, std::string(list) + " doubled", outcome);
        damaged = saved;
        damaged[pointer].erase(damaged[pointer].size() / 2);
        RefusedOrAlive(damaged, std::string(list) + " one entry short", outcome);
        damaged = saved;
        if (damaged[pointer][0].is_object() && !damaged[pointer][0].empty()) {
            damaged[pointer][0].erase(damaged[pointer][0].begin());
            RefusedOrAlive(damaged, std::string(list) + " first entry one field short", outcome);
        }
    }
    // Objects the rest of the save points at, taken away: the animals keep their `home`,
    // `shelter` and `target_prop`, the visitors their targets.
    for (const char* list : {"/zoo/enclosures/enclosures", "/zoo/objects/buildings", "/zoo/objects/props"}) {
        const Json::json_pointer pointer(list);
        Json damaged = saved;
        if (damaged.contains(pointer) && damaged[pointer].is_array() && !damaged[pointer].empty()) {
            damaged[pointer].erase(0);
            RefusedOrAlive(damaged, std::string(list) + " without its first entry", outcome);
        }
    }
    std::cout << "PARTS refused=" << outcome.refused << " accepted=" << outcome.accepted << '\n';
}

std::string TempFile(const std::string& name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "zoo-simulator-tests";
    std::filesystem::create_directories(dir);
    return (dir / name).string();
}

TEST(ZooSaveDamageTests, TruncatedFilesAreRefused) {
    const std::unique_ptr<Zoo> zoo = SavedZoo();
    const std::string path = TempFile("truncated.zoo");
    std::string error;
    ASSERT_TRUE(SaveZoo(*zoo, path, &error)) << error;
    std::string text;
    {
        std::ifstream in(path, std::ios::binary);
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    ASSERT_GT(text.size(), 1000U);
    // A file cut anywhere is not a document any more: an empty file, the first bytes, the middle of
    // the map, one byte short of the end.
    std::vector<std::size_t> cuts{0, 1, 2, 17, 60, text.size() - 1, text.size() - 2};
    for (int part = 1; part < 24; ++part) {
        cuts.push_back(text.size() * static_cast<std::size_t>(part) / 24);
    }
    for (const std::size_t cut : cuts) {
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out.write(text.data(), static_cast<std::streamsize>(cut));
        }
        error.clear();
        EXPECT_EQ(LoadZoo(path, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr)
            << "cut at " << cut << " of " << text.size();
        EXPECT_FALSE(error.empty()) << "cut at " << cut;
        SaveSummary summary;
        error.clear();
        EXPECT_FALSE(ReadSaveSummary(path, summary, &error)) << "cut at " << cut;
        EXPECT_FALSE(error.empty()) << "cut at " << cut;
    }
    // A directory in place of the file, and a file that cannot be there.
    error.clear();
    EXPECT_EQ(LoadZoo(std::filesystem::path(path).parent_path().string(), testdata::GameCatalog(),
                      testdata::GameSpecies(), &error),
              nullptr);
    EXPECT_FALSE(error.empty());
    SaveSummary none;
    error.clear();
    EXPECT_FALSE(ReadSaveSummary(std::filesystem::path(path).parent_path().string(), none, &error));
    EXPECT_FALSE(error.empty());
    error.clear();
    EXPECT_FALSE(SaveZoo(*zoo, path + "/below-a-file/save.zoo", &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(std::filesystem::exists(path + "/below-a-file/save.zoo.tmp"));
    std::filesystem::remove(path);
}

TEST(ZooSaveDamageTests, FlippedBytesAreRefusedOrHarmless) {
    const std::unique_ptr<Zoo> zoo = SavedZoo();
    const std::string text = ZooToJson(*zoo).dump();
    Rng rng(20261113);
    Outcome outcome;
    int notJson = 0;
    // The map's arrays make most of the file: besides flips anywhere in it, as many again go
    // into the first 6,000 characters of each other part.
    std::vector<std::pair<std::size_t, std::size_t>> ranges{{0, text.size()}};
    for (const char* key : {"\"animals\":{", "\"visitors\":{", "\"objects\":{", "\"config\":{",
                            "\"finance\":{", "\"enclosures\":", "\"weather\":{"}) {
        const std::size_t at = text.find(key);
        ASSERT_NE(at, std::string::npos) << key;
        ranges.emplace_back(at, std::min(text.size(), at + 6000));
    }
    constexpr int kFlipsPerRange = 40;
    for (const auto& [from, to] : ranges) {
        for (int flip = 0; flip < kFlipsPerRange; ++flip) {
            std::string damaged = text;
            const auto at = from + static_cast<std::size_t>(rng.NextU32()) % (to - from);
            const std::uint32_t how = rng.NextU32();
            if (how % 3 == 0) {
                damaged[at] = static_cast<char>(damaged[at] ^ (1 << (how >> 8) % 7));
            } else if (how % 3 == 1) {
                damaged[at] = "0123456789-.e\"{}[],:tfn "[(how >> 8) % 25];
            } else {
                damaged.erase(at, 1 + (how >> 8) % 3);
            }
            const Json parsed = Json::parse(damaged, nullptr, false);
            if (parsed.is_discarded()) {
                ++notJson;
                continue;
            }
            RefusedOrAlive(parsed, "byte " + std::to_string(at) + " of the save text", outcome);
        }
    }
    std::cout << "FLIPS not-json=" << notJson << " refused=" << outcome.refused
              << " accepted=" << outcome.accepted << '\n';
}

// A save of format version 1 has no stand-off layer; this program still opens it (ZooFromJson),
// as the zoo it was: no reserved ground.
TEST(ZooSaveDamageTests, AVersionOneSaveWithoutStandOffOpens) {
    const std::unique_ptr<Zoo> zoo = SavedZoo();
    Json saved = ZooToJson(*zoo);
    ASSERT_TRUE(saved["zoo"]["stand_off"].empty()) << "the starter zoo reserves no ground";
    saved["version"] = 1;
    saved["zoo"].erase("stand_off");
    std::string error;
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_TRUE(zoo->SameState(*loaded));
    EXPECT_EQ(zoo->StateHash(), loaded->StateHash());
    // And version 2 without the layer is damaged, not old.
    saved["version"] = 2;
    EXPECT_EQ(ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr);
    EXPECT_NE(error.find("stand_off"), std::string::npos) << error;
}

}  // namespace
}  // namespace zoo
