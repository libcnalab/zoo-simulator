// SPDX-License-Identifier: MIT
#include "SunShadows.hpp"

#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Enhanced/ShadowReception.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/ShadowMap.hpp"

namespace zoogame {

namespace Enhanced = CnaEngine::Enhanced;
namespace Graphics = CnaEngine::Graphics;

bool EnableSunShadows(CnaEngine::App::EngineGame& game, const CnaEngine::App::ShadowSettings settings) {
    if (!game.getGraphicsDeviceProperty().SupportsShadowSamplingEXT())
        return false;
    Graphics::ForwardRenderer* const renderer = &game.Renderer();
    game.EnableShadowMap(settings, [renderer](Graphics::ShadowMap& map) {
        Enhanced::ApplyShadowReception(*renderer, &map, {});
        // Skinned models read their own caster depth wrongly (see the header): no reception.
        renderer->StockBinder()->SkinnedLitEffect().setShadowsEnabledEXT(false);
        if (auto* pbr = dynamic_cast<Enhanced::PbrMaterialBinder*>(&renderer->MaterialBinder()))
            pbr->SkinnedEffect().setShadowsEnabledEXT(false);
    });
    Enhanced::CalibrateShadowMap(game.getGraphicsDeviceProperty(), *game.Shadows());
    return true;
}

}  // namespace zoogame
