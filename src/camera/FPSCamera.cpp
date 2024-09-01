#include "FPSCamera.hpp"

#include <SDL2/SDL_mouse.h>
#include <imgui.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>

#include "Window.hpp"

glm::mat4 FPSCamera::GetProjection(float aspect_ratio, float fov_degrees) const {
  return glm::perspective(glm::radians(fov_degrees), aspect_ratio, near_plane_, far_plane_);
}

glm::mat4 FPSCamera::GetView() const { return glm::lookAt(pos_, pos_ + front_, kUpVector); }

void FPSCamera::SetPosition(const glm::vec3& pos) { pos_ = pos; }

void FPSCamera::Update(Window& window) {
  auto mouse_pos = window.GetMousePosition();
  auto window_center = window.GetWindowCenter();
  glm::vec2 cursor_offset = mouse_pos - window_center;
  if (first_mouse_) {
    first_mouse_ = false;
    window.CenterCursor();
    return;
  }
  window.CenterCursor();

  // TODO: better mouse sensitivity
  float mouse_multiplier = 0.1 * 1;
  yaw_ += cursor_offset.x * mouse_multiplier;
  pitch_ = glm::clamp(pitch_ - cursor_offset.y * mouse_multiplier, -89.0f, 89.0f);
  CalculateFront();
}

void FPSCamera::CalculateFront() {
  glm::vec3 front;
  front.x = glm::cos(glm::radians(yaw_)) * glm::cos(glm::radians(pitch_));
  front.y = glm::sin(glm::radians(pitch_));
  front.z = glm::sin(glm::radians(yaw_)) * glm::cos(glm::radians(pitch_));
  front_ = glm::normalize(front);
}

void FPSCamera::LookAt(const glm::vec3& pos) {
  front_ = glm::normalize(pos - pos_);
  yaw_ = glm::degrees(glm::atan(front_.z, front_.x));
  pitch_ = glm::degrees(glm::asin(front_.y));
  CalculateFront();
}

void FPSCamera::OnImGui() const {
  ImGui::Text("Yaw: %.1f, Pitch: %.1f", yaw_, pitch_);
  ImGui::Text("Position: %.1f, %.1f, %.1f", pos_.x, pos_.y, pos_.z);
  ImGui::Text("Front: %.2f, %.2f, %.2f", front_.x, front_.y, front_.z);
}

float FPSCamera::GetPitch() const { return pitch_; }
float FPSCamera::GetYaw() const { return yaw_; }

void FPSCamera::SetOrientation(float pitch, float yaw) {
  pitch_ = pitch;
  yaw_ = yaw;
  CalculateFront();
}
