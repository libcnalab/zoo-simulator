// SPDX-License-Identifier: MIT
//
// The animal viewer (zoo-simulator --viewer): a ground plane and one generated
// animal playing one clip. For checking models and animations on their own.
#include "AnimalViewer.hpp"

#include "CnaEngine/Animation/Animator.hpp"
#include "CnaEngine/App/EngineGame.hpp"
#include "CnaEngine/Enhanced/ModelImport.hpp"
#include "CnaEngine/Enhanced/PbrMaterialBinder.hpp"
#include "CnaEngine/Enhanced/ShadowReception.hpp"
#include "CnaEngine/Graphics/CameraControllers.hpp"
#include "CnaEngine/Graphics/MeshBuilder.hpp"
#include "CnaEngine/Graphics/ModelView.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace CnaEngine;
using Microsoft::Xna::Framework::Matrix;
using Microsoft::Xna::Framework::Vector3;
namespace Xna = Microsoft::Xna::Framework::Graphics;

namespace {

class Probe final : public App::EngineGame {
public:
    Probe() : EngineGame(MakeOptions()) {}

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

        model_ = std::make_unique<Xna::Model>(getContentProperty().Load<Xna::Model>("animals/" + animal_));
        view_ = std::make_unique<Graphics::ModelView>(*model_, Enhanced::PbrMaterialFromEffect);
        if (const auto* skin = Enhanced::FindSkinningData(*model_)) {
            skeleton_ = Enhanced::SkeletonFromSkinningData(*skin);
            clips_ = Enhanced::ClipsFromSkinningData(*skin);
            animator_ = std::make_unique<Animation::Animator>(skeleton_);
            std::printf("probe: %zu bones, %zu clips\n", skeleton_.Count(), clips_.size());
            for (const auto& clip : clips_) {
                std::printf("probe: clip %s %.2fs\n", clip.name.c_str(), static_cast<double>(clip.duration));
                if (clip.name == clip_) {
                    animator_->Play(clip, {.fadeSeconds = 0.0f});
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
        Enhanced::EnableShadows(*this, {.size = 2048, .radius = 12.0f});

        MainCamera().SetPosition(Vector3(2.6f, 1.6f, 3.2f));
        MainCamera().LookAt(Vector3(0.0f, 0.8f, 0.0f));
    }

    void UpdateFrame(const double seconds) override {
        if (animator_) {
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
    std::unique_ptr<Xna::Model> model_;
    std::unique_ptr<Graphics::ModelView> view_;
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
