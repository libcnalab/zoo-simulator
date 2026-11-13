// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/App/EngineGame.hpp"

namespace zoogame {

/// The sun's shadow map for a game, as Enhanced::EnableShadows sets it up, except that
/// skinned models (animals) do not receive it.
///
/// Why: cna-engine's ShadowMap draws skinned casters with a stock SkinnedEffect that has no
/// texture, and CNA reads an unbound texture as black (XNA's behaviour), so a skinned caster's
/// depth is stored as if it stood far nearer the sun than it does. The shadow it throws on the
/// ground is still right, but the model itself, and anything between it and the sun, reads
/// as shadowed: every animal was drawn in its own shadow, flat and dull, and its normal map
/// (which only shapes direct light) showed nothing. Measured in the viewer on 2026-11-13
/// (docs/UPSTREAM.md, ZOO-UP-001). Until cna-engine is corrected, animals take the sun
/// unshadowed: they are no longer dimmed under a tree, which is the smaller error.
///
/// Returns false on a device that cannot sample shadow maps (no map then).
bool EnableSunShadows(CnaEngine::App::EngineGame& game, CnaEngine::App::ShadowSettings settings);

}  // namespace zoogame
