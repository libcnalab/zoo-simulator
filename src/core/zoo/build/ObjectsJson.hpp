// SPDX-License-Identifier: MIT
#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include "zoo/build/Objects.hpp"
namespace zoo {
nlohmann::json ToJson(const ZooObjects& objects, const Catalog& catalog);
std::optional<ZooObjects> ObjectsFromJson(const nlohmann::json& json, const Catalog& catalog,
                                          std::string* error = nullptr);
struct ObjectsAccess {
    static const std::vector<Building>& Buildings(const ZooObjects& objects) { return objects.buildings_; }
    static const std::vector<Prop>& Props(const ZooObjects& objects) { return objects.props_; }
    static std::uint32_t NextBuilding(const ZooObjects& objects) { return objects.nextBuilding_; }
    static std::uint32_t NextProp(const ZooObjects& objects) { return objects.nextProp_; }
    static void Set(ZooObjects& objects, std::vector<Building> buildings, std::vector<Prop> props,
                    std::uint32_t nextBuilding, std::uint32_t nextProp, std::uint32_t revision) {
        objects.buildings_ = std::move(buildings);
        objects.props_ = std::move(props);
        objects.nextBuilding_ = nextBuilding;
        objects.nextProp_ = nextProp;
        objects.revision_ = revision;
        objects.bucketsRevision_ = 0;  // the loaded revision may equal the cached one
    }
};
}  // namespace zoo
