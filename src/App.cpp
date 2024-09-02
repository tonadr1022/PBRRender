#include "App.hpp"

#include <SDL_events.h>
#include <SDL_timer.h>
#include <imgui.h>

#include <glm/ext/matrix_transform.hpp>

#include "Input.hpp"
#include "MeshLoader.hpp"
#include "Path.hpp"
#include "Renderer.hpp"
#include "ResourceManager.hpp"
#include "Window.hpp"
#include "gl/ShaderManager.hpp"
#include "types.hpp"

App::App()
    : window_(1600, 900, "PBR Render", [this](SDL_Event& event) { OnEvent(event); }),
      player_(window_) {}

namespace {

LightsInfo lights_info{.directional_dir = glm::vec3{0, -1, 0}, .directional_color = glm::vec3(1)};

}  // namespace

void App::Run() {
  player_.SetPosition({-10, 15, -10});
  player_.LookAt({0, 0, 0});
  std::string err;
  std::string warn;
  std::string path = "/home/tony/damaged_helmet.glb";
  gl::ShaderManager::Init();
  gl::ShaderManager::Get().AddShader(
      "textured", {{GET_SHADER_PATH("textured.vs.glsl"), gl::ShaderType::kVertex, {}},
                   {GET_SHADER_PATH("textured.fs.glsl"), gl::ShaderType::kFragment, {}}});
  Renderer renderer;
  ResourceManager resource_manager{renderer};
  renderer.Init();

  auto helmet_handle = resource_manager.Load<Model>(path);
  Model& helmet = *resource_manager.Get<Model>(helmet_handle);
  // auto plane_handle = resource_manager.Load<Model>(
  //     "/home/tony/dep/models/glTF-Sample-Assets/Models/TwoSidedPlane/glTF/TwoSidedPlane.gltf");
  // Model& plane = *resource_manager.Get<Model>(plane_handle);
  auto sponza_handle = resource_manager.Load<Model>("/home/tony/toycar.glb");
  // "/home/tony/dep/models/glTF-Sample-Assets/Models/ToyCar/glTF/ToyCar.gltf");
  // auto sponza_handle = resource_manager.Load<Model>("/home/tony/sponza.glb");
  Model& sponza = *resource_manager.Get<Model>(sponza_handle);

  // auto submit_instanced = [&](int z, const Model& model) {
  //   glm::vec3 iter{0, 0, z};
  //   std::vector<glm::mat4> model_matrices;
  //   constexpr int kHalfLen = 10;
  //   for (iter.x = -kHalfLen; iter.x <= kHalfLen; iter.x += 4) {
  //     for (iter.y = -kHalfLen; iter.y <= kHalfLen; iter.y += 4) {
  //       model_matrices.emplace_back(glm::translate(glm::mat4(1), iter));
  //     }
  //   }
  //   renderer.SubmitStaticInstancedModel(model, model_matrices);
  // };

  // renderer.SubmitStaticModel(helmet, glm::translate(glm::mat4(1), glm::vec3(0, 0, 0)));
  // renderer.SubmitStaticModel(plane, glm::translate(glm::mat4(1), glm::vec3(0, 0, 2)));
  renderer.SubmitStaticModel(sponza, glm::scale(glm::mat4(1), glm::vec3(0.1)));

  // submit_instanced(5, plane);

  uint64_t curr_time = SDL_GetPerformanceCounter();
  uint64_t prev_time = 0;
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
    RenderInfo render_info{
        .view_matrix = player_.GetCamera().GetView(),
        .projection_matrix = player_.GetCamera().GetProjection(window_.GetAspectRatio(), 75),
        .view_pos = player_.Position()};

    auto shader = gl::ShaderManager::Get().GetShader("textured").value();
    shader.Bind();
    // shader.SetVec3("u_directional_dir", lights_info.directional_dir);
    // shader.SetVec3("u_directional_color", lights_info.directional_color);
    renderer.DrawStaticOpaque(render_info);

    if (imgui_enabled_) {
      OnImGui();
      player_.OnImGui();
    }

    window_.EndRenderFrame(imgui_enabled_);
  }

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
  ImGui::ColorEdit3("Directional Color", &lights_info.directional_color.x);
  ImGui::SliderFloat3("Directional Direction", &lights_info.directional_dir.x, -1, 1);
  ImGui::End();
}
