#include "App.hpp"

#include <SDL_events.h>
#include <SDL_timer.h>
#include <imgui.h>

#include "Input.hpp"
#include "MeshLoader.hpp"
#include "Renderer.hpp"
#include "ResourceManager.hpp"
#include "Window.hpp"
#include "gl/ShaderManager.hpp"

App::App()
    : window_(1600, 900, "PBR Render", [this](SDL_Event& event) { OnEvent(event); }),
      player_(window_) {}

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
  renderer.SubmitStaticModel(model, glm::mat4(1));

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
    glClear(GL_COLOR_BUFFER_BIT);
    RenderInfo ri{
        .view_matrix = player_.GetCamera().GetView(),
        .projection_matrix = player_.GetCamera().GetProjection(window_.GetAspectRatio(), 75)};
    renderer.Render(ri);

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
  ImGui::Text("hellO");
  ImGui::End();
}
