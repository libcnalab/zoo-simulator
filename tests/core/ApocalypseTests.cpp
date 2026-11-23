// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <set>

#include "TestData.hpp"
#include "zoo/animals/AnimalTuning.hpp"
#include "zoo/sim/VisitorZoo.hpp"
#include "zoo/sim/ZooSaveJson.hpp"
#include "zoo/visitors/VisitorInternal.hpp"

namespace zoo {
namespace {

std::unique_ptr<Zoo> ApocalypseZoo() {
    static const nlohmann::json base = [] {
        Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies(), VisitorZooConfig(1));
        const ScenarioReport report = BuildVisitorZoo(zoo);
        for (const std::string& problem : report.problems) {
            ADD_FAILURE() << problem;
        }
        return ZooToJson(zoo);
    }();
    std::string error;
    std::unique_ptr<Zoo> zoo = ZooFromJson(base, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    EXPECT_NE(zoo, nullptr) << error;
    return zoo;
}

TEST(Apocalypse, EveryOutdoorHabitatGetsOneSavedReachableBreachAndRestoresExactly) {
    std::unique_ptr<Zoo> zoo = ApocalypseZoo();
    const World before = zoo->Map();
    visitors_detail::Sites visitorSites;
    visitorSites.Refresh(zoo->MakeEnv());
    const std::uint32_t ordinaryVisitorNav = visitorSites.nav;
    const std::size_t ordinaryViews = visitorSites.views.size();
    const std::size_t ordinaryServices = visitorSites.services.size();
    std::string error;
    const auto enableBegan = std::chrono::steady_clock::now();
    ASSERT_TRUE(zoo->EnableApocalypse(&error)) << error;
    std::cout << "APOCALYPSE enable "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - enableBegan).count()
              << " s\n";
    const ApocalypseState first = zoo->Apocalypse();
    ASSERT_TRUE(first.active);
    visitorSites.Refresh(zoo->MakeEnv());
    EXPECT_EQ(visitorSites.nav, ordinaryVisitorNav);
    EXPECT_EQ(visitorSites.views.size(), ordinaryViews);
    EXPECT_EQ(visitorSites.services.size(), ordinaryServices);
    EXPECT_EQ(first.breaches.size(), zoo->Habitats().All().size() - 2U);  // two sealed snake terraria
    EXPECT_EQ(first.animals.size(), zoo->Fauna().All().size() - 2U);

    std::set<std::uint32_t> homes;
    for (const ApocalypseBreach& breach : first.breaches) {
        SCOPED_TRACE(breach.enclosure.value);
        EXPECT_TRUE(homes.insert(breach.enclosure.value).second);
        EXPECT_TRUE(breach.former.Exists());
        EXPECT_TRUE(breach.former.gate);
        EXPECT_NE(breach.former.kind, kPerimeterFence);
        EXPECT_FALSE(zoo->Map().FenceAt(breach.edge).Exists());
        const TileCoord a = breach.edge.horizontal ? TileCoord{breach.edge.x, breach.edge.z - 1}
                                                   : TileCoord{breach.edge.x - 1, breach.edge.z};
        const TileCoord b = breach.edge.horizontal ? TileCoord{breach.edge.x, breach.edge.z}
                                                   : TileCoord{breach.edge.x, breach.edge.z};
        const TileCoord outside = zoo->Habitats().At(a) == breach.enclosure ? b : a;
        EXPECT_EQ(zoo->Habitats().RegionAt(outside), Region::Public);
        EXPECT_TRUE(zoo->Routes().Passable(outside, NavProfile::Animal(0.0f, false)));
    }

    const nlohmann::json activeSave = ZooToJson(*zoo);
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(activeSave, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_TRUE(loaded->Apocalypse().active);
    EXPECT_TRUE(zoo->SameState(*loaded));
    EXPECT_EQ(activeSave, ZooToJson(*loaded));
    zoo->DisableApocalypse();
    visitorSites.Refresh(zoo->MakeEnv());
    EXPECT_EQ(visitorSites.nav, zoo->Map().NavRevision());
    EXPECT_NE(visitorSites.nav, ordinaryVisitorNav);

    double stepTotal = 0.0;
    double stepLongest = 0.0;
    int stepLongestAt = -1;
    for (int tick = 0; tick < 200; ++tick) {
        const auto stepBegan = std::chrono::steady_clock::now();
        loaded->Step();
        const double seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - stepBegan).count();
        stepTotal += seconds;
        if (seconds > stepLongest) {
            stepLongest = seconds;
            stepLongestAt = tick;
        }
    }
    std::cout << "APOCALYPSE 200 steps " << stepTotal << " s; longest " << stepLongest << " s at "
              << stepLongestAt << "\n";
    // Ten simulated seconds must leave at least half of one CPU for drawing and input. The old
    // whole-estate shore scan took about nine seconds here and then kept getting slower.
    EXPECT_LT(stepTotal, 5.0);
    EXPECT_LT(stepLongest, 2.0);
    std::size_t loose = 0;
    for (const ApocalypseAnimal& participant : loaded->Apocalypse().animals) {
        const Animal* animal = loaded->Fauna().Find(participant.animal);
        ASSERT_NE(animal, nullptr);
        ASSERT_TRUE(animal->InWorld());
        EXPECT_TRUE(loaded->Map().InBounds(World::TileAt(animal->position))) << animal->name;
        const SpeciesDef& species = loaded->SpeciesList().All()[static_cast<std::size_t>(animal->species)];
        const float wade = species.canSwim
                               ? tuning::kWadingShoulderFraction * species.shoulderHeightM *
                                     BodyScale(*animal, species, loaded->Time())
                               : 0.0f;
        EXPECT_TRUE(loaded->Routes().Passable(World::TileAt(animal->position),
                                              NavProfile::Animal(wade, species.canSwim)))
            << animal->name << " stands on an impassable tile";
        loose += animal->loose ? 1U : 0U;
    }
    EXPECT_GT(loose, 0U);

    loaded->DisableApocalypse();
    EXPECT_FALSE(loaded->Apocalypse().active);
    EXPECT_EQ(loaded->Map(), before);
    for (const ApocalypseAnimal& participant : first.animals) {
        const Animal* animal = loaded->Fauna().Find(participant.animal);
        ASSERT_NE(animal, nullptr);
        EXPECT_FALSE(animal->loose) << animal->name;
        EXPECT_TRUE(animal->activity == AnimalActivity::Idle || animal->activity == AnimalActivity::Crated)
            << animal->name;
        if (animal->InWorld()) {
            EXPECT_EQ(loaded->Habitats().At(animal->position), animal->home) << animal->name;
        }
    }

    ASSERT_TRUE(loaded->EnableApocalypse(&error)) << error;
    ASSERT_EQ(loaded->Apocalypse().breaches.size(), first.breaches.size());
    for (std::size_t i = 0; i < first.breaches.size(); ++i) {
        EXPECT_EQ(loaded->Apocalypse().breaches[i].enclosure, first.breaches[i].enclosure);
        EXPECT_EQ(loaded->Apocalypse().breaches[i].edge, first.breaches[i].edge);
        EXPECT_EQ(loaded->Apocalypse().breaches[i].former, first.breaches[i].former);
    }
    loaded->DisableApocalypse();
    EXPECT_EQ(loaded->Map(), before);
}

TEST(Apocalypse, DamagedActiveSaveIsRefusedInsteadOfLoadingAHalfRestoredZoo) {
    std::unique_ptr<Zoo> zoo = ApocalypseZoo();
    std::string error;
    ASSERT_TRUE(zoo->EnableApocalypse(&error)) << error;
    nlohmann::json saved = ZooToJson(*zoo);
    saved["zoo"]["apocalypse"]["breaches"][0]["fence"]["kind"] = 255;
    EXPECT_EQ(ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr);
    EXPECT_NE(error.find("Apocalypse"), std::string::npos) << error;
}

}  // namespace
}  // namespace zoo
