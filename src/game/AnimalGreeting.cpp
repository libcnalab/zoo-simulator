// SPDX-License-Identifier: MIT
#include "AnimalRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace zoogame {

LionNeck AnimalRenderer::Actor::NeckAt(const Vector3& bindPoint) const {
    const GreetingRig& rig = *kind->greeting;
    LionNeck neck;
    for (std::size_t i = 0; i < rig.joints.size(); ++i)
        neck.joints[i] =
            Vector3::Transform(animator.BoneTransform(rig.joints[i]).getTranslationProperty(), world);
    const Matrix& head = animator.SkinPalette()[static_cast<std::size_t>(rig.joints.back())];
    neck.landmark = Vector3::Transform(bindPoint, head * world);
    return neck;
}

void AnimalRenderer::Actor::Greet(zoo::AnimalId partner, const NeckTurns& wanted) {
    if (!greeting)
        greeting = std::make_unique<GreetingPose>();
    greeting->partner = partner;
    greeting->wanted = wanted;
    greeting->paired = true;
}

void AnimalRenderer::Actor::ApplyGreeting(float seconds) {
    if (!greeting)
        return;
    const float blend = 1.0f - std::exp(-8.0f * seconds);
    bool settled = true;
    for (std::size_t i = 0; i < greeting->turns.size(); ++i) {
        using Microsoft::Xna::Framework::Quaternion;
        auto& turn = greeting->turns[i];
        turn = Quaternion::Slerp(turn, greeting->paired ? greeting->wanted[i] : Quaternion::Identity, blend);
        settled = settled && std::abs(turn.W) > 0.99999f;
    }
    if (!greeting->paired && settled) {
        greeting.reset();
        return;
    }
    const GreetingRig& rig = *kind->greeting;
    const LionNeck neck = NeckAt(rig.muzzle);
    auto corrections = NeckCorrections(neck, greeting->turns);
    greeting->head = Vector3::Transform(neck.joints.back(), corrections.back());
    const Matrix localFromWorld = Matrix::Invert(world);
    for (auto& correction : corrections)
        correction = world * correction * localFromWorld;
    greeting->skin = animator.SkinPalette();
    for (std::size_t i = 0; i < greeting->skin.size(); ++i) {
        const int level = rig.neckLevel[i];
        if (level >= 0)
            greeting->skin[i] = greeting->skin[i] * corrections[static_cast<std::size_t>(level)];
    }
}

void AnimalRenderer::UpdateGreetings(const zoo::Zoo& zoo, float seconds) {
    for (auto& [id, actor] : actors_)
        if (actor->greeting)
            actor->greeting->paired = false;

    const int lion = species_.IndexOf("lion");
    const auto& animals = zoo.Fauna().All();
    for (const zoo::Animal& animal : animals) {
        if (animal.species != lion || animal.activity != zoo::AnimalActivity::Socialize || !animal.home ||
            !animal.Visible() || animal.moving || animal.loose || animal.sick)
            continue;
        const auto actor = actors_.find(animal.id);
        if (actor == actors_.end())
            continue;
        Actor& first = *actor->second;
        if (first.pose != AnimalPose::Nuzzle || !first.kind->greeting ||
            (first.greeting && first.greeting->paired) || first.animator.Clip()->duration <= 0.0f)
            continue;
        const LionNeck muzzle = first.NeckAt(first.kind->greeting->muzzle);
        const float phase = first.animator.Time() / first.animator.Clip()->duration;
        // The nuzzling clip sweeps along the cheek and back. The second animal responds
        // with its own neck; both bodies keep the legs and gait of their current clips.
        const float sweep = 0.125f * (1.0f + std::sin(2.0f * std::numbers::pi_v<float> * phase));
        float best = std::numeric_limits<float>::infinity();
        Actor* receiver = nullptr;
        zoo::AnimalId receiverId;
        std::optional<LionGreeting> fit;
        for (const zoo::Animal& other : animals) {
            if (other.id == animal.id || other.species != lion || other.home != animal.home ||
                !other.Visible() || other.moving || other.loose || other.sick ||
                (other.activity != zoo::AnimalActivity::Rest && other.activity != zoo::AnimalActivity::Idle &&
                 other.activity != zoo::AnimalActivity::Socialize))
                continue;
            const auto at = actors_.find(other.id);
            if (at == actors_.end())
                continue;
            Actor& mate = *at->second;
            if (!mate.kind->greeting || (mate.greeting && mate.greeting->paired) ||
                (mate.pose != AnimalPose::Rest && mate.pose != AnimalPose::Idle &&
                 mate.pose != AnimalPose::Nuzzle))
                continue;
            const float reach = first.bounds + mate.bounds;
            if (zoo::DistanceSquared(first.shown, mate.shown) > reach * reach)
                continue;
            const bool previous = first.greeting && first.greeting->partner == other.id;
            const auto& rig = *mate.kind->greeting;
            for (const auto& mark : rig.cheeks) {
                const LionNeck cheek = mate.NeckAt(Vector3::Lerp(mark, rig.muzzle, sweep));
                const float distance = Vector3::DistanceSquared(muzzle.landmark, cheek.landmark);
                if (distance >= best && (!previous || receiver == &mate))
                    continue;
                if (auto pair = FitLionGreeting(muzzle, cheek, std::min(first.standing, mate.standing))) {
                    best = distance;
                    receiver = &mate;
                    receiverId = other.id;
                    fit = pair;
                }
            }
            // Keep a reachable partner throughout the greeting, even if a third lion approaches.
            if (receiver == &mate && previous)
                break;
        }
        if (fit) {
            first.Greet(receiverId, fit->initiator);
            receiver->Greet(animal.id, fit->receiver);
        }
    }
    for (auto& [id, actor] : actors_)
        actor->ApplyGreeting(seconds);
}

}  // namespace zoogame
