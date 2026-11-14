// SPDX-License-Identifier: MIT
#include "zoo/build/Objects.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "zoo/world/World.hpp"

namespace zoo {
namespace {
template <typename T, typename IdType>
auto Locate(std::vector<T>& values, IdType id) {
    return std::lower_bound(values.begin(), values.end(), id.value,
                            [](const T& item, std::uint32_t value) { return item.id.value < value; });
}
template <typename T, typename IdType>
auto Locate(const std::vector<T>& values, IdType id) {
    return std::lower_bound(values.begin(), values.end(), id.value,
                            [](const T& item, std::uint32_t value) { return item.id.value < value; });
}
int Bucket(float coordinate) {
    const double value = std::floor(static_cast<double>(coordinate) / 16.0);
    if (value < static_cast<double>(std::numeric_limits<int>::min()))
        return std::numeric_limits<int>::min();
    if (value > static_cast<double>(std::numeric_limits<int>::max()))
        return std::numeric_limits<int>::max();
    return static_cast<int>(value);
}
}  // namespace
BuildingId ZooObjects::AddBuilding(Building building) {
    if (nextBuilding_ == 0 || nextBuilding_ == std::numeric_limits<std::uint32_t>::max())
        return {};
    building.id = BuildingId{nextBuilding_++};
    buildings_.push_back(building);
    ++revision_;
    return building.id;
}
bool ZooObjects::RemoveBuilding(BuildingId id) {
    auto it = Locate(buildings_, id);
    if (it == buildings_.end() || it->id != id)
        return false;
    buildings_.erase(it);
    ++revision_;
    return true;
}
Building* ZooObjects::FindBuilding(BuildingId id) {
    auto it = Locate(buildings_, id);
    return it != buildings_.end() && it->id == id ? &*it : nullptr;
}
const Building* ZooObjects::FindBuilding(BuildingId id) const {
    auto it = Locate(buildings_, id);
    return it != buildings_.end() && it->id == id ? &*it : nullptr;
}
PropId ZooObjects::AddProp(Prop prop) {
    if (nextProp_ == 0 || nextProp_ == std::numeric_limits<std::uint32_t>::max())
        return {};
    prop.id = PropId{nextProp_++};
    props_.push_back(prop);
    ++revision_;
    bucketsRevision_ = 0;
    return prop.id;
}
bool ZooObjects::RemoveProp(PropId id) {
    auto it = Locate(props_, id);
    if (it == props_.end() || it->id != id)
        return false;
    props_.erase(it);
    ++revision_;
    bucketsRevision_ = 0;
    return true;
}
Prop* ZooObjects::FindProp(PropId id) {
    auto it = Locate(props_, id);
    return it != props_.end() && it->id == id ? &*it : nullptr;
}
const Prop* ZooObjects::FindProp(PropId id) const {
    auto it = Locate(props_, id);
    return it != props_.end() && it->id == id ? &*it : nullptr;
}
void ZooObjects::RebuildBuckets() const {
    if (bucketsRevision_ == revision_)
        return;
    buckets_.clear();
    for (std::size_t i = 0; i < props_.size(); ++i) {
        const auto& p = props_[i];
        buckets_[{Bucket(p.position.x), Bucket(p.position.z)}].push_back(static_cast<std::uint32_t>(i));
    }
    bucketsRevision_ = revision_;
}
void ZooObjects::PropsNear(Vec2 center, float radius, std::vector<PropId>& out) const {
    out.clear();
    if (!std::isfinite(center.x) || !std::isfinite(center.z) || !std::isfinite(radius) || radius < 0.0f)
        return;
    RebuildBuckets();
    const int minX = Bucket(center.x - radius), maxX = Bucket(center.x + radius),
              minZ = Bucket(center.z - radius), maxZ = Bucket(center.z + radius);
    const float radius2 = radius * radius;
    for (int z = minZ;; ++z) {
        for (int x = minX;; ++x) {
            const auto it = buckets_.find({x, z});
            if (it != buckets_.end())
                for (const std::uint32_t index : it->second) {
                    const Prop& p = props_[index];
                    if (DistanceSquared(center, p.position) <= radius2)
                        out.push_back(p.id);
                }
            if (x == maxX || x == std::numeric_limits<int>::max())
                break;
        }
        if (z == maxZ || z == std::numeric_limits<int>::max())
            break;
    }
    std::sort(out.begin(), out.end(), [](PropId a, PropId b) { return a.value < b.value; });
}
void ZooObjects::PropsInTiles(TileCoord min, TileCoord max, std::vector<PropId>& out) const {
    out.clear();
    if (min.x > max.x || min.z > max.z)
        return;
    RebuildBuckets();
    const float minX = static_cast<float>(min.x) * kTileSize, minZ = static_cast<float>(min.z) * kTileSize,
                maxX = static_cast<float>(max.x + 1) * kTileSize, maxZ = static_cast<float>(max.z + 1) * kTileSize;
    for (const Prop& p : props_)
        if (p.position.x >= minX && p.position.x < maxX && p.position.z >= minZ && p.position.z < maxZ)
            out.push_back(p.id);
}
bool ZooObjects::operator==(const ZooObjects& other) const {
    return buildings_ == other.buildings_ && props_ == other.props_ && nextBuilding_ == other.nextBuilding_ &&
           nextProp_ == other.nextProp_;
}
}  // namespace zoo
