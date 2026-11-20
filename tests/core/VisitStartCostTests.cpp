// SPDX-License-Identifier: MIT
//
// A measurement, not a check (docs/RELIABILITY.md, "Start-up"): what the game pays at every
// start for building the Visitor Zoo by script and letting the morning's visitors in, and
// what reading the same state from a save would cost instead.
//
//   ./build/tests/zoo_core_tests --gtest_filter='VisitStartCost.*' --gtest_also_run_disabled_tests
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "TestData.hpp"
#include "zoo/sim/VisitStart.hpp"
#include "zoo/sim/VisitorZoo.hpp"
#include "zoo/sim/ZooSave.hpp"
#include "zoo/sim/ZooSaveJson.hpp"

namespace zoo {
namespace {

double Since(std::chrono::steady_clock::time_point began) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
}

TEST(VisitStartCost, DISABLED_BuildingAndFillingAgainstLoadingASave) {
    const Catalog& catalog = testdata::GameCatalog();
    const SpeciesCatalog& species = testdata::GameSpecies();
    const auto directory = std::filesystem::temp_directory_path() / "zoo-simulator-tests";
    std::filesystem::create_directories(directory);
    const std::string path = (directory / "visit-start-cost.zoo.json").string();
    for (int round = 0; round < 3; ++round) {
        auto began = std::chrono::steady_clock::now();
        Zoo zoo(catalog, species, VisitorZooConfig(1));
        const double made = Since(began);
        began = std::chrono::steady_clock::now();
        BuildVisitorZoo(zoo);
        const double built = Since(began);
        began = std::chrono::steady_clock::now();
        // The game's own start: kVisitPeople and kFillArrivals are VisitStartOptions' defaults.
        FillForVisit(zoo);
        const double filled = Since(began);

        began = std::chrono::steady_clock::now();
        std::string error;
        ASSERT_TRUE(SaveZoo(zoo, path, &error)) << error;
        const double saved = Since(began);
        began = std::chrono::steady_clock::now();
        const std::unique_ptr<Zoo> loaded = LoadZoo(path, catalog, species, &error);
        const double read = Since(began);
        ASSERT_NE(loaded, nullptr) << error;
        EXPECT_EQ(loaded->StateHash(), zoo.StateHash());
        // Measure both paths: an old/moved save without its derived cache, then the menu's normal
        // cached read after that fallback repaired the sidecar.
        std::filesystem::remove(path + kSaveSummarySuffix);
        began = std::chrono::steady_clock::now();
        SaveSummary summary;
        ASSERT_TRUE(ReadSaveSummary(path, summary, &error)) << error;
        const double summarisedCold = Since(began);
        began = std::chrono::steady_clock::now();
        ASSERT_TRUE(ReadSaveSummary(path, summary, &error)) << error;
        const double summarisedCached = Since(began);
        std::cout << "STARTCOST round " << round << ": world " << made << " s, script " << built
                  << " s, fill " << filled << " s = " << made + built + filled << " s | save " << saved
                  << " s, load " << read << " s, summary cold " << summarisedCold << " s, summary cached "
                  << summarisedCached << " s, file " << std::filesystem::file_size(path) / 1024 << " KiB\n";
    }
    std::filesystem::remove(path);
    std::filesystem::remove(path + kSaveSummarySuffix);
}

}  // namespace
}  // namespace zoo
