// SPDX-License-Identifier: MIT
// The derived places visitors may use must include the last step off the paving:
// a reachable path tile does not make a seat usable through a fence.
#include <gtest/gtest.h>

#include <cstdint>

#include "TestData.hpp"
#include "zoo/animals/Animals.hpp"
#include "zoo/visitors/VisitorInternal.hpp"

namespace zoo {
namespace {

using visitors_detail::SeatPosition;
using visitors_detail::Sites;

TEST(VisitorSites, ASeatAcrossAFenceIsNotOffered) {
    Catalog catalog = testdata::GameCatalog();
    SpeciesCatalog species = testdata::GameSpecies();
    World world(12, 12);
    for (int z = 6; z < world.Depth(); ++z) {
        world.SetPath({6, z}, PathKind::Asphalt);
    }

    ZooObjects objects;
    Prop bench;
    bench.def = catalog.PropIndex("bench");
    ASSERT_GE(bench.def, 0);
    bench.position = World::TileCenter({7, 6});
    bench.heading = 0.0f;
    objects.AddProp(bench);
    const EdgeRef barrier = EdgeOf({6, 6}, Dir::East);
    const int fence = catalog.fences.front().kind;
    world.SetFence(barrier, {static_cast<std::uint8_t>(fence), false, 1.0f});

    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    Sites sites;
    sites.Refresh(env);
    EXPECT_TRUE(sites.benches.empty());

    world.SetFence(barrier, {});
    enclosures.Rebuild(world, objects, catalog);
    sites.Refresh(env);
    ASSERT_EQ(sites.benches.size(), 1U);
    const visitors_detail::SeatSite usable = sites.benches.front();
    NavProfile ground = NavProfile::Visitor();
    ground.pathsOnly = false;
    for (int seat = 0; seat < usable.seats; ++seat) {
        float face = 0.0f;
        EXPECT_TRUE(nav.Walkable(usable.access, SeatPosition(usable, seat, face), ground));
    }
}

TEST(VisitorSites, RoutesUseTheVerifiedSeatAccessOnArrivalAndDeparture) {
    Catalog catalog = testdata::GameCatalog();
    SpeciesCatalog species = testdata::GameSpecies();
    World world(12, 12);
    for (int z = 6; z < world.Depth(); ++z) {
        world.SetPath({6, z}, PathKind::Asphalt);
    }
    world.SetPath({7, 7}, PathKind::Asphalt);

    ZooObjects objects;
    Prop bench;
    bench.def = catalog.PropIndex("bench");
    ASSERT_GE(bench.def, 0);
    bench.position = World::TileCenter({7, 6});
    bench.heading = 0.0f;
    objects.AddProp(bench);
    const int fence = catalog.fences.front().kind;
    world.SetFence(EdgeOf({6, 6}, Dir::East), {static_cast<std::uint8_t>(fence), false, 1.0f});

    Enclosures enclosures;
    enclosures.Rebuild(world, objects, catalog);
    Navigator nav(world);
    Env env{world, objects, enclosures, catalog, species, nav, nullptr, nullptr, {}, {}};
    Visitors visitors(1);
    Animals animals(1);
    visitors_detail::Stepper stepper(visitors, env, animals);
    stepper.scr.sites.Refresh(env);
    ASSERT_EQ(stepper.scr.sites.benches.size(), 1U);
    const visitors_detail::SeatSite& site = stepper.scr.sites.benches.front();
    EXPECT_EQ(site.access, World::TileCenter(TileCoord{7, 7}));
    float face = 0.0f;
    const Vec2 seat = SeatPosition(site, site.seats / 2, face);
    const Vec2 path = World::TileCenter({6, 10});

    std::vector<Vec2> arriving;
    ASSERT_TRUE(stepper.Route(path, seat, arriving));
    ASSERT_GE(arriving.size(), 2U);
    EXPECT_EQ(arriving[arriving.size() - 2], site.access);
    EXPECT_EQ(arriving.back(), seat);

    std::vector<Vec2> leaving;
    ASSERT_TRUE(stepper.Route(seat, path, leaving));
    ASSERT_FALSE(leaving.empty());
    EXPECT_EQ(leaving.front(), site.access);
}

}  // namespace
}  // namespace zoo
