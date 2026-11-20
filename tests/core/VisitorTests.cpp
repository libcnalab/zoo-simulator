// SPDX-License-Identifier: MIT
// The visitors as the rest of the game meets them: who comes and at what price,
// what a save keeps, what happens when the zoo changes under their feet.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "TestData.hpp"
#include "zoo/sim/Scenario.hpp"
#include "zoo/sim/ZooSaveJson.hpp"
#include "zoo/visitors/VisitorTuning.hpp"
#include "zoo/visitors/VisitorsJson.hpp"

namespace zoo {

namespace {

bool Travelling(const Visitor& v) {
    return v.activity == VisitorActivity::Walking || v.activity == VisitorActivity::Arriving ||
           v.activity == VisitorActivity::Leaving || v.activity == VisitorActivity::Fleeing;
}

void Advance(Zoo& zoo, std::int64_t ticks) {
    for (std::int64_t i = 0; i < ticks; ++i) {
        zoo.Step();
    }
}

/// Steps until the clock of the running day shows `hour`.
void AdvanceToHour(Zoo& zoo, float hour) {
    while (zoo.Time().Hour() < hour) {
        zoo.Step();
    }
}

std::unique_ptr<Zoo> StarterZoo(std::uint64_t seed = 1) {
    ZooConfig config;
    config.seed = seed;
    auto zoo = std::make_unique<Zoo>(testdata::GameCatalog(), testdata::GameSpecies(), config);
    BuildStarterZoo(*zoo);
    return zoo;
}

float ArrivalsAt(Zoo& zoo, Money price) {
    zoo.Guests().SetTicketPrice(price);
    const Env env = zoo.MakeEnv();
    return zoo.Guests().ExpectedArrivalsPerHour(env, zoo.Fauna());
}

int CountEvents(Zoo& zoo, EventType type) {
    std::vector<Event> events;
    zoo.DrainEvents(events);
    return static_cast<int>(
        std::count_if(events.begin(), events.end(), [type](const Event& e) { return e.type == type; }));
}

}  // namespace

TEST(VisitorTests, TheGateOpensInTheMorning) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    // The game starts at eight; the gate opens at nine.
    {
        const Env env = zoo->MakeEnv();
        EXPECT_EQ(zoo->Guests().ExpectedArrivalsPerHour(env, zoo->Fauna()), 0.0f);
    }
    Advance(*zoo, kTicksPerGameHour - 20);
    EXPECT_EQ(zoo->Guests().Count(), 0);
    AdvanceToHour(*zoo, 11.0f);
    EXPECT_GT(zoo->Guests().Count(), 20);
    EXPECT_GT(zoo->Guests().ArrivedToday(), 20);
    EXPECT_GT(zoo->Accounts().Today().amounts[static_cast<std::size_t>(Account::Tickets)], Dollars(200));
}

TEST(VisitorTests, AClosedZooIsLeftAlone) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 12.0f);
    ASSERT_GT(zoo->Guests().Count(), 20);
    const int arrived = zoo->Guests().ArrivedToday();
    zoo->Guests().SetOpen(false);
    {
        const Env env = zoo->MakeEnv();
        EXPECT_EQ(zoo->Guests().ExpectedArrivalsPerHour(env, zoo->Fauna()), 0.0f);
    }
    // Those on the approach road turn round at the gate; those inside finish their visit.
    Advance(*zoo, kTicksPerGameHour * 2);
    EXPECT_EQ(zoo->Guests().ArrivedToday(), arrived);
    EXPECT_GT(zoo->Guests().Count(), 20);
    for (const Visitor& v : zoo->Guests().All()) {
        EXPECT_TRUE(v.ticketPaid);
    }
    while (zoo->Time().Day() == 0) {
        zoo->Step();
    }
    EXPECT_EQ(zoo->Guests().Count(), 0);
    EXPECT_EQ(zoo->Guests().ArrivedEver(), arrived);
}

TEST(VisitorTests, ADearerTicketDrawsFewerPeople) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 13.0f);
    const Env env = zoo->MakeEnv();
    const float attraction = zoo->Guests().Attraction(env, zoo->Fauna());
    EXPECT_GT(attraction, 0.15f);
    EXPECT_LT(attraction, 0.8f) << "three species are not everything there is to see";
    const Money fair = Visitors::FairTicketPrice(attraction);
    EXPECT_GT(fair, Dollars(8));
    EXPECT_LT(fair, Dollars(40));

    const float free = ArrivalsAt(*zoo, 0);
    const float cheap = ArrivalsAt(*zoo, fair / 2);
    const float right = ArrivalsAt(*zoo, fair);
    const float dear = ArrivalsAt(*zoo, fair * 2);
    const float absurd = ArrivalsAt(*zoo, fair * 5);
    EXPECT_GT(free, cheap);
    EXPECT_GT(cheap, right);
    EXPECT_GT(right, dear * 2.5f) << "double the fair price empties the zoo";
    EXPECT_GT(dear, absurd * 10.0f);
    EXPECT_GT(right, 15.0f);
    EXPECT_LT(right, 60.0f);
    EXPECT_LT(free, right * 1.6f) << "a free zoo is not twice as interesting";
    // The takings peak somewhere near the fair price, not at either end.
    const auto takings = [&](Money price) { return ArrivalsAt(*zoo, price) * static_cast<float>(price); };
    EXPECT_GT(takings(fair), takings(fair / 4));
    EXPECT_GT(takings(fair), takings(fair * 3));
}

TEST(VisitorTests, AnEmptyZooDrawsAlmostNobody) {
    Zoo zoo(testdata::GameCatalog(), testdata::GameSpecies());
    AdvanceToHour(zoo, 13.0f);
    {
        const Env env = zoo.MakeEnv();
        EXPECT_EQ(zoo.Guests().Attraction(env, zoo.Fauna()), 0.0f);
        EXPECT_LT(zoo.Guests().ExpectedArrivalsPerHour(env, zoo.Fauna()), 1.5f);
    }
    std::vector<Event> events;
    while (zoo.Time().Day() == 0) {
        zoo.Step();
    }
    zoo.DrainEvents(events);
    EXPECT_LT(zoo.Guests().ArrivedEver(), 12);
    EXPECT_EQ(zoo.Guests().Count(), 0);
    // Whoever did come said what was wrong with the place.
    const bool arrived = zoo.Guests().ArrivedEver() > 0;
    const bool said = std::any_of(events.begin(), events.end(), [](const Event& e) {
        return e.type == EventType::VisitorThought &&
               e.detail == static_cast<int>(VisitorThought::NothingToSee);
    });
    EXPECT_EQ(arrived, said);
    EXPECT_LT(zoo.Guests().Rating(), 0.6f + 1e-6f);
}

TEST(VisitorTests, RainKeepsPeopleAtHome) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 13.0f);
    Env env = zoo->MakeEnv();
    env.weather.rain = 0.0f;
    env.weather.temperatureC = 22.0f;
    const float fine = zoo->Guests().ExpectedArrivalsPerHour(env, zoo->Fauna());
    env.weather.rain = 0.8f;
    const float wet = zoo->Guests().ExpectedArrivalsPerHour(env, zoo->Fauna());
    env.weather.rain = 0.0f;
    env.weather.temperatureC = -5.0f;
    const float cold = zoo->Guests().ExpectedArrivalsPerHour(env, zoo->Fauna());
    EXPECT_LT(wet, fine * 0.7f);
    EXPECT_LT(cold, fine * 0.9f);
    EXPECT_GT(wet, 0.0f);
}

TEST(VisitorTests, TheSameZooGetsTheSameVisitors) {
    const std::unique_ptr<Zoo> a = StarterZoo(3);
    const std::unique_ptr<Zoo> b = StarterZoo(3);
    const std::unique_ptr<Zoo> c = StarterZoo(4);
    for (int i = 0; i < kTicksPerGameHour * 7; ++i) {
        a->Step();
        b->Step();
        c->Step();
        if (i % 500 == 0) {
            ASSERT_EQ(a->Guests().StateHash(), b->Guests().StateHash()) << "tick " << i;
        }
    }
    ASSERT_GT(a->Guests().Count(), 50);
    EXPECT_TRUE(a->Guests() == b->Guests());
    EXPECT_EQ(a->StateHash(), b->StateHash());
    EXPECT_NE(a->Guests().StateHash(), c->Guests().StateHash());
}

TEST(VisitorTests, GroupsStayTogetherAndKeepTheirShape) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 12.0f);
    int groups = 0;
    int children = 0;
    for (int step = 0; step < 40; ++step) {
        Advance(*zoo, 30);
        const std::vector<Visitor>& all = zoo->Guests().All();
        for (std::size_t i = 0; i < all.size();) {
            ASSERT_TRUE(all[i].leader) << "a group starts with its leader";
            std::size_t end = i + 1;
            while (end < all.size() && all[end].group == all[i].group) {
                EXPECT_FALSE(all[end].leader);
                EXPECT_GT(all[end].id, all[end - 1].id);
                ++end;
            }
            EXPECT_LE(end - i, static_cast<std::size_t>(Visitors::kMaxGroupSize));
            EXPECT_FALSE(all[i].child) << "children do not lead";
            EXPECT_EQ(zoo->Guests().LeaderOf(all[end - 1].id), &all[i]);
            for (std::size_t k = i; k < end; ++k) {
                // Nobody is left behind: the group waits for its slowest.
                EXPECT_LT(Distance(all[k].position, all[i].position), 14.0f)
                    << "visitor " << all[k].id.value << " doing " << ToString(all[k].activity);
                EXPECT_EQ(all[k].goal, all[i].goal);
                EXPECT_GE(all[k].happiness, 0.0f);
                EXPECT_LE(all[k].happiness, 1.0f);
                EXPECT_TRUE(std::isfinite(all[k].position.x) && std::isfinite(all[k].position.z));
                children += all[k].child ? 1 : 0;
            }
            if (step == 0) {
                ++groups;
            }
            i = end;
        }
    }
    EXPECT_GT(groups, 15);
    EXPECT_GT(children, 0);
}

TEST(VisitorTests, WatchersStandAtTheFenceAndTheAnimalsKnow) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 12.5f);
    int watching = 0;
    for (int step = 0; step < 60 && watching < 10; ++step) {
        Advance(*zoo, 20);
        watching = 0;
        for (const Enclosure& enclosure : zoo->Habitats().All()) {
            watching += zoo->Guests().Watching(enclosure.id);
        }
    }
    ASSERT_GE(watching, 10);
    // A group that has just arrived still turns from the path to the fence (`face` is where it
    // turns to): those who watch now are given the time of a half turn before their heading counts.
    std::vector<std::uint32_t> arrived;
    for (const Visitor& v : zoo->Guests().All()) {
        if (v.activity == VisitorActivity::Watching) {
            arrived.push_back(v.id.value);
        }
    }
    Advance(*zoo, static_cast<std::int64_t>(
                      std::ceil(3.2f / (visitor_tuning::kTurnRadiansPerSecond * kTickSeconds))) +
                      1);
    int checked = 0;
    for (const Visitor& v : zoo->Guests().All()) {
        if (v.activity != VisitorActivity::Watching) {
            continue;
        }
        const Enclosure* enclosure = zoo->Habitats().Find(v.targetEnclosure);
        ASSERT_NE(enclosure, nullptr);
        const TileCoord tile = World::TileAt(v.position);
        // A path tile at a corner of the habitat is a viewing place of two of its sides, so the
        // tile alone does not say which fence the visitor stands at: any of them will do.
        bool stands = false;
        bool turnsIn = false;
        bool looksIn = false;
        for (const ViewingSpot& spot : enclosure->viewing) {
            if (spot.tile != tile) {
                continue;
            }
            stands = true;
            const Vec2 in{static_cast<float>(Step(spot.side).x), static_cast<float>(Step(spot.side).z)};
            turnsIn = turnsIn || Dot(HeadingVector(v.face), in) > 0.7f;
            looksIn = looksIn || Dot(HeadingVector(v.heading), in) > 0.7f;
        }
        ASSERT_TRUE(stands) << "visitor " << v.id.value << " watches from nowhere";
        // Looking in, not at the path.
        EXPECT_TRUE(turnsIn) << "visitor " << v.id.value << " at the fence turns away from the habitat";
        if (std::find(arrived.begin(), arrived.end(), v.id.value) != arrived.end()) {
            EXPECT_TRUE(looksIn) << "visitor " << v.id.value << " at the fence looks away from the habitat";
            ++checked;
        }
    }
    EXPECT_GE(checked, 10);
}

TEST(VisitorTests, NobodySharesASeatOrAPlaceAtTheFence) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 11.0f);
    int seated = 0;
    for (int step = 0; step < 150; ++step) {
        Advance(*zoo, 40);
        const std::vector<Visitor>& all = zoo->Guests().All();
        for (std::size_t i = 0; i < all.size(); ++i) {
            const Visitor& a = all[i];
            const bool sits = static_cast<bool>(a.seat) && !Travelling(a);
            const bool watches = a.activity == VisitorActivity::Watching;
            if (!sits && !watches) {
                continue;
            }
            seated += sits ? 1 : 0;
            for (std::size_t k = i + 1; k < all.size(); ++k) {
                const Visitor& b = all[k];
                // Seat against seat, place at the fence against place at the fence.
                const bool both = sits ? static_cast<bool>(b.seat) && !Travelling(b)
                                       : b.activity == VisitorActivity::Watching;
                if (!both) {
                    continue;
                }
                if (sits) {
                    EXPECT_FALSE(a.seat == b.seat && a.slot == b.slot)
                        << a.id.value << " and " << b.id.value << " have the same seat";
                }
                EXPECT_GT(Distance(a.position, b.position), 0.3f)
                    << a.id.value << " and " << b.id.value << " are " << ToString(a.activity)
                    << " in the same place at tick " << zoo->Time().tick << ": seats " << a.seat.value << "/"
                    << static_cast<int>(a.slot) << " and " << b.seat.value << "/" << static_cast<int>(b.slot)
                    << " at " << a.position.x << "," << a.position.z << " and " << b.position.x << ","
                    << b.position.z << " targets " << a.target.x << "," << a.target.z << " and " << b.target.x
                    << "," << b.target.z;
            }
        }
    }
    EXPECT_GT(seated, 0) << "somebody sat down in six hours";
}

TEST(VisitorTests, ASaveKeepsEveryVisitorWhereItWas) {
    const std::unique_ptr<Zoo> zoo = StarterZoo(7);
    AdvanceToHour(*zoo, 13.5f);
    ASSERT_GT(zoo->Guests().Count(), 60);
    ASSERT_FALSE(zoo->Guests().LitterOnGround().empty() && zoo->Guests().ArrivedToday() == 0);

    // Through text, as a file would go.
    const std::string text = ZooToJson(*zoo).dump();
    std::string error;
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(nlohmann::json::parse(text), testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_TRUE(zoo->Guests() == loaded->Guests());
    EXPECT_EQ(zoo->Guests().StateHash(), loaded->Guests().StateHash());
    EXPECT_EQ(zoo->Guests().TicketPrice(), loaded->Guests().TicketPrice());
    EXPECT_EQ(zoo->Guests().Rating(), loaded->Guests().Rating());

    // And they go on exactly as they would have.
    for (int i = 0; i < kTicksPerGameHour * 3; ++i) {
        zoo->Step();
        loaded->Step();
        if (i % 300 == 0) {
            ASSERT_EQ(zoo->Guests().StateHash(), loaded->Guests().StateHash())
                << i << " ticks after the load";
        }
    }
    EXPECT_TRUE(zoo->SameState(*loaded));
}

TEST(VisitorTests, AGroupRemembersEveryRejectedServiceAcrossSaves) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 12.0f);
    nlohmann::json saved = ToJson(zoo->Guests());
    ASSERT_FALSE(saved.at("visitors").empty());
    nlohmann::json& leader = saved["visitors"][0];
    ASSERT_TRUE(leader.at("leader").get<bool>());
    leader["skip_building"] = 23;  // compatibility field for old readers
    leader["avoided_buildings"] = {11, 17, 23};

    std::string error;
    std::optional<Visitors> loaded = VisitorsFromJson(saved, &error);
    ASSERT_TRUE(loaded.has_value()) << error;
    const Visitor& restored = loaded->All().front();
    EXPECT_TRUE(restored.Avoids(BuildingId{11}));
    EXPECT_TRUE(restored.Avoids(BuildingId{17}));
    EXPECT_TRUE(restored.Avoids(BuildingId{23}));
    EXPECT_EQ(restored.avoidedBuildings.size(), 3U);
    const nlohmann::json roundTrip = ToJson(*loaded);
    EXPECT_EQ(roundTrip["visitors"][0]["avoided_buildings"], nlohmann::json({11, 17, 23}));
    EXPECT_EQ(roundTrip["visitors"][0]["skip_building"], 23);

    // A version-one save from before the list existed still restores its one
    // remembered service and gains the list on its next save.
    leader.erase("avoided_buildings");
    leader["skip_building"] = 17;
    loaded = VisitorsFromJson(saved, &error);
    ASSERT_TRUE(loaded.has_value()) << error;
    EXPECT_EQ(loaded->All().front().avoidedBuildings, std::vector<BuildingId>({BuildingId{17}}));
}

TEST(VisitorTests, SavesFromBeforeThereWereVisitorsStillOpen) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 12.0f);
    nlohmann::json saved = ZooToJson(*zoo);
    ASSERT_EQ(saved.at("zoo").erase("visitors"), 1U);
    std::string error;
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_EQ(loaded->Guests().Count(), 0);
    Advance(*loaded, kTicksPerGameHour);
    EXPECT_GT(loaded->Guests().Count(), 5) << "and people come again";
}

TEST(VisitorTests, DamagedVisitorsAreRefusedNotLoaded) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 12.0f);
    const nlohmann::json good = ToJson(zoo->Guests());
    ASSERT_GT(good.at("visitors").size(), 10U);
    std::string error;
    ASSERT_TRUE(VisitorsFromJson(good, &error).has_value()) << error;

    const auto refused = [&](const char* what, const auto& damage) {
        nlohmann::json bad = good;
        damage(bad);
        std::string why;
        EXPECT_FALSE(VisitorsFromJson(bad, &why).has_value()) << what;
        EXPECT_FALSE(why.empty()) << what;
    };
    refused("a need out of range", [](nlohmann::json& j) { j["visitors"][0]["hunger"] = 5.0; });
    refused("ids out of order", [](nlohmann::json& j) { std::swap(j["visitors"][0], j["visitors"][1]); });
    refused("an unknown activity", [](nlohmann::json& j) { j["visitors"][0]["activity"] = "flying"; });
    refused("a missing field", [](nlohmann::json& j) { j["visitors"][0].erase("position"); });
    refused("a route index past the route",
            [](nlohmann::json& j) { j["visitors"][0]["route_index"] = 100000; });
    refused("duplicate avoided buildings",
            [](nlohmann::json& j) { j["visitors"][0]["avoided_buildings"] = {4, 4}; });
    refused("a zero avoided building",
            [](nlohmann::json& j) { j["visitors"][0]["avoided_buildings"] = {0}; });
    refused("too many avoided buildings", [](nlohmann::json& j) {
        j["visitors"][0]["avoided_buildings"] =
            std::vector<std::uint32_t>(Visitor::kMaxAvoidedBuildings + 1, 1);
    });
    refused("an id that would be given again",
            [](nlohmann::json& j) { j["next_id"] = j["visitors"][0]["id"]; });
    refused("a newer format", [](nlohmann::json& j) { j["version"] = 999; });
    refused("not an object", [](nlohmann::json& j) { j = nlohmann::json::array(); });

    // In a whole save the same damage names the part.
    nlohmann::json saved = ZooToJson(*zoo);
    saved["zoo"]["visitors"]["visitors"][0]["thirst"] = -3.0;
    EXPECT_EQ(ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr);
    EXPECT_NE(error.find("visitors"), std::string::npos) << error;
}

TEST(VisitorTests, ATornUpPathSendsPeopleAnotherWayOrHome) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 13.0f);
    ASSERT_GT(zoo->Guests().Count(), 60);
    // Tear up every path in a band right across the zoo, under whoever walks there.
    const World& map = zoo->Map();
    const int row = map.EntranceTile().z - 12;
    std::vector<TileCoord> band;
    for (int z = row; z < row + 2; ++z) {
        for (int x = 0; x < map.Width(); ++x) {
            if (map.At({x, z}).path != PathKind::None) {
                band.push_back({x, z});
            }
        }
    }
    ASSERT_FALSE(band.empty());
    Builder builder = zoo->FreeBuilder();
    ASSERT_TRUE(builder.PlacePath(band, PathKind::None).Ok());

    // Nobody freezes, nobody walks through what is no longer there for long.
    Advance(*zoo, kTicksPerGameHour * 2);
    for (const Visitor& v : zoo->Guests().All()) {
        EXPECT_TRUE(std::isfinite(v.position.x) && std::isfinite(v.position.z));
    }
    // Those cut off from the gate cannot get home the proper way; by the morning
    // they are gone all the same, and so is everybody else.
    while (zoo->Time().Day() == 0) {
        zoo->Step();
    }
    EXPECT_EQ(zoo->Guests().Count(), 0);

    // With the path back, the next day is an ordinary one.
    ASSERT_TRUE(builder.PlacePath(band, PathKind::Asphalt).Ok());
    AdvanceToHour(*zoo, 12.0f);
    EXPECT_GT(zoo->Guests().Count(), 20);
}

TEST(VisitorTests, ALooseLionClearsThePaths) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    AdvanceToHour(*zoo, 13.0f);
    ASSERT_GT(zoo->Guests().Count(), 60);
    CountEvents(*zoo, EventType::VisitorScared);

    const int lion = zoo->SpeciesList().IndexOf("lion");
    ASSERT_GE(lion, 0);
    AnimalId beast;
    for (const Animal& animal : zoo->Fauna().All()) {
        if (animal.species == lion) {
            beast = animal.id;
            break;
        }
    }
    ASSERT_TRUE(beast);
    // Set it down in the middle of the crowd.
    Vec2 middle;
    for (const Visitor& v : zoo->Guests().All()) {
        middle += v.position;
    }
    middle = middle / static_cast<float>(zoo->Guests().Count());
    const auto tile = zoo->Routes().NearestPassable(World::TileAt(middle), NavProfile::Visitor(), 20);
    ASSERT_TRUE(tile.has_value());
    ASSERT_EQ(zoo->MoveAnimal(beast, World::TileCenter(*tile)), ActionError::None);
    const std::uint32_t firstNote = zoo->NextNotificationId();

    Advance(*zoo, kTicksPerSecond * 20);
    const Animal* animal = zoo->Fauna().Find(beast);
    ASSERT_NE(animal, nullptr);
    ASSERT_TRUE(animal->loose);
    EXPECT_GT(CountEvents(*zoo, EventType::VisitorScared), 0);
    int fleeing = 0;
    for (const Visitor& v : zoo->Guests().All()) {
        fleeing += v.activity == VisitorActivity::Fleeing ? 1 : 0;
    }
    EXPECT_GT(fleeing, 0);
    int alarms = 0;
    for (const Notification& n : zoo->Notifications()) {
        if (n.id >= firstNote && n.text.find("running") != std::string::npos) {
            ++alarms;
            EXPECT_EQ(n.severity, Severity::Alarm);
        }
    }
    EXPECT_EQ(alarms, 1) << "one message for the scare, not one for each group";
}

TEST(VisitorTests, TheLooseAnimalAlarmCooldownSurvivesASave) {
    const std::unique_ptr<Zoo> original = StarterZoo();
    AdvanceToHour(*original, 13.0f);
    ASSERT_GT(original->Guests().Count(), 60);
    CountEvents(*original, EventType::VisitorScared);

    const int lion = original->SpeciesList().IndexOf("lion");
    ASSERT_GE(lion, 0);
    AnimalId beast;
    for (const Animal& animal : original->Fauna().All()) {
        if (animal.species == lion) {
            beast = animal.id;
            break;
        }
    }
    ASSERT_TRUE(beast);
    const auto calmLeader = [](const Zoo& zoo) -> const Visitor* {
        const auto found =
            std::find_if(zoo.Guests().All().begin(), zoo.Guests().All().end(), [](const Visitor& visitor) {
                return visitor.leader && visitor.ticketPaid && !visitor.inside &&
                       visitor.goal != VisitorGoal::Flee;
            });
        return found == zoo.Guests().All().end() ? nullptr : &*found;
    };
    const auto alarmNotesFrom = [](const Zoo& zoo, std::uint32_t first) {
        return std::count_if(zoo.Notifications().begin(), zoo.Notifications().end(),
                             [first](const Notification& note) {
                                 return note.id >= first && note.text.find("running") != std::string::npos;
                             });
    };

    const Visitor* firstLeader = calmLeader(*original);
    ASSERT_NE(firstLeader, nullptr);
    const auto firstTile =
        original->Routes().NearestPassable(World::TileAt(firstLeader->position), NavProfile::Visitor(), 4);
    ASSERT_TRUE(firstTile.has_value());
    ASSERT_EQ(original->MoveAnimal(beast, World::TileCenter(*firstTile)), ActionError::None);
    const std::uint32_t firstNote = original->NextNotificationId();
    for (int step = 0; step < 40 && alarmNotesFrom(*original, firstNote) == 0; ++step) {
        original->Step();
    }
    ASSERT_EQ(alarmNotesFrom(*original, firstNote), 1);
    EXPECT_GT(CountEvents(*original, EventType::VisitorScared), 0);

    nlohmann::json saved = ZooToJson(*original);
    ASSERT_EQ(saved["zoo"]["last_scare_note"].get<std::int64_t>(), original->Time().tick);
    std::string error;
    const std::unique_ptr<Zoo> loaded =
        ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error);
    ASSERT_NE(loaded, nullptr) << error;
    EXPECT_TRUE(original->SameState(*loaded));

    const Visitor* secondLeader = calmLeader(*original);
    ASSERT_NE(secondLeader, nullptr);
    const VisitorId secondLeaderId = secondLeader->id;
    const auto secondTile =
        original->Routes().NearestPassable(World::TileAt(secondLeader->position), NavProfile::Visitor(), 4);
    ASSERT_TRUE(secondTile.has_value());
    const Vec2 secondPlace = World::TileCenter(*secondTile);
    ASSERT_EQ(original->MoveAnimal(beast, secondPlace), ActionError::None);
    ASSERT_EQ(loaded->MoveAnimal(beast, secondPlace), ActionError::None);
    ASSERT_NE(loaded->Guests().Find(secondLeaderId), nullptr);
    const std::uint32_t nextNote = loaded->NextNotificationId();
    for (int step = 0; step < 40; ++step) {
        original->Step();
        loaded->Step();
    }
    EXPECT_GT(CountEvents(*original, EventType::VisitorScared), 0);
    EXPECT_GT(CountEvents(*loaded, EventType::VisitorScared), 0);
    EXPECT_EQ(alarmNotesFrom(*original, nextNote), 0);
    EXPECT_EQ(alarmNotesFrom(*loaded, nextNote), 0)
        << "loading must not reset the one-game-hour alarm cooldown";
    EXPECT_TRUE(original->SameState(*loaded));

    nlohmann::json old = saved;
    old["zoo"].erase("last_scare_note");
    EXPECT_NE(ZooFromJson(old, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr) << error;
    saved["zoo"]["last_scare_note"] = original->Time().tick + 1;
    EXPECT_EQ(ZooFromJson(saved, testdata::GameCatalog(), testdata::GameSpecies(), &error), nullptr);
    EXPECT_NE(error.find("last_scare_note"), std::string::npos) << error;
}

TEST(VisitorTests, WrappersGoInTheBinsAndTheNightShiftEmptiesThem) {
    const std::unique_ptr<Zoo> zoo = StarterZoo();
    const Catalog& catalog = zoo->Definitions();
    const auto in_bins = [&] {
        float sum = 0.0f;
        for (const Prop& prop : zoo->Objects().Props()) {
            if (catalog.props[static_cast<std::size_t>(prop.def)].role == PropRole::Bin) {
                sum += prop.amount;
                EXPECT_LE(prop.amount, catalog.props[static_cast<std::size_t>(prop.def)].capacity);
            }
        }
        return sum;
    };
    AdvanceToHour(*zoo, 17.0f);
    EXPECT_GT(in_bins(), 20.0f);
    EXPECT_LE(zoo->Guests().LitterOnGround().size(), static_cast<std::size_t>(Visitors::kMaxLitter));
    while (zoo->Time().Day() == 0) {
        zoo->Step();
    }
    EXPECT_EQ(in_bins(), 0.0f);
    EXPECT_TRUE(zoo->Guests().LitterOnGround().empty());
    EXPECT_EQ(zoo->Guests().ArrivedToday(), 0) << "a new day";

    // Without the night shift what is dropped stays where it fell.
    zoo->SetAutoCare(false);
    AdvanceToHour(*zoo, 17.0f);
    const float evening = in_bins();
    EXPECT_GT(evening, 20.0f);
    while (zoo->Time().Day() == 1) {
        zoo->Step();
    }
    EXPECT_GE(in_bins(), evening);
}

TEST(VisitorTests, TheTicketPriceHasLimits) {
    Visitors visitors(1);
    visitors.SetTicketPrice(-Dollars(5));
    EXPECT_EQ(visitors.TicketPrice(), 0);
    visitors.SetTicketPrice(Dollars(100000));
    EXPECT_EQ(visitors.TicketPrice(), Dollars(200));
    EXPECT_EQ(Visitors::FairTicketPrice(0.0f), Dollars(4));
    EXPECT_EQ(Visitors::FairTicketPrice(1.0f), Dollars(50));
    EXPECT_GT(Visitors::FairTicketPrice(0.6f), Visitors::FairTicketPrice(0.3f));
}

}  // namespace zoo
