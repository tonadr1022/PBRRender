#pragma once

#include <SDL_events.h>

using EventCallback = std::function<void(SDL_Event&)>;

class Window {
 public:
  Window(int width, int height, const char* title, const EventCallback& event_callback);
  void SetVsync(bool vsync);
  void Shutdown();
  void SetCursorVisible(bool cursor_visible);
  void CenterCursor();
  void SetUserPointer(void* ptr);
  void StartRenderFrame(bool imgui_enabled);
  void EndRenderFrame(bool imgui_enabled) const;
  void PollEvents();
  void SetMouseGrab(bool state);
  void SetTitle(std::string_view title);
  void SetFullScreen(bool fullscreen);
  [[nodiscard]] glm::ivec2 GetWindowSize() const;
  [[nodiscard]] glm::ivec2 GetMousePosition() const;
  [[nodiscard]] glm::ivec2 GetWindowCenter() const;
  static void DisableImGuiInputs();
  static void EnableImGuiInputs();

  [[nodiscard]] float GetAspectRatio() const;
  [[nodiscard]] bool GetVsync() const;
  [[nodiscard]] bool ShouldClose() const;

 private:
  EventCallback event_callback_;
  SDL_Window* window_{nullptr};
  uint32_t window_width_, window_height_;
  uint32_t framebuffer_width_;
  uint32_t framebuffer_height_;
  bool should_close_{false};
  bool vsync_on_{true};
};
