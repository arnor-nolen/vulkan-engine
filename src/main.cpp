#include "vk_engine.hpp"

auto main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) -> int {
  VulkanEngine engine;

  engine.init();

  engine.run();

  engine.cleanup();

  return 0;
}
