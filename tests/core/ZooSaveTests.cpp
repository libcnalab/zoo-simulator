// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "TestData.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/sim/ZooSave.hpp"
#include "zoo/sim/ZooSaveJson.hpp"

namespace zoo {

namespace {

void Advance(Zoo& zoo, std::int64_t ticks) {
    for (std::int64_t i = 0; i < ticks; ++i) {
        zoo.Step();
    }
}

std::string TempFile(const char* name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "zoo-simulator-tests";
    std::filesystem::create_directories(dir);
    return (dir / name).string();
}

std::unique_ptr<Zoo> LivedInZoo() {
    ZooConfig config;
    config.name = "Save Me";
    config.seed = 5;
    auto zoo = std::make_unique<Zoo>(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(*zoo);
    Advance(*zoo, static_cast<std::int64_t>(kTicksPerDay) * 2 + 1234);
    return zoo;
}

}  // namespace

TEST(ZooSaveTests, ARoundTripRestoresTheZooExactly) {
    const std::unique_ptr<Zoo> zoo = LivedInZoo();
    const nlohmann::json saved = ZooToJson(*zoo);
    std::string error;
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_EQ(zoo->Config(), loaded->Config());
    EXPECT_EQ(zoo->Time(), loaded->Time());
    EXPECT_EQ(zoo->Map(), loaded->Map());
    EXPECT_EQ(zoo->Objects(), loaded->Objects());
    EXPECT_EQ(zoo->Habitats(), loaded->Habitats());
    EXPECT_EQ(zoo->Accounts(), loaded->Accounts());
    EXPECT_EQ(zoo->Fauna(), loaded->Fauna());
    EXPECT_EQ(zoo->Sky().State(), loaded->Sky().State());
    EXPECT_TRUE(zoo->SameState(*loaded));
    EXPECT_EQ(zoo->StateHash(), loaded->StateHash());
    EXPECT_EQ(zoo->Weather(), loaded->Weather());
    EXPECT_EQ(zoo->Name(), "Save Me");
    ASSERT_EQ(zoo->Notifications().size(), loaded->Notifications().size());
    EXPECT_EQ(zoo->Notifications().back().text, loaded->Notifications().back().text);
    EXPECT_EQ(zoo->NextNotificationId(), loaded->NextNotificationId());
    // Saving the loaded zoo gives the same document.
    EXPECT_EQ(saved, ZooToJson(*loaded));
}

TEST(ZooSaveTests, ALoadedZooLivesOnLikeTheOriginal) {
    const std::unique_ptr<Zoo> zoo = LivedInZoo();
    std::string error;
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(ZooToJson(*zoo), testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    for (int hour = 0; hour < 40; ++hour) {
        Advance(*zoo, kTicksPerGameHour);
        Advance(*loaded, kTicksPerGameHour);
        ASSERT_EQ(zoo->StateHash(), loaded->StateHash()) << "after hour " << hour;
    }
    EXPECT_TRUE(zoo->SameState(*loaded));
}

TEST(ZooSaveTests, FilesAreWrittenWholeAndSummarised) {
    const std::unique_ptr<Zoo> zoo = LivedInZoo();
    const std::string path = TempFile("roundtrip.zoo");
    std::string error;
    ASSERT_TRUE(SaveZoo(*zoo, path, &error)) << error;
    EXPECT_FALSE(std::filesystem::exists(path + ".tmp"));
    EXPECT_TRUE(std::filesystem::exists(path + kSaveSummarySuffix));
    EXPECT_FALSE(std::filesystem::exists(path + kSaveSummarySuffix + ".tmp"));

    SaveSummary summary;
    ASSERT_TRUE(ReadSaveSummary(path, summary, &error)) << error;
    EXPECT_EQ(summary.name, "Save Me");
    EXPECT_EQ(summary.version, kSaveVersion);
    EXPECT_EQ(summary.animals, zoo->AnimalCount());
    EXPECT_EQ(summary.species, 3);
    EXPECT_EQ(summary.balance, zoo->Accounts().Balance());
    EXPECT_EQ(summary.day, zoo->Time().Day());

    // The sidecar is only a derived cache. Losing it must neither lose nor invalidate the save,
    // and the successful fallback repairs it for the next load-menu visit.
    ASSERT_TRUE(std::filesystem::remove(path + kSaveSummarySuffix));
    error.clear();
    ASSERT_TRUE(ReadSaveSummary(path, summary, &error)) << error;
    EXPECT_EQ(summary.name, "Save Me");
    EXPECT_TRUE(std::filesystem::exists(path + kSaveSummarySuffix));

    const std::unique_ptr<Zoo> loaded =
        LoadZoo(path, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_TRUE(zoo->SameState(*loaded));
    std::filesystem::remove(path);
    std::filesystem::remove(path + kSaveSummarySuffix);
}

TEST(ZooSaveTests, StaleOrDamagedSummarySidecarsAreIgnoredAndRepaired) {
    const std::unique_ptr<Zoo> zoo = LivedInZoo();
    const std::string path = TempFile("stale-summary.zoo");
    const std::string sidecar = path + kSaveSummarySuffix;
    std::string error;
    ASSERT_TRUE(SaveZoo(*zoo, path, &error)) << error;

    // Simulate another program replacing the save while leaving our derived summary behind. The
    // changed file size makes the stamp stale, so the authoritative save must win.
    nlohmann::json replacement;
    {
        std::ifstream in(path, std::ios::binary);
        ASSERT_TRUE(in.good());
        in >> replacement;
    }
    replacement["summary"]["name"] = "A Manually Replaced Zoo";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << replacement.dump();
        ASSERT_TRUE(out.good());
    }
    SaveSummary summary;
    ASSERT_TRUE(ReadSaveSummary(path, summary, &error)) << error;
    EXPECT_EQ(summary.name, "A Manually Replaced Zoo");

    // Malformed cache data has the same semantics as no cache data and is repaired as well.
    std::ofstream(sidecar, std::ios::binary | std::ios::trunc) << "not json";
    error.clear();
    ASSERT_TRUE(ReadSaveSummary(path, summary, &error)) << error;
    EXPECT_EQ(summary.name, "A Manually Replaced Zoo");
    nlohmann::json repaired;
    {
        std::ifstream in(sidecar, std::ios::binary);
        ASSERT_TRUE(in.good());
        EXPECT_NO_THROW(in >> repaired);
    }
    EXPECT_EQ(repaired.value("format", std::string()), "zoo-simulator-save-summary");

    // A sidecar is expected to be tiny. Do not let an unrelated oversized file make the menu
    // allocate and parse it; use the authoritative save and replace it with a small cache.
    std::ofstream(sidecar, std::ios::binary | std::ios::trunc) << std::string(64U * 1024U + 1U, 'x');
    error.clear();
    ASSERT_TRUE(ReadSaveSummary(path, summary, &error)) << error;
    EXPECT_EQ(summary.name, "A Manually Replaced Zoo");
    EXPECT_LT(std::filesystem::file_size(sidecar), 64U * 1024U);

    std::filesystem::remove(path);
    std::filesystem::remove(sidecar);
}

TEST(ZooSaveTests, RefusesWhatIsNotASave) {
    std::string error;
    EXPECT_EQ(LoadZoo(TempFile("missing.zoo"), testdata::GameCatalog(), testdata::GameSpecies(), &error),
              nullptr);
    EXPECT_FALSE(error.empty());

    const std::string garbage = TempFile("garbage.zoo");
    std::ofstream(garbage) << "this is not json {";
    error.clear();
    EXPECT_EQ(LoadZoo(garbage, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr);
    EXPECT_FALSE(error.empty());
    SaveSummary summary;
    EXPECT_FALSE(ReadSaveSummary(garbage, summary, &error));
    std::filesystem::remove(garbage);
    std::filesystem::remove(garbage + kSaveSummarySuffix);

    error.clear();
    EXPECT_EQ(ZooFromJson(nlohmann::json{{"format", "something-else"}, {"version", 1}},
                          testdata::GameCatalog(), testdata::GameSpecies(), &error),
              nullptr);
    EXPECT_FALSE(error.empty());
}

TEST(ZooSaveTests, RefusesSavesFromANewerGame) {
    const std::unique_ptr<Zoo> zoo = LivedInZoo();
    nlohmann::json saved = ZooToJson(*zoo);
    saved["version"] = kSaveVersion + 1;
    std::string error;
    EXPECT_EQ(ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr);
    EXPECT_NE(error.find("newer"), std::string::npos) << error;
}

TEST(ZooSaveTests, DamagedSavesFailWithoutCrashing) {
    const std::unique_ptr<Zoo> zoo = LivedInZoo();
    const nlohmann::json saved = ZooToJson(*zoo);
    for (const char* key :
         {"config", "tick", "weather", "world", "objects", "enclosures", "finance", "animals"}) {
        nlohmann::json damaged = saved;
        damaged["zoo"].erase(key);
        std::string error;
        EXPECT_EQ(ZooFromJson(damaged, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr)
            << key;
        EXPECT_FALSE(error.empty()) << key;

        damaged = saved;
        damaged["zoo"][key] = "nonsense";
        error.clear();
        EXPECT_EQ(ZooFromJson(damaged, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr)
            << key;
    }
}

}  // namespace zoo
