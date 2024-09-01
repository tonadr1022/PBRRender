#include "App.hpp"

#include <SDL_events.h>
#include <SDL_timer.h>
#include <imgui.h>

#include <glm/ext/matrix_transform.hpp>

#include "Input.hpp"
#include "MeshLoader.hpp"
#include "Renderer.hpp"
#include "ResourceManager.hpp"
#include "Window.hpp"
#include "gl/ShaderManager.hpp"

App::App()
    : window_(1600, 900, "PBR Render", [this](SDL_Event& event) { OnEvent(event); }),
      player_(window_) {}

namespace {
void FreeModel(Renderer& renderer, ResourceManager& resource_manager, Model& model) {
  for (auto& mat : model.material_handles) {
    renderer.FreeMaterial(mat);
  }
  for (auto& tex : model.texture_handles) {
    resource_manager.FreeTexture(tex);
  }
  model.material_handles.clear();
  model.texture_handles.clear();
  for (auto& p : model.primitives) {
    renderer.FreeMesh(p.mesh_handle);
  }
  model.primitives.clear();
  model.texture_handles.clear();
  model.material_handles.clear();
}

}  // namespace
void App::Run() {
  std::string err;
  std::string warn;
  std::string path =
      "/home/tony/dep/models/glTF-Sample-Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf";
  std::string name = "DamagedHelmet.gltf";
  gl::ShaderManager::Init();
  ResourceManager resource_manager;
  Renderer renderer;
  renderer.Init();
  Model model = LoadModel(resource_manager, renderer, path);
  Model plane = LoadModel(
      resource_manager, renderer,
      "/home/tony/dep/models/glTF-Sample-Assets/Models/TwoSidedPlane/glTF/TwoSidedPlane.gltf");

  player_.SetPosition({0, 0, 3});
  auto submit_instanced = [&](int z) {
    glm::vec3 iter{0, 0, z};
    std::vector<glm::mat4> model_matrices;
    constexpr int kHalfLen = 10;
    for (iter.x = -kHalfLen; iter.x <= kHalfLen; iter.x += 4) {
      for (iter.y = -kHalfLen; iter.y <= kHalfLen; iter.y += 4) {
        model_matrices.emplace_back(glm::translate(glm::mat4(1), iter));
      }
    }
    renderer.SubmitStaticInstancedModel(model, model_matrices);
  };
  submit_instanced(0);
  submit_instanced(5);
  renderer.SubmitStaticModel(model, glm::translate(glm::mat4(1), glm::vec3(0, 0, 8)));
  renderer.SubmitStaticModel(plane, glm::translate(glm::mat4(1), glm::vec3(0, -4, 0)));

  Uint64 curr_time = SDL_GetPerformanceCounter();
  Uint64 prev_time = 0;
  double dt = 0;
  while (!window_.ShouldClose()) {
    prev_time = curr_time;
    curr_time = SDL_GetPerformanceCounter();
    dt = ((curr_time - prev_time) / static_cast<double>(SDL_GetPerformanceFrequency()));
    window_.PollEvents();
    window_.StartRenderFrame(imgui_enabled_);
    player_.Update(dt);

    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    RenderInfo ri{
        .view_matrix = player_.GetCamera().GetView(),
        .projection_matrix = player_.GetCamera().GetProjection(window_.GetAspectRatio(), 75)};
    renderer.DrawStaticOpaque(ri);

    if (imgui_enabled_) {
      OnImGui();
      player_.OnImGui();
    }

    window_.EndRenderFrame(imgui_enabled_);
  }
  FreeModel(renderer, resource_manager, model);

  gl::ShaderManager::Shutdown();
}

void App::OnEvent(SDL_Event& event) {
  if (event.type == SDL_KEYDOWN) {
    if (event.key.keysym.sym == SDLK_g && event.key.keysym.mod & KMOD_ALT) {
      imgui_enabled_ = !imgui_enabled_;
      return;
    }
  }

  player_.OnEvent(event);

  switch (event.type) {
    case SDL_KEYDOWN:
      Input::SetKeyPressed(event.key.keysym.sym, true);
      break;
    case SDL_KEYUP:
      Input::SetKeyPressed(event.key.keysym.sym, false);
      break;
    case SDL_MOUSEBUTTONDOWN:
      Input::SetMouseButtonPressed(event.button.button, true);
      break;
    case SDL_MOUSEBUTTONUP:
      Input::SetMouseButtonPressed(event.button.button, false);
      break;
  }
}

void App::OnImGui() const {
  ImGui::Begin("Test");
  ImGui::Text("hellO");
  ImGui::End();
}
