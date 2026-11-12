// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "zoo/world/Terrain.hpp"

namespace zoo {

enum class Diet : std::uint8_t { Carnivore, Herbivore, Omnivore, Piscivore, Insectivore };
enum class ActivityPattern : std::uint8_t { Diurnal, Nocturnal, Crepuscular, Cathemeral };
/// IUCN Red List category.
enum class ConservationStatus : std::uint8_t {
    LeastConcern,
    NearThreatened,
    Vulnerable,
    Endangered,
    CriticallyEndangered,
    ExtinctInTheWild,
    NotEvaluated  ///< domestic forms and taxa the IUCN Red List does not assess
};

std::string_view ToString(Diet v);  // "carnivore", "herbivore", "omnivore", "piscivore", "insectivore"
std::string_view ToString(ActivityPattern v);     // "diurnal", "nocturnal", "crepuscular", "cathemeral"
std::string_view ToString(ConservationStatus v);  // "LC", "NT", "VU", "EN", "CR", "EW", "NE"
/// "Least Concern", "Near Threatened", ...
std::string_view DisplayName(ConservationStatus v);

/// Everything the simulation knows about a species. Loaded from
/// data/species/<id>.json; all values describe real animals (sources are named in
/// the file) except the explicitly game-side ones (price, appeal).
///
/// Durations of biology are real-world values (years, days); the simulation
/// converts them with kGameDaysPerRealYear / kGameDaysPerRealDay (Calendar.hpp).
struct SpeciesDef {
    // ---- identity -------------------------------------------------------------------
    std::string id;              ///< "lion"; file name and save-game key
    std::string commonName;      ///< "African lion"
    std::string scientificName;  ///< "Panthera leo"
    std::string family;          ///< "Felidae"
    std::string model;           ///< asset name: assets/animals/<model>.glb
    /// Free grouping used by the UI and by visitor interests:
    /// "big_cat", "bear", "canine", "ungulate", "pachyderm", "primate", "bird", "marsupial".
    std::string group;

    // ---- body -----------------------------------------------------------------------
    float adultMassKg = 100.0f;
    float shoulderHeightM = 1.0f;  ///< standing height for birds
    float bodyLengthM = 1.5f;
    /// Radius of the circle the body occupies on the ground (collision, spacing).
    float bodyRadiusM = 0.6f;
    float walkSpeed = 1.0f;  ///< m/s, relaxed walk
    float runSpeed = 4.0f;   ///< m/s, the fastest gait the animation set shows
    bool canSwim = false;
    bool canClimb = false;
    float jumpHeightM = 1.0f;  ///< how high a barrier it clears

    // ---- life history (real-world values) -------------------------------------------
    float lifespanYears = 15.0f;  ///< typical in human care
    float maturityYears = 3.0f;
    float gestationDays = 110.0f;  ///< incubation for birds
    int litterMin = 1;
    int litterMax = 1;
    float birthIntervalYears = 2.0f;

    // ---- diet -----------------------------------------------------------------------
    Diet diet = Diet::Herbivore;
    /// Accepted food item ids, most preferred first:
    /// "meat", "fish", "hay", "browse", "grass_pellets", "fruit_veg", "insects", "bamboo".
    std::vector<std::string> foods;
    float foodPerDayKg = 5.0f;  ///< adult daily intake in human care
    float waterPerDayL = 10.0f;

    // ---- behaviour ------------------------------------------------------------------
    ActivityPattern activity = ActivityPattern::Diurnal;
    float sleepHoursPerDay = 10.0f;
    int groupMin = 1;          ///< fewer than this: lonely
    int groupMax = 6;          ///< more than this: crowded
    int maxAdultMales = 0;     ///< per group; 0 = no limit
    float sociability = 0.5f;  ///< 0 solitary .. 1 intensely social
    float playfulness = 0.5f;  ///< how much enrichment it needs, 0..1
    float shyness = 0.5f;      ///< stress from crowds and noise, 0..1
    float aggression = 0.2f;   ///< 0..1, towards keepers and enclosure mates
    bool dangerous = false;    ///< keepers do not enter while it is loose inside

    // ---- habitat --------------------------------------------------------------------
    float minEnclosureM2 = 200.0f;  ///< for the first groupMin animals
    float spacePerExtraAnimalM2 = 50.0f;
    /// How well each ground cover suits the species, 0..1 each (not normalised).
    std::array<float, kTerrainCount> terrainSuitability{};
    float waterAreaFraction = 0.0f;  ///< ideal share of open water in the enclosure
    bool needsDeepWater = false;     ///< needs a pool it can swim or wallow in
    float vegetationDensity = 0.3f;  ///< ideal plant cover, 0 bare .. 1 dense
    bool needsShelter = true;
    float tempMinC = 0.0f;  ///< comfortable air temperature range
    float tempMaxC = 30.0f;

    // ---- containment ----------------------------------------------------------------
    float fenceHeightM = 1.5f;  ///< lowest barrier that holds it
    int fenceStrength = 1;      ///< 1 (light mesh) .. 5 (elephant-proof)

    // ---- management (game side) -----------------------------------------------------
    int price = 1000;  ///< purchase price
    int appeal = 50;   ///< how much visitors want to see it, 0..100
    ConservationStatus status = ConservationStatus::LeastConcern;
    std::string nativeRange;  ///< "Sub-Saharan Africa"
    std::string biome;        ///< "Savanna and open woodland"
    /// Short, factual statements shown on education boards and in the zoopedia.
    std::vector<std::string> facts;
    /// Sound ids (assets/audio/animals/<id>.ogg); may be empty.
    std::vector<std::string> sounds;
    /// Where the numbers come from (free text, for the docs).
    std::vector<std::string> sources;
};

/// The set of species of a game.
class SpeciesCatalog {
public:
    /// Loads every *.json file in `directory` (sorted by file name, so the order is
    /// deterministic). Files that fail to parse or validate are skipped and
    /// reported in Errors().
    static SpeciesCatalog LoadDirectory(const std::string& directory);
    /// Parses one definition from JSON text; on failure returns false and appends
    /// to Errors(). `origin` names the source in messages.
    bool AddFromJson(std::string_view jsonText, std::string_view origin);
    /// Adds a definition after validating it (see Validate()).
    bool Add(SpeciesDef def, std::string_view origin = "code");

    const std::vector<SpeciesDef>& All() const { return species_; }
    /// nullptr if unknown.
    const SpeciesDef* Find(std::string_view id) const;
    /// Index in All(), or -1.
    int IndexOf(std::string_view id) const;
    const std::vector<std::string>& Errors() const { return errors_; }

    /// Problems of a definition (empty = valid): missing id or names, values out of
    /// range, min > max, no foods, no terrain with suitability > 0, ...
    static std::vector<std::string> Validate(const SpeciesDef& def);

private:
    std::vector<SpeciesDef> species_;
    std::vector<std::string> errors_;
};

}  // namespace zoo
