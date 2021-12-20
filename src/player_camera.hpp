#pragma once

// For SDL, since it's dumb and uses memcpy without import
#include <string>

#include "vk_types.hpp"
#include <SDL_events.h>
#include <glm/glm.hpp>

struct PlayerCamera {
  glm::vec3 position;
  glm::vec3 velocity;
  glm::vec3 inputAxis;

  float pitch{0};
  float yaw{0};

  bool bSprint = false;
  bool bLocked = false;

  void process_input_event(SDL_Event *ev);
  void update_camera(float deltaSeconds);

  auto get_view_matrix() -> glm::mat4;
  auto get_projection_matrix() -> glm::mat4;
  auto get_rotation_matrix() -> glm::mat4;
};