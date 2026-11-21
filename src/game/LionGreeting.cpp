// SPDX-License-Identifier: MIT
#include "LionGreeting.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zoogame {
namespace {

using Microsoft::Xna::Framework::Quaternion;

struct Bend {
    NeckTurns turns = kNoNeckTurn;
    Vector3 point;
};

Bend BendTowards(LionNeck neck, const Vector3& target, const std::array<float, 3>& limits) {
    Bend bent;
    for (std::size_t i = 0; i < neck.joints.size(); ++i) {
        const Vector3 pivot = neck.joints[i];
        const Vector3 from = neck.landmark - pivot;
        const Vector3 to = target - pivot;
        const Vector3 axis = Vector3::Cross(from, to);
        const float length = axis.Length();
        if (length < 1.0e-7f)
            continue;
        const float angle = std::min(std::atan2(length, Vector3::Dot(from, to)),
                                     limits[i] * std::numbers::pi_v<float> / 180.0f);
        const Quaternion turn = Quaternion::CreateFromAxisAngle(axis / length, angle);
        bent.turns[i] = turn;
        neck.landmark = pivot + Vector3::Transform(from, turn);
        for (std::size_t j = i + 1; j < neck.joints.size(); ++j)
            neck.joints[j] = pivot + Vector3::Transform(neck.joints[j] - pivot, turn);
    }
    bent.point = neck.landmark;
    return bent;
}

}  // namespace

std::optional<LionGreeting> FitLionGreeting(const LionNeck& muzzle, const LionNeck& cheek,
                                            float smallerHeight) {
    if (smallerHeight <= 0.0f ||
        Vector3::DistanceSquared(muzzle.landmark, cheek.landmark) > smallerHeight * smallerHeight)
        return std::nullopt;
    constexpr std::array pressing{19.0f, 16.0f, 14.0f};
    constexpr std::array receiving{14.0f, 11.0f, 10.0f};
    const Vector3 middle = (muzzle.landmark + cheek.landmark) * 0.5f;
    const Bend first = BendTowards(muzzle, middle, pressing);
    const Bend other = BendTowards(cheek, first.point, receiving);
    const Bend rub = BendTowards(muzzle, other.point, pressing);
    const float tolerance = 0.075f * smallerHeight;
    if (Vector3::DistanceSquared(rub.point, other.point) > tolerance * tolerance)
        return std::nullopt;
    return LionGreeting{rub.turns, other.turns};
}

std::array<Matrix, 3> NeckCorrections(const LionNeck& neck, const NeckTurns& turns) {
    std::array<Matrix, 3> corrections;
    Matrix cumulative = Matrix::getIdentityProperty();
    for (std::size_t i = 0; i < turns.size(); ++i) {
        const Vector3 pivot = Vector3::Transform(neck.joints[i], cumulative);
        cumulative = cumulative * Matrix::CreateTranslation(-pivot) * Matrix::CreateFromQuaternion(turns[i]) *
                     Matrix::CreateTranslation(pivot);
        corrections[i] = cumulative;
    }
    return corrections;
}

}  // namespace zoogame
