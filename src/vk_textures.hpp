#pragma once

#include "vk_engine.hpp"
#include "vk_types.hpp"
#include <filesystem>

namespace vkutil {
auto load_image_from_file(VulkanEngine &engine,
                          const std::filesystem::path &file,
                          AllocatedImage &outImage) -> bool;

auto load_image_from_asset(VulkanEngine &engine,
                           const std::filesystem::path &filename,
                           AllocatedImage &outImage) -> bool;

auto upload_image(int texWidth, int texHeight, VkFormat image_format,
                  VulkanEngine &engine, AllocatedBuffer &stagingBuffer)
    -> AllocatedImage;

} // namespace vkutil
