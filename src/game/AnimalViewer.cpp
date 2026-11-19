// SPDX-License-Identifier: MIT
//
// The animal viewer (zoo-simulator --viewer): a ground plane and one generated
// animal playing one clip. For checking models and animations on their own.
#include "AnimalViewer.hpp"

#include "AnimalTextures.hpp"
#include "SunShadows.hpp"

#include "CnaEngine/Animation/Animator.hpp"
#include "CnaEngine/App/AssetDirectory.hpp"
#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Enhanced/ModelImport.hpp"
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Enhanced/ShadowReception.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

using namespace CnaEngine;
using Microsoft::Xna::Framework::Matrix;
using Microsoft::Xna::Framework::Vector3;
namespace Xna = Microsoft::Xna::Framework::Graphics;

namespace {

class Probe final : public App::EngineGame {
public:
    Probe() : EngineGame(MakeOptions()) {
#if defined(__linux__) && !defined(__ANDROID__)
        getGraphicsDeviceProperty().SetContextRecoveryEnabled(false);
#endif
    }

protected:
    static App::GameOptions MakeOptions() {
        App::GameOptions options;
        options.title = "Zoo Simulator";
        options.application = "zoo-simulator";
        return options;
    }

    bool ParseArgument(std::span<const std::string_view> arguments, std::size_t& index) override {
        if (arguments[index] == "--animal" && index + 1 < arguments.size()) {
            animal_ = std::string(arguments[++index]);
            return true;
        }
        if (arguments[index] == "--clip" && index + 1 < arguments.size()) {
            clip_ = std::string(arguments[++index]);
            return true;
        }
        // Where the camera stands: degrees round the animal (0 in front of it, 90 on its
        // left), metres from it, metres above the ground; and the moment of the clip to hold.
        for (const auto& [name, value] : {std::pair{"--angle", &angle_},
                                          std::pair{"--distance", &distance_},
                                          std::pair{"--eye", &eye_},
                                          std::pair{"--at", &at_}}) {
            if (arguments[index] == name && index + 1 < arguments.size()) {
                *value = std::stof(std::string(arguments[++index]));
                return true;
            }
        }
        return false;
    }

    void LoadGame() override {
        auto& device = getGraphicsDeviceProperty();
        Renderer().SetMaterialBinder(
            std::make_shared<Enhanced::PbrMaterialBinder>(device, Renderer().StockBinder()));
        auto& lighting = Renderer().Lighting();
        lighting.defaultLighting = false;
        lighting.lightDirection = Vector3(-0.4f, -0.8f, -0.45f);
        lighting.lightColor = Vector3(2.5f, 2.4f, 2.2f);
        lighting.ambientColor = Vector3(0.35f, 0.38f, 0.45f);

#if defined(__linux__) && !defined(__ANDROID__)
        device.SetContextRecoveryEnabled(true);
#endif
        try {
            model_ = std::make_unique<Xna::Model>(getContentProperty().Load<Xna::Model>("animals/" + animal_));
        } catch (...) {
#if defined(__linux__) && !defined(__ANDROID__)
            device.SetContextRecoveryEnabled(false);
#endif
            throw;
        }
        view_ = std::make_unique<Graphics::ModelView>(*model_, Enhanced::PbrMaterialFromEffect);
#if defined(__linux__) && !defined(__ANDROID__)
        device.SetContextRecoveryEnabled(false);
#endif
        textures_ = zoogame::GiveMipmaps(*view_);
#if defined(__linux__) && !defined(__ANDROID__)
        // Automated screenshots are read through a CPU shadow in CNA.
        device.SetContextRecoveryEnabled(true);
#endif
        std::printf("zoo: viewer: mip levels ready, %zu separate filtering variant(s)\n", textures_.size());
        for (const Graphics::ModelPart& part : view_->Parts()) {
            const Graphics::Material& material = part.material.material;
            std::printf("zoo: viewer: part %s: base %s, normal map %s, occlusion/roughness/metal %s, tangents %s\n",
                        part.meshName.c_str(), material.baseTexture != nullptr ? "yes" : "no",
                        material.normalTexture != nullptr ? "yes" : "no",
                        material.occlusionRoughnessMetallicTexture != nullptr ? "yes" : "no",
                        part.mesh.Attributes().Has(Graphics::VertexAttribute::Tangent) ? "yes" : "no");
        }
        if (const auto* skin = Enhanced::FindSkinningData(*model_)) {
            skeleton_ = Enhanced::SkeletonFromSkinningData(*skin);
            clips_ = Enhanced::ClipsFromSkinningData(*skin);
            animator_ = std::make_unique<Animation::Animator>(skeleton_);
            std::printf("probe: %zu bones, %zu clips\n", skeleton_.Count(), clips_.size());
            for (const auto& clip : clips_) {
                std::printf("probe: clip %s %.2fs\n", clip.name.c_str(), static_cast<double>(clip.duration));
                if (clip.name == clip_) {
                    animator_->Play(clip, {.fadeSeconds = 0.0f});
                    if (at_ >= 0.0f) {
                        animator_->SetTime(std::min(at_, clip.duration));
                        animator_->Update(0.0f);
                    }
                }
            }
        } else {
            std::printf("probe: no skin\n");
        }

        Graphics::MeshBuilder builder;
        builder.AddGrid(Vector3::Zero, 40.0f, 40.0f, 20, 20, 20.0f);
        ground_ = Graphics::GpuMesh::Upload(device, builder.Take(),
                                            Graphics::VertexLayout::PositionNormalTexture());
        groundMaterial_.shading = Graphics::ShadingModel::PhysicallyBased;
        groundMaterial_.diffuseColor = Vector3(0.3f, 0.45f, 0.28f);
        groundMaterial_.roughness = 0.9f;
        (void)zoogame::EnableSunShadows(*this, {.size = 2048, .radius = 12.0f});

        // The whole animal in the picture, from where a visitor's eyes are: the generator's
        // sidecar says how big it is. The model looks along +z.
        float tall = 1.2f;
        float body = 0.0f;
        if (const auto directory = App::FindAssetDirectory("assets")) {
            try {
                std::ifstream file(*directory / "animals" / (animal_ + ".json"));
                const auto meta = nlohmann::json::parse(file).at("meta");
                tall = meta.value("shoulder_height_m", tall);
                body = meta.value("body_length_m", body);
            } catch (const std::exception&) {
                // no sidecar: the figures of a lion-sized animal
            }
        }
        const float size = std::max(tall * 1.25f, body > 0.0f ? body : 2.1f * tall);
        const float distance = distance_ > 0.0f ? distance_ : std::max(1.6f, 1.15f * size);
        const float angle = angle_ * 3.14159265f / 180.0f;
        MainCamera().SetPosition(Vector3(std::sin(angle) * distance, eye_, std::cos(angle) * distance));
        MainCamera().LookAt(Vector3(0.0f, std::min(0.62f * tall, eye_ + 0.4f * distance), 0.0f));
    }

    void UpdateFrame(const double seconds) override {
        // What the renderer could not draw as the material asks, said once.
        if (!fallbacksSaid_ && ++frames_ > 2) {
            fallbacksSaid_ = true;
            for (const std::string& fallback : Renderer().LastFallbacks())
                std::printf("zoo: viewer: fallback: %s\n", fallback.c_str());
        }
        if (animator_ && at_ < 0.0f) {
            animator_->Update(static_cast<float>(seconds));
        }
    }

    void Submit(App::Frame& frame) override {
        frame.Add(ground_, groundMaterial_, Matrix::getIdentityProperty());
        frame.Add(*view_, Matrix::getIdentityProperty(), animator_ ? &animator_->SkinPalette() : nullptr);
    }

private:
    std::string animal_ = "lion";
    std::string clip_ = "walk";
    float angle_ = 40.0f;
    float distance_ = 0.0f;
    float eye_ = 1.6f;
    float at_ = -1.0f;
    int frames_ = 0;
    bool fallbacksSaid_ = false;
    std::unique_ptr<Xna::Model> model_;
    std::unique_ptr<Graphics::ModelView> view_;
    std::vector<std::unique_ptr<Xna::Texture2D>> textures_;
    Animation::Skeleton skeleton_;
    std::vector<Animation::AnimationClip> clips_;
    std::unique_ptr<Animation::Animator> animator_;
    Graphics::GpuMesh ground_;
    Graphics::Material groundMaterial_;
};

}  // namespace

int zoogame::RunAnimalViewer(int argc, char** argv) {
    return App::RunGame<Probe>(argc, argv);
}
