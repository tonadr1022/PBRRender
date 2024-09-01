#include "App.hpp"

#include <SDL_events.h>
#include <imgui.h>

#include "Input.hpp"
#include "MeshLoader.hpp"
#include "Renderer.hpp"
#include "Window.hpp"

App::App() : window_(1600, 900, "PBR Render", [this](SDL_Event& event) { OnEvent(event); }) {}

void App::Run() {
  std::string err;
  std::string warn;
  std::string path =
      "/home/tony/dep/models/glTF-Sample-Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf";
  std::string name = "DamagedHelmet.gltf";
  Renderer renderer;
  renderer.Init();
  LoadModel(renderer, path);

  while (!window_.ShouldClose()) {
    window_.PollEvents();
    window_.StartRenderFrame(imgui_enabled_);
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    OnImGui();

    window_.EndRenderFrame(imgui_enabled_);
  }
}

void App::OnEvent(SDL_Event& event) {
  if (event.type == SDL_KEYDOWN) {
    if (event.key.keysym.sym == SDLK_g && event.key.keysym.mod & KMOD_ALT) {
      imgui_enabled_ = !imgui_enabled_;
      return;
    }
  }

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
  if (!imgui_enabled_) return;
  ImGui::Begin("Test");
  ImGui::Text("hellO");
  ImGui::End();
}
