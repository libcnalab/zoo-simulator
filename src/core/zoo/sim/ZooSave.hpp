// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>

#include "zoo/sim/Zoo.hpp"

namespace zoo {

/// The save format: one JSON document,
///   { "format": "zoo-simulator-save", "version": N, "summary": {...}, "zoo": {...} }
/// `version` is bumped whenever a reader of the previous version could no longer
/// read the file; a newer file than the program knows is refused with a clear
/// message, older ones are migrated on load (see ZooFromJson).
inline constexpr const char* kSaveFormat = "zoo-simulator-save";
// Version 2 reserves stand-off ground; old readers must not reinterpret it as habitats.
// Version 3 may store the mesh-backed wildlife hedge (stable fence kind 16).
// Version 4 records reversible Apocalypse breaches. Older readers must refuse an active save
// instead of rebuilding its open habitats as public ground and silently losing restoration data.
inline constexpr int kSaveVersion = 4;
/// Derived summary cache beside a save. Missing or stale files fall back to the save itself.
inline constexpr const char* kSaveSummarySuffix = ".summary";

/// What a load menu shows about a saved game without loading it.
struct SaveSummary {
    std::string name;
    int version = 0;
    std::int64_t day = 0;  ///< 0 = the first day
    int year = 1;
    Money balance = 0;
    int animals = 0;
    int species = 0;
    std::string gameVersion;  ///< VersionString() of the program that wrote it
};

/// Writes the save to `path` (through a temporary file beside it, so that a
/// crash while writing never destroys the previous save).
bool SaveZoo(const Zoo& zoo, const std::string& path, std::string* error = nullptr);
std::unique_ptr<Zoo> LoadZoo(const std::string& path, const Catalog& catalog, const SpeciesCatalog& species,
                             std::string* error = nullptr);
bool ReadSaveSummary(const std::string& path, SaveSummary& out, std::string* error = nullptr);

}  // namespace zoo
