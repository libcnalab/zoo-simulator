// SPDX-License-Identifier: MIT
#pragma once

#include "zoo/animals/Animals.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace zoo {

nlohmann::json ToJson(const Animals& animals, const SpeciesCatalog& species);
std::optional<Animals> AnimalsFromJson(const nlohmann::json& json, const SpeciesCatalog& species,
                                       std::string* error = nullptr);

struct AnimalsAccess {
    static const std::vector<Animal>& List(const Animals& animals) { return animals.animals_; }
    static const std::vector<Animals::EnclosureCare>& Care(const Animals& animals) { return animals.care_; }
    static auto EmptyCare() { return std::vector<Animals::EnclosureCare>{}; }
    static void AddCare(Animals& animals, EnclosureId id, float dirt) {
        animals.care_.push_back(Animals::EnclosureCare{id, dirt, 0});
    }
    static std::uint32_t NextId(const Animals& animals) { return animals.nextId_; }
    static std::uint64_t RngState(const Animals& animals) { return animals.rng_.State(); }
    static std::uint64_t RngStream(const Animals& animals) { return animals.rng_.Stream(); }
    static int Births(const Animals& animals) { return animals.births_; }
    static int Deaths(const Animals& animals) { return animals.deaths_; }
    static int Escapes(const Animals& animals) { return animals.escapes_; }
    static void Restore(Animals& animals, std::vector<Animal> list, std::vector<Animals::EnclosureCare> care,
                        std::uint32_t nextId, std::uint64_t state, std::uint64_t stream, int births,
                        int deaths, int escapes) {
        animals.animals_ = std::move(list);
        animals.care_ = std::move(care);
        animals.nextId_ = nextId;
        animals.rng_.Restore(state, stream);
        animals.births_ = births;
        animals.deaths_ = deaths;
        animals.escapes_ = escapes;
    }
};

}  // namespace zoo
