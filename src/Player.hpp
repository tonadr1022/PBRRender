#pragma once

#include <SDL_events.h>

#include "camera/FPSCamera.hpp"
#include "camera/OrbitCamera.hpp"

enum class CameraState { kLocked, kUnfocused, kFocused };
class Window;
class Player {
 public:
  explicit Player(Window& window);
  enum class CameraMode { kFPS, kOrbit };
  virtual void Update(double dt);
  virtual void OnImGui();
  virtual bool OnEvent(const SDL_Event& event);
  void SetCameraState(CameraState state);
  void SetPosition(const glm::vec3& pos);
  void SetMovementSpeed(float speed);
  [[nodiscard]] float GetMovementSpeed() const;
  [[nodiscard]] const glm::vec3& Position() const;
  Camera& GetCamera();
  FPSCamera& GetFPSCamera();
  OrbitCamera& GetOrbitCamera();
  CameraMode camera_mode{CameraMode::kFPS};
  void LookAt(const glm::vec3& pos);

 protected:
  CameraState camera_state_;
  Window& window_;
  glm::vec3 position_{0};
  FPSCamera fps_camera_;
  OrbitCamera orbit_camera_;

  float move_speed_{10.f};
  bool override_movement_{true};
};
