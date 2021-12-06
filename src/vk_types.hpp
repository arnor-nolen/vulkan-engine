#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

struct AllocatedBuffer {
  VkBuffer _buffer;
  VmaAllocation _allocation;
};

struct AllocatedImage {
  VkImage _image;
  VmaAllocation _allocation;
  VkImageView _defaultView;
  int mipLevels;
};