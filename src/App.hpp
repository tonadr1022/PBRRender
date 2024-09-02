#pragma once

#include "Player.hpp"
#include "Window.hpp"

class App {
 public:
  App();
  void Run();
  void OnEvent(SDL_Event& event);

 private:
  Window window_;
  bool imgui_enabled_{true};
  void OnImGui();
  Player player_;
};
