#pragma once

#include <imgui.h>
#include <imgui_filebrowser/imgui_filebrowser.h>

#include "Player.hpp"
#include "Renderer.hpp"
#include "ResourceManager.hpp"
#include "Window.hpp"

class App {
 public:
  App();
  void Run();
  void OnEvent(SDL_Event& event);

 private:
  Window window_;
  Renderer renderer_;
  ResourceManager resource_manager_;
  bool imgui_enabled_{true};
  Player player_;

  void OnImGui();
  void OnModelChange(const std::string& model);
};
