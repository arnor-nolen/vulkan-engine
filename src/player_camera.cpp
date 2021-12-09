#include "player_camera.hpp"

#include <glm/gtx/transform.hpp>

void PlayerCamera::process_input_event(SDL_Event *ev) {
  if (ev->type == SDL_KEYDOWN) {
    switch (ev->key.keysym.sym) {
    case SDLK_UP:
    case SDLK_w:
      inputAxis.x -= 1.f;
      break;
    case SDLK_DOWN:
    case SDLK_s:
      inputAxis.x += 1.f;
      break;
    case SDLK_LEFT:
    case SDLK_a:
      inputAxis.y -= 1.f;
      break;
    case SDLK_RIGHT:
    case SDLK_d:
      inputAxis.y += 1.f;
      break;
    case SDLK_LCTRL:
      inputAxis.z -= 1.f;
      break;
    case SDLK_SPACE:
      inputAxis.z += 1.f;
      break;
    case SDLK_LSHIFT:
      bSprint = true;
      break;
    }
  } else if (ev->type == SDL_KEYUP) {
    switch (ev->key.keysym.sym) {
    case SDLK_UP:
    case SDLK_w:
      inputAxis.x += 1.f;
      break;
    case SDLK_DOWN:
    case SDLK_s:
      inputAxis.x -= 1.f;
      break;
    case SDLK_LEFT:
    case SDLK_a:
      inputAxis.y += 1.f;
      break;
    case SDLK_RIGHT:
    case SDLK_d:
      inputAxis.y -= 1.f;
      break;
    case SDLK_LCTRL:
      inputAxis.z += 1.f;
      break;
    case SDLK_SPACE:
      inputAxis.z -= 1.f;
      break;
    case SDLK_LSHIFT:
      bSprint = false;
      break;
    }
  } else if (ev->type == SDL_MOUSEMOTION) {
    if (!bLocked) {
      const float mouse_sensitivity = 0.001F;
      pitch += ev->motion.yrel * mouse_sensitivity;
      yaw += ev->motion.xrel * mouse_sensitivity;
    }
  }

  inputAxis = glm::clamp(inputAxis, {-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
}

void PlayerCamera::update_camera(float deltaSeconds) {
  const float cam_vel = 0.001F + bSprint * 0.01F;
  glm::vec3 forward = {0, 0, cam_vel};
  glm::vec3 right = {cam_vel, 0, 0};
  glm::vec3 up = {0, cam_vel, 0};

  glm::mat4 cam_rot = get_rotation_matrix();

  forward = cam_rot * glm::vec4(forward, 0.F);
  right = cam_rot * glm::vec4(right, 0.F);

  velocity = inputAxis.x * forward + inputAxis.y * right + inputAxis.z * up;

  velocity *= 10 * deltaSeconds;

  position += velocity;
}

auto PlayerCamera::get_view_matrix() -> glm::mat4 {
  glm::vec3 camPos = position;
  glm::mat4 cam_rot = get_rotation_matrix();
  glm::mat4 view = glm::translate(glm::mat4{1}, camPos) * cam_rot;

  // We need to invert camera matrix
  view = glm::inverse(view);

  return view;
}

auto PlayerCamera::get_projection_matrix() -> glm::mat4 {
  glm::mat4 pro =
      glm::perspective(glm::radians(70.F), 1700.F / 900.F, 0.1F, 5000.F);
  pro[1][1] *= -1;
  return pro;
}

auto PlayerCamera::get_rotation_matrix() -> glm::mat4 {
  glm::mat4 yaw_rot = glm::rotate(glm::mat4{1}, yaw, {0, -1, 0});
  glm::mat4 pitch_rot = glm::rotate(glm::mat4{yaw_rot}, pitch, {-1, 0, 0});
  return pitch_rot;
}
