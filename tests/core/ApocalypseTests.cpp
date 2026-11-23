// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <set>

#include "TestData.hpp"
#include "zoo/sim/VisitorZoo.hpp"
#include "zoo/sim/ZooSaveJson.hpp"

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
    std::string error;
    ASSERT_TRUE(zoo->EnableApocalypse(&error)) << error;
    const ApocalypseState first = zoo->Apocalypse();
    ASSERT_TRUE(first.active);
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

    for (int tick = 0; tick < 200; ++tick) {
        loaded->Step();
    }
    std::size_t loose = 0;
    for (const ApocalypseAnimal& participant : loaded->Apocalypse().animals) {
        const Animal* animal = loaded->Fauna().Find(participant.animal);
        ASSERT_NE(animal, nullptr);
        ASSERT_TRUE(animal->InWorld());
        EXPECT_TRUE(loaded->Map().InBounds(World::TileAt(animal->position))) << animal->name;
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
