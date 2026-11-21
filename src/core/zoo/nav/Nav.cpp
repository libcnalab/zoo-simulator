// SPDX-License-Identifier: MIT
#include "zoo/nav/Nav.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <tuple>

namespace zoo {
namespace {

constexpr std::array<Dir, 4> kDirections = {Dir::North, Dir::East, Dir::South, Dir::West};
constexpr float kDiagonal = 1.4142135623730951f;

std::size_t Index(TileCoord tile, int width) {
    return static_cast<std::size_t>(tile.z) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(tile.x);
}

TileCoord Coord(std::size_t index, int width) {
    return {static_cast<int>(index % static_cast<std::size_t>(width)),
            static_cast<int>(index / static_cast<std::size_t>(width))};
}

float Heuristic(TileCoord a, TileCoord b) {
    const int dx = std::abs(a.x - b.x);
    const int dz = std::abs(a.z - b.z);
    const int diagonal = std::min(dx, dz);
    return static_cast<float>(std::max(dx, dz) - diagonal) + kDiagonal * static_cast<float>(diagonal);
}

float TileFactor(const World& world, TileCoord tile, const NavProfile& profile) {
    float factor = 1.0f;
    if (world.At(tile).path == PathKind::None) {
        factor *= profile.offPathCost;
    }
    if (world.HasWater(tile)) {
        factor *= profile.waterCost;
    }
    return factor;
}

bool EdgeOpen(const World& world, TileCoord from, Dir direction, const NavProfile& profile) {
    const Fence fence = world.FenceOn(from, direction);
    if (!fence.Exists()) {
        return true;
    }
    if (!fence.gate) {
        return false;
    }
    if (fence.kind == kPerimeterFence) {
        return profile.useEntrance;
    }
    return profile.useGates || (fence.visitorGate && profile.pathsOnly);
}

bool ClearsObstacles(Vec2 from, Vec2 to, std::span<const NavObstacle> obstacles) {
    const Vec2 travel = to - from;
    const float travelSquared = LengthSquared(travel);
    for (const NavObstacle& obstacle : obstacles) {
        if (obstacle.radius <= 0.0f) {
            continue;
        }
        const float radiusSquared = obstacle.radius * obstacle.radius;
        const Vec2 relative = from - obstacle.position;
        const float startSquared = LengthSquared(relative);
        // A damaged save or newly grown body may begin inside scenery. It must be
        // allowed to move monotonically out, just as the movement collision does.
        if (startSquared < radiusSquared && Dot(travel, relative) >= 0.0f &&
            DistanceSquared(to, obstacle.position) > startSquared) {
            continue;
        }
        const float along =
            travelSquared > 0.0f ? std::clamp(-Dot(relative, travel) / travelSquared, 0.0f, 1.0f) : 0.0f;
        if (DistanceSquared(from + travel * along, obstacle.position) < radiusSquared) {
            return false;
        }
    }
    return true;
}

}  // namespace

Navigator::Navigator(const World& world) : world_(&world) {
    const std::size_t count =
        static_cast<std::size_t>(world.Width()) * static_cast<std::size_t>(world.Depth());
    cost_.resize(count);
    parent_.resize(count);
    stamp_.resize(count);
}

bool Navigator::Passable(TileCoord tile, const NavProfile& profile) const {
    if (!world_->InBounds(tile)) {
        return false;
    }
    const Tile& value = world_->At(tile);
    // Stand-off planting between a habitat's barrier and the public rail is nobody's ground.
    if (static_cast<bool>(value.building) || value.standOff ||
        (profile.pathsOnly && value.path == PathKind::None)) {
        return false;
    }
    if (!world_->IsProperty(tile) && (!profile.leaveProperty || value.path == PathKind::None)) {
        return false;
    }
    if ((profile.keepOffWater && world_->HasWater(tile)) ||
        world_->WaterDepthAt(World::TileCenter(tile)) > profile.maxWaterDepth ||
        world_->TileRelief(tile) > profile.maxRelief) {
        return false;
    }
    return true;
}

bool Navigator::CanStep(TileCoord from, Dir direction, const NavProfile& profile) const {
    const TileCoord to = from + Step(direction);
    return Passable(from, profile) && Passable(to, profile) && EdgeOpen(*world_, from, direction, profile);
}

bool Navigator::Walkable(Vec2 from, Vec2 to, const NavProfile& profile,
                         std::span<const NavObstacle> obstacles) const {
    if (!std::isfinite(from.x) || !std::isfinite(from.z) || !std::isfinite(to.x) || !std::isfinite(to.z)) {
        return false;
    }
    if (from.x < 0.0f || from.z < 0.0f || to.x < 0.0f || to.z < 0.0f || from.x >= world_->SizeX() ||
        to.x >= world_->SizeX() || from.z >= world_->SizeZ() || to.z >= world_->SizeZ()) {
        return false;
    }
    const double x0 = static_cast<double>(from.x) / static_cast<double>(kTileSize);
    const double z0 = static_cast<double>(from.z) / static_cast<double>(kTileSize);
    const double x1 = static_cast<double>(to.x) / static_cast<double>(kTileSize);
    const double z1 = static_cast<double>(to.z) / static_cast<double>(kTileSize);
    const double dx = x1 - x0;
    const double dz = z1 - z0;
    const double epsilon = 1.0e-9;
    std::array<double, 2048> events{};
    std::size_t eventCount = 2;
    events[0] = 0.0;
    events[1] = 1.0;
    if (std::abs(dx) > epsilon) {
        const int low = static_cast<int>(std::floor(std::min(x0, x1))) + 1;
        const int high = static_cast<int>(std::floor(std::max(x0, x1)));
        for (int grid = low; grid <= high; ++grid) {
            const double t = (static_cast<double>(grid) - x0) / dx;
            if (t > epsilon && t < 1.0 - epsilon) {
                events[eventCount] = t;
                ++eventCount;
            }
        }
    }
    if (std::abs(dz) > epsilon) {
        const int low = static_cast<int>(std::floor(std::min(z0, z1))) + 1;
        const int high = static_cast<int>(std::floor(std::max(z0, z1)));
        for (int grid = low; grid <= high; ++grid) {
            const double t = (static_cast<double>(grid) - z0) / dz;
            if (t > epsilon && t < 1.0 - epsilon) {
                events[eventCount] = t;
                ++eventCount;
            }
        }
    }
    std::sort(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(eventCount));
    std::size_t uniqueCount = 0;
    for (std::size_t i = 0; i < eventCount; ++i) {
        if (uniqueCount == 0 || std::abs(events[i] - events[uniqueCount - 1]) > epsilon) {
            events[uniqueCount] = events[i];
            ++uniqueCount;
        }
    }
    eventCount = uniqueCount;
    const auto tileAt = [](double x, double z, double vx, double vz) {
        const double bias = 1.0e-8;
        return TileCoord{static_cast<int>(std::floor(x + vx * bias)),
                         static_cast<int>(std::floor(z + vz * bias))};
    };
    const auto vertexOpen = [this, &profile](double x, double z) {
        if (std::abs(x - std::round(x)) > 1.0e-7 || std::abs(z - std::round(z)) > 1.0e-7) {
            return true;
        }
        const int gx = static_cast<int>(std::round(x));
        const int gz = static_cast<int>(std::round(z));
        const std::array<TileCoord, 4> around = {{{gx - 1, gz - 1}, {gx, gz - 1}, {gx - 1, gz}, {gx, gz}}};
        for (const TileCoord tile : around) {
            if (!Passable(tile, profile)) {
                return false;
            }
        }
        return EdgeOpen(*world_, {gx - 1, gz - 1}, Dir::East, profile) &&
               EdgeOpen(*world_, {gx - 1, gz}, Dir::East, profile) &&
               EdgeOpen(*world_, {gx - 1, gz - 1}, Dir::South, profile) &&
               EdgeOpen(*world_, {gx, gz - 1}, Dir::South, profile);
    };
    if (!vertexOpen(x0, z0) || !vertexOpen(x1, z1)) {
        return false;
    }
    for (std::size_t i = 0; i + 1 < eventCount; ++i) {
        const double middle = (events[i] + events[i + 1]) * 0.5;
        const TileCoord tile = tileAt(x0 + dx * middle, z0 + dz * middle, dx, dz);
        if (!Passable(tile, profile)) {
            return false;
        }
    }
    for (std::size_t i = 1; i + 1 < eventCount; ++i) {
        const double t = events[i];
        const double x = x0 + dx * t;
        const double z = z0 + dz * t;
        const bool crossX = std::abs(x - std::round(x)) <= 1.0e-7 && std::abs(dx) > epsilon;
        const bool crossZ = std::abs(z - std::round(z)) <= 1.0e-7 && std::abs(dz) > epsilon;
        const TileCoord before = tileAt(x0 + dx * (t - 1.0e-7), z0 + dz * (t - 1.0e-7), dx, dz);
        const TileCoord after = tileAt(x0 + dx * (t + 1.0e-7), z0 + dz * (t + 1.0e-7), dx, dz);
        if (crossX && crossZ) {
            const int gx = static_cast<int>(std::round(x));
            const int gz = static_cast<int>(std::round(z));
            const std::array<TileCoord, 4> around = {
                {{gx - 1, gz - 1}, {gx, gz - 1}, {gx - 1, gz}, {gx, gz}}};
            for (const TileCoord tile : around) {
                if (!Passable(tile, profile)) {
                    return false;
                }
            }
            if (!EdgeOpen(*world_, {gx - 1, gz - 1}, Dir::East, profile) ||
                !EdgeOpen(*world_, {gx - 1, gz}, Dir::East, profile) ||
                !EdgeOpen(*world_, {gx - 1, gz - 1}, Dir::South, profile) ||
                !EdgeOpen(*world_, {gx, gz - 1}, Dir::South, profile)) {
                return false;
            }
        } else if (before != after) {
            Dir direction = Dir::North;
            if (after.x > before.x) {
                direction = Dir::East;
            } else if (after.x < before.x) {
                direction = Dir::West;
            } else if (after.z > before.z) {
                direction = Dir::South;
            }
            if (!EdgeOpen(*world_, before, direction, profile)) {
                return false;
            }
        }
    }
    if (std::abs(dx) < epsilon && std::abs(x0 - std::round(x0)) <= 1.0e-7) {
        const int gx = static_cast<int>(std::round(x0));
        const int firstZ = static_cast<int>(std::floor(std::min(z0, z1)));
        const int lastZ = static_cast<int>(std::floor(std::max(z0, z1)));
        for (int z = firstZ; z <= lastZ; ++z) {
            if (!EdgeOpen(*world_, {gx - 1, z}, Dir::East, profile) || !Passable({gx - 1, z}, profile) ||
                !Passable({gx, z}, profile)) {
                return false;
            }
        }
    }
    if (std::abs(dz) < epsilon && std::abs(z0 - std::round(z0)) <= 1.0e-7) {
        const int gz = static_cast<int>(std::round(z0));
        const int firstX = static_cast<int>(std::floor(std::min(x0, x1)));
        const int lastX = static_cast<int>(std::floor(std::max(x0, x1)));
        for (int x = firstX; x <= lastX; ++x) {
            if (!EdgeOpen(*world_, {x, gz - 1}, Dir::South, profile) || !Passable({x, gz - 1}, profile) ||
                !Passable({x, gz}, profile)) {
                return false;
            }
        }
    }
    return Passable(World::TileAt(from), profile) && Passable(World::TileAt(to), profile) &&
           ClearsObstacles(from, to, obstacles);
}

bool Navigator::FindPath(Vec2 from, Vec2 to, const NavProfile& profile, std::vector<Vec2>& out,
                         int maxExpansions, std::span<const NavObstacle> obstacles) const {
    out.clear();
    const TileCoord start = World::TileAt(from);
    const TileCoord goal = World::TileAt(to);
    if (!Passable(start, profile) || !Passable(goal, profile) || maxExpansions <= 0) {
        return false;
    }
    if (start == goal) {
        if (Walkable(from, to, profile, obstacles)) {
            out.push_back(to);
            return true;
        }
        return false;
    }
    ++generation_;
    if (generation_ == 0) {
        std::fill(stamp_.begin(), stamp_.end(), 0);
        generation_ = 1;
    }
    open_.clear();
    const std::int32_t startIndex = static_cast<std::int32_t>(Index(start, world_->Width()));
    const std::int32_t goalIndex = static_cast<std::int32_t>(Index(goal, world_->Width()));
    cost_[static_cast<std::size_t>(startIndex)] = 0.0f;
    stamp_[static_cast<std::size_t>(startIndex)] = generation_;
    parent_[static_cast<std::size_t>(startIndex)] = -1;
    const auto lessNode = [](const OpenNode& a, const OpenNode& b) {
        const float fa = a.g + a.h;
        const float fb = b.g + b.h;
        if (fa != fb) {
            return fa > fb;
        }
        if (a.h != b.h) {
            return a.h > b.h;
        }
        return a.index > b.index;
    };
    const auto push = [this, &lessNode](OpenNode node) {
        open_.push_back(node);
        std::push_heap(open_.begin(), open_.end(), lessNode);
    };
    push({startIndex, 0.0f, Heuristic(start, goal)});
    int expanded = 0;
    bool found = false;
    while (!open_.empty()) {
        std::pop_heap(open_.begin(), open_.end(), lessNode);
        const OpenNode current = open_.back();
        open_.pop_back();
        const std::size_t currentIndex = static_cast<std::size_t>(current.index);
        if (stamp_[currentIndex] != generation_ || current.g != cost_[currentIndex]) {
            continue;
        }
        if (expanded >= maxExpansions) {
            return false;
        }
        ++expanded;
        if (current.index == goalIndex) {
            found = true;
            break;
        }
        const TileCoord tile = Coord(currentIndex, world_->Width());
        for (const Dir direction : kDirections) {
            const TileCoord delta = Step(direction);
            const TileCoord next = tile + delta;
            if (!CanStep(tile, direction, profile)) {
                continue;
            }
            if (!ClearsObstacles(World::TileCenter(tile), World::TileCenter(next), obstacles)) {
                continue;
            }
            const float length = 1.0f;
            const float nextCost =
                current.g +
                length * (TileFactor(*world_, tile, profile) + TileFactor(*world_, next, profile)) * 0.5f;
            const std::size_t nextIndex = Index(next, world_->Width());
            if (stamp_[nextIndex] == generation_ && nextCost >= cost_[nextIndex]) {
                continue;
            }
            stamp_[nextIndex] = generation_;
            cost_[nextIndex] = nextCost;
            parent_[nextIndex] = current.index;
            push({static_cast<std::int32_t>(nextIndex), nextCost, Heuristic(next, goal)});
        }
        for (int dz = -1; dz <= 1; dz += 2) {
            for (int dx = -1; dx <= 1; dx += 2) {
                const TileCoord next{tile.x + dx, tile.z + dz};
                const Dir horizontal = dx > 0 ? Dir::East : Dir::West;
                const Dir vertical = dz > 0 ? Dir::South : Dir::North;
                const TileCoord horizontalTile = tile + Step(horizontal);
                const TileCoord verticalTile = tile + Step(vertical);
                if (!CanStep(tile, horizontal, profile) || !CanStep(tile, vertical, profile) ||
                    !CanStep(horizontalTile, vertical, profile) ||
                    !CanStep(verticalTile, horizontal, profile)) {
                    continue;
                }
                if (!ClearsObstacles(World::TileCenter(tile), World::TileCenter(next), obstacles)) {
                    continue;
                }
                const float nextCost =
                    current.g +
                    kDiagonal * (TileFactor(*world_, tile, profile) + TileFactor(*world_, next, profile)) *
                        0.5f;
                const std::size_t nextIndex = Index(next, world_->Width());
                if (stamp_[nextIndex] == generation_ && nextCost >= cost_[nextIndex]) {
                    continue;
                }
                stamp_[nextIndex] = generation_;
                cost_[nextIndex] = nextCost;
                parent_[nextIndex] = current.index;
                push({static_cast<std::int32_t>(nextIndex), nextCost, Heuristic(next, goal)});
            }
        }
    }
    if (!found) {
        return false;
    }
    for (std::int32_t index = goalIndex; index != startIndex;
         index = parent_[static_cast<std::size_t>(index)]) {
        out.push_back(World::TileCenter(Coord(static_cast<std::size_t>(index), world_->Width())));
    }
    std::reverse(out.begin(), out.end());
    out.back() = to;
    Vec2 anchor = from;
    std::size_t candidate = 0;
    std::size_t write = 0;
    const std::size_t rawSize = out.size();
    const auto avoidsCostlyWater = [this, &profile](Vec2 a, Vec2 b) {
        if (profile.waterCost <= 1.0f) {
            return true;
        }
        const float distance = Distance(a, b);
        const int samples = std::max(1, static_cast<int>(std::ceil(distance * 2.0f)));
        for (int i = 0; i <= samples; ++i) {
            const float amount = static_cast<float>(i) / static_cast<float>(samples);
            if (world_->HasWater(World::TileAt(Lerp(a, b, amount)))) {
                return false;
            }
        }
        return true;
    };
    while (candidate < rawSize) {
        std::size_t farthest = candidate;
        for (std::size_t test = rawSize; test > candidate; --test) {
            const Vec2 end = test == rawSize ? to : out[test - 1];
            if (Walkable(anchor, end, profile, obstacles) && avoidsCostlyWater(anchor, end)) {
                farthest = test - 1;
                break;
            }
        }
        const Vec2 selected = farthest == rawSize - 1 ? to : out[farthest];
        out[write] = selected;
        ++write;
        anchor = selected;
        candidate = farthest + 1;
    }
    out.resize(write);
    return true;
}

float Navigator::PathLength(Vec2 from, Vec2 to, const NavProfile& profile) const {
    std::vector<Vec2> path;
    if (!FindPath(from, to, profile, path)) {
        return -1.0f;
    }
    float length = 0.0f;
    Vec2 previous = from;
    for (const Vec2 point : path) {
        length += Distance(previous, point);
        previous = point;
    }
    return length;
}

std::optional<TileCoord> Navigator::NearestPassable(TileCoord around, const NavProfile& profile,
                                                    int maxRadius) const {
    if (maxRadius < 0) {
        return std::nullopt;
    }
    std::optional<TileCoord> best;
    int bestDistance = std::numeric_limits<int>::max();
    const int minX = std::max(0, around.x - maxRadius);
    const int maxX = std::min(world_->Width() - 1, around.x + maxRadius);
    const int minZ = std::max(0, around.z - maxRadius);
    const int maxZ = std::min(world_->Depth() - 1, around.z + maxRadius);
    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            const TileCoord tile{x, z};
            if (!Passable(tile, profile)) {
                continue;
            }
            const int dx = x - around.x;
            const int dz = z - around.z;
            const int distance = dx * dx + dz * dz;
            if (distance <= maxRadius * maxRadius && distance < bestDistance) {
                best = tile;
                bestDistance = distance;
            }
        }
    }
    return best;
}

void Navigator::DistanceField(std::span<const TileCoord> goals, const NavProfile& profile,
                              std::vector<float>& out) const {
    const std::size_t count =
        static_cast<std::size_t>(world_->Width()) * static_cast<std::size_t>(world_->Depth());
    out.assign(count, -1.0f);
    struct Node {
        float cost;
        std::size_t index;
    };
    const auto compare = [](const Node& a, const Node& b) {
        return a.cost != b.cost ? a.cost > b.cost : a.index > b.index;
    };
    std::priority_queue<Node, std::vector<Node>, decltype(compare)> queue(compare);
    for (const TileCoord goal : goals) {
        if (Passable(goal, profile)) {
            const std::size_t index = Index(goal, world_->Width());
            if (out[index] < 0.0f) {
                out[index] = 0.0f;
                queue.push({0.0f, index});
            }
        }
    }
    while (!queue.empty()) {
        const Node current = queue.top();
        queue.pop();
        if (out[current.index] != current.cost) {
            continue;
        }
        const TileCoord tile = Coord(current.index, world_->Width());
        for (const Dir direction : kDirections) {
            if (!CanStep(tile, direction, profile)) {
                continue;
            }
            const TileCoord next = tile + Step(direction);
            const std::size_t nextIndex = Index(next, world_->Width());
            const float nextCost =
                current.cost +
                (TileFactor(*world_, tile, profile) + TileFactor(*world_, next, profile)) * 0.5f;
            if (out[nextIndex] < 0.0f || nextCost < out[nextIndex]) {
                out[nextIndex] = nextCost;
                queue.push({nextCost, nextIndex});
            }
        }
        for (int dz = -1; dz <= 1; dz += 2) {
            for (int dx = -1; dx <= 1; dx += 2) {
                const TileCoord next{tile.x + dx, tile.z + dz};
                const Dir horizontal = dx > 0 ? Dir::East : Dir::West;
                const Dir vertical = dz > 0 ? Dir::South : Dir::North;
                const TileCoord horizontalTile = tile + Step(horizontal);
                const TileCoord verticalTile = tile + Step(vertical);
                if (!CanStep(tile, horizontal, profile) || !CanStep(tile, vertical, profile) ||
                    !CanStep(horizontalTile, vertical, profile) ||
                    !CanStep(verticalTile, horizontal, profile)) {
                    continue;
                }
                const std::size_t nextIndex = Index(next, world_->Width());
                const float nextCost =
                    current.cost +
                    kDiagonal * (TileFactor(*world_, tile, profile) + TileFactor(*world_, next, profile)) *
                        0.5f;
                if (out[nextIndex] < 0.0f || nextCost < out[nextIndex]) {
                    out[nextIndex] = nextCost;
                    queue.push({nextCost, nextIndex});
                }
            }
        }
    }
}

int Navigator::ComponentOf(TileCoord tile, const NavProfile& profile) const {
    if (!world_->InBounds(tile) || !Passable(tile, profile)) {
        return -1;
    }
    const std::uint32_t revision = world_->NavRevision();
    for (auto it = components_.begin(); it != components_.end(); ++it) {
        if (it->profile == profile && it->revision == revision) {
            const int label = it->labels[Index(tile, world_->Width())];
            if (it != components_.begin()) {
                ComponentCache cache = std::move(*it);
                components_.erase(it);
                components_.insert(components_.begin(), std::move(cache));
            }
            return label;
        }
    }
    ComponentCache cache;
    cache.profile = profile;
    cache.revision = revision;
    const std::size_t count =
        static_cast<std::size_t>(world_->Width()) * static_cast<std::size_t>(world_->Depth());
    cache.labels.assign(count, -1);
    int component = 0;
    std::vector<TileCoord> queue;
    for (int z = 0; z < world_->Depth(); ++z) {
        for (int x = 0; x < world_->Width(); ++x) {
            const TileCoord start{x, z};
            const std::size_t startIndex = Index(start, world_->Width());
            if (cache.labels[startIndex] >= 0 || !Passable(start, profile)) {
                continue;
            }
            queue.clear();
            queue.push_back(start);
            cache.labels[startIndex] = component;
            for (std::size_t head = 0; head < queue.size(); ++head) {
                const TileCoord current = queue[head];
                for (const Dir direction : kDirections) {
                    if (!CanStep(current, direction, profile)) {
                        continue;
                    }
                    const TileCoord next = current + Step(direction);
                    const std::size_t nextIndex = Index(next, world_->Width());
                    if (cache.labels[nextIndex] < 0) {
                        cache.labels[nextIndex] = component;
                        queue.push_back(next);
                    }
                }
            }
            ++component;
        }
    }
    components_.insert(components_.begin(), std::move(cache));
    if (components_.size() > 8u) {
        components_.pop_back();
    }
    return components_.front().labels[Index(tile, world_->Width())];
}

bool FollowPath(std::span<const Vec2> waypoints, std::size_t& index, Vec2& position, float distance,
                float arriveRadius) {
    if (waypoints.empty() || index >= waypoints.size()) {
        return true;
    }
    float remaining = std::max(distance, 0.0f);
    while (index < waypoints.size()) {
        const Vec2 delta = waypoints[index] - position;
        const float length = Length(delta);
        if (length <= arriveRadius) {
            position = waypoints[index];
            ++index;
            continue;
        }
        if (remaining <= 0.0f) {
            return false;
        }
        if (length <= remaining + arriveRadius) {
            position = waypoints[index];
            remaining = std::max(0.0f, remaining - length);
            ++index;
            continue;
        }
        position += delta * (remaining / length);
        return false;
    }
    return true;
}

}  // namespace zoo
