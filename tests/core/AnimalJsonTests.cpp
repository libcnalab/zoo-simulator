// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include "zoo/animals/AnimalsJson.hpp"

namespace zoo {

TEST(AnimalJsonTests, EmptyStateRoundTripsRandomStreamAndCounters) {
    SpeciesCatalog species;
    Animals animals(1234U);
    const nlohmann::json encoded = ToJson(animals, species);
    std::string error;
    const auto decoded = AnimalsFromJson(encoded, species, &error);
    ASSERT_TRUE(decoded.has_value()) << error;
    EXPECT_EQ(animals, *decoded);
    EXPECT_EQ(animals.StateHash(), decoded->StateHash());
}

TEST(AnimalJsonTests, RejectsUnknownSpeciesAndMalformedAnimals) {
    SpeciesCatalog species;
    nlohmann::json invalid = ToJson(Animals{}, species);
    invalid["animals"] = nlohmann::json::array({nlohmann::json{{"id", 1}, {"species", "missing"}}});
    invalid["next_id"] = 2U;
    invalid["alerts"] = nlohmann::json::array({0U});
    std::string error;
    EXPECT_FALSE(AnimalsFromJson(invalid, species, &error).has_value());
    EXPECT_FALSE(error.empty());
}

}  // namespace zoo
