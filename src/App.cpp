#include "App.hpp"

#include <SDL_events.h>
#include <SDL_timer.h>
#include <imgui.h>

#include <glm/ext/matrix_transform.hpp>
#include <random>

#include "CubeMapConverter.hpp"
#include "Input.hpp"
#include "MeshLoader.hpp"
#include "Path.hpp"
#include "Player.hpp"
#include "Renderer.hpp"
#include "ResourceManager.hpp"
#include "Window.hpp"
#include "gl/ShaderManager.hpp"
#include "gl/Texture.hpp"
#include "types.hpp"
#include "util/ThreadPool.hpp"

struct RandomGen {
  RandomGen(int min, int max) : random_engine(r()), uniform_dist(min, max) {}
  std::random_device r;
  std::default_random_engine random_engine;
  std::uniform_real_distribution<float> uniform_dist;

  float Get() { return uniform_dist(random_engine); }
};

App::App()
    : window_(1600, 900, "PBR Render", [this](SDL_Event& event) { OnEvent(event); }),
      resource_manager_(renderer_),
      player_(window_) {}

namespace {

bool point_lights_enabled{true};
bool directional_light_enabled{true};
Model* active_model{};
AssetHandle model_handle{};
LightsInfo lights_info{.directional_dir = glm::vec3{0, -1, 0}, .directional_color = glm::vec3(1)};
int cam_index = -1;
ImGui::FileBrowser file_dialog;
std::vector<PointLight> point_lights;

}  // namespace

void App::OnModelChange(const std::string& model) {
  if (model_handle) {
    resource_manager_.Free<Model>(model_handle);
  }
  renderer_.ResetStaticDrawCommands();
  model_handle = resource_manager_.Load<Model>(model, window_.GetAspectRatio());
  active_model = resource_manager_.Get<Model>(model_handle);
  renderer_.SubmitStaticModel(*active_model, glm::mat4(1));
  if (!active_model->camera_data.empty()) {
    cam_index = 0;
  }
}

void App::Run() {
  ThreadPool::Init();
  gl::ShaderManager::Init();
  gl::ShaderManager::Get().AddShader(
      "textured", {{GET_SHADER_PATH("textured.vs.glsl"), gl::ShaderType::kVertex, {}},
                   {GET_SHADER_PATH("textured.fs.glsl"), gl::ShaderType::kFragment, {}}});
  renderer_.Init();

  CubeMapConverter cube_map_converter;
  cube_map_converter.Init();
  const char* k_hdr_img_path = GET_PATH("resources/textures/hdr/newport_loft.hdr");
  AssetHandle hdr_equirect_handle = resource_manager_.Load<gl::Texture>(
      k_hdr_img_path, gl::Tex2DCreateInfoLoadImage{.path = k_hdr_img_path,
                                                   .wrap_s = GL_CLAMP_TO_EDGE,
                                                   .wrap_t = GL_CLAMP_TO_EDGE,
                                                   .internal_format = GL_RGB16F,
                                                   .format = GL_RGB,
                                                   .type = GL_FLOAT,
                                                   .min_filter = GL_LINEAR,
                                                   .mag_filter = GL_LINEAR,
                                                   .bindless = false,
                                                   .gen_mipmaps = false,
                                                   .flip_image = true});
  cube_map_converter.RenderEquirectangularEnvMap(
      *resource_manager_.Get<gl::Texture>(hdr_equirect_handle));

  RandomGen g(-10, 10);
  for (int i = 0; i < 100; i++) {
    glm::vec3 color = {g.Get(), g.Get() / 2, g.Get()};
    point_lights.emplace_back(
        PointLight{.position = color, ._pad1 = 0, .color = glm::vec3{1}, .intensity = 0.1});
  }
  renderer_.SubmitPointLights(point_lights);

  file_dialog.SetTitle("Select GLTF Model");
  file_dialog.SetTypeFilters({".gltf", ".glb"});
  file_dialog.SetCurrentDirectory(std::filesystem::path("/home/tony/glTF-Sample-Assets/Models"));
  player_.SetPosition({-2, 1, 0});
  player_.LookAt({0, 0, 0});

  // OnModelChange("/home/tony/abeautifulgame.glb");
  OnModelChange("/home/tony/toycar.glb");
  // OnModelChange("/home/tony/glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf");

  uint64_t curr_time = SDL_GetPerformanceCounter();
  uint64_t prev_time = 0;
  double dt = 0;
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_FRAMEBUFFER_SRGB);
  while (!window_.ShouldClose()) {
    prev_time = curr_time;
    curr_time = SDL_GetPerformanceCounter();
    dt = ((curr_time - prev_time) / static_cast<double>(SDL_GetPerformanceFrequency()));

    static double sum = 0;
    static int frame_counter_count = 0;
    sum += dt;
    frame_counter_count++;
    if (frame_counter_count % 100 == 0) {
      window_.SetTitle("Frame Time:" + std::to_string(sum / frame_counter_count) +
                       ", FPS: " + std::to_string(frame_counter_count / sum));
      frame_counter_count = 0;
      sum = 0;
    }

    window_.PollEvents();
    window_.StartRenderFrame(imgui_enabled_);
    player_.Update(dt);
    RenderInfo render_info;
    if (cam_index != -1 && active_model != nullptr) {
      CameraData& cam = active_model->camera_data[cam_index];
      player_.camera_mode = Player::CameraMode::kFPS;
      player_.SetCameraState(CameraState::kLocked);
      render_info.view_matrix = cam.view_matrix;
      render_info.projection_matrix = cam.proj_matrix;
      render_info.view_pos = cam.view_pos;
    } else {
      render_info.view_matrix = player_.GetCamera().GetView();
      render_info.projection_matrix =
          player_.GetCamera().GetProjection(window_.GetAspectRatio(), 75);
      render_info.view_pos = player_.Position();
    }

    glDisable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glViewport(0, 0, window_.GetWindowSize().x, window_.GetWindowSize().y);

    glEnable(GL_DEPTH_TEST);
    // TODO: separate back face cull vs no cull
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    auto shader = gl::ShaderManager::Get().GetShader("textured").value();
    shader.Bind();
    shader.SetBool("point_lights_enabled", point_lights_enabled);
    shader.SetBool("directional_light_enabled", directional_light_enabled);
    shader.SetVec3("u_directional_dir", lights_info.directional_dir);
    shader.SetVec3("u_directional_color", lights_info.directional_color);
    renderer_.DrawStaticOpaque(render_info);
    cube_map_converter.irradiance_map.Bind(0);
    cube_map_converter.Draw();
    // cube_map_converter.DrawIrradiance();

    if (imgui_enabled_) {
      OnImGui();
      player_.OnImGui();
    }

    glDisable(GL_FRAMEBUFFER_SRGB);
    window_.EndRenderFrame(imgui_enabled_);
    glEnable(GL_FRAMEBUFFER_SRGB);
  }

  renderer_.Shutdown();
  gl::ShaderManager::Shutdown();
  resource_manager_.Shutdown();
  window_.Shutdown();
  ThreadPool::Get().thread_pool.wait();
  ThreadPool::Shutdown();
}

void App::OnEvent(SDL_Event& event) {
  if (event.type == SDL_KEYDOWN) {
    if (event.key.keysym.sym == SDLK_g && event.key.keysym.mod & KMOD_ALT) {
      imgui_enabled_ = !imgui_enabled_;
      return;
    }
    if (event.key.keysym.sym == SDLK_r && event.key.keysym.mod & KMOD_ALT) {
      gl::ShaderManager::Get().RecompileShaders();
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

void App::OnImGui() {
  ImGui::Begin("PBR Renderer");
  bool vsync = window_.GetVsync();
  if (ImGui::Checkbox("Vsync", &vsync)) {
    window_.SetVsync(vsync);
  }

  if (ImGui::CollapsingHeader("Point Lights")) {
    ImGui::Checkbox("Enabled", &point_lights_enabled);
    int i = 0;
    for (auto& light : point_lights) {
      ImGui::PushID(&light);
      if (ImGui::ColorEdit3("Color", &light.color.x)) {
        renderer_.EditPointLight(light, i);
      }
      if (ImGui::DragFloat3("Position", &light.position.x)) {
        renderer_.EditPointLight(light, i);
      }
      if (ImGui::SliderFloat("Intensity", &light.intensity, 0.1, 100)) {
        renderer_.EditPointLight(light, i);
      }
      ImGui::PopID();
      i++;
    }
  }
  if (ImGui::Button("Select Model glTF")) {
    file_dialog.Open();
  }

  file_dialog.Display();

  if (file_dialog.HasSelected()) {
    OnModelChange(file_dialog.GetSelected().string());
    file_dialog.ClearSelected();
  }

  if (ImGui::CollapsingHeader("Directional Light")) {
    ImGui::Checkbox("Enabled", &directional_light_enabled);
    ImGui::ColorEdit3("Directional Color", &lights_info.directional_color.x);
    ImGui::SliderFloat3("Directional Direction", &lights_info.directional_dir.x, -1, 1);
  }

  ImGui::Text("Cam Index: %i", cam_index);
  if (active_model) {
    size_t i = 0;
    for (auto& cam : active_model->camera_data) {
      ImGui::PushID(&cam);
      if (ImGui::Button("Camera")) {
        cam_index = i;
      }
      i++;
      ImGui::PopID();
    }
  }
  ImGui::End();
}
