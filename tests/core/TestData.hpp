// SPDX-License-Identifier: MIT
#pragma once

#include <string>

#include "zoo/animals/Species.hpp"
#include "zoo/build/Catalog.hpp"

namespace zoo::testdata {

/// The game's own definitions, loaded once per test program.
inline const Catalog& GameCatalog() {
    static const Catalog catalog = Catalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/construction");
    return catalog;
}

inline const SpeciesCatalog& GameSpecies() {
    static const SpeciesCatalog species =
        SpeciesCatalog::LoadDirectory(std::string(ZOO_SOURCE_DIR) + "/data/species");
    return species;
}

}  // namespace zoo::testdata
