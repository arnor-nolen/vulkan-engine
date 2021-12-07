#pragma once

#include <array>
#include <unordered_map>
#include <vector>
#include <vk_types.hpp>

namespace vkutil {

class DescriptorAllocator {
public:
  struct PoolSizes {
    std::vector<std::pair<VkDescriptorType, float>> sizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5F},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.F},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.F},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.F},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.F},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.F},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.F},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.F},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.F},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.F},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5F}};
  };

  void reset_pools();
  void init(VkDevice newDevice);
  void cleanup();

  VkDevice device;

  auto allocate(VkDescriptorSet *set, VkDescriptorSetLayout layout) -> bool;

private:
  VkDescriptorPool currentPool{VK_NULL_HANDLE};
  PoolSizes descriptorSizes;
  std::vector<VkDescriptorPool> usedPools;
  std::vector<VkDescriptorPool> freePools;

  auto grab_pool() -> VkDescriptorPool;
};

class DescriptorLayoutCache {
public:
  void init(VkDevice newDevice);
  void cleanup();

  auto create_descriptor_layout(VkDescriptorSetLayoutCreateInfo *info)
      -> VkDescriptorSetLayout;

  struct DescriptorLayoutInfo {
    // Good idea to turn this into and inlined array
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    auto operator==(const DescriptorLayoutInfo &other) const -> bool;

    auto hash() const -> size_t;
  };

private:
  struct DescriptorLayoutHash {
    size_t operator()(const DescriptorLayoutInfo &k) const { return k.hash(); }
  };

  std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout,
                     DescriptorLayoutHash>
      layoutCache;
  VkDevice device;
};

class DescriptorBuilder {
public:
  static auto begin(DescriptorLayoutCache *layoutCache,
                    DescriptorAllocator *allocator) -> DescriptorBuilder;

  auto bind_buffer(std::uint32_t binding, VkDescriptorBufferInfo *bufferInfo,
                   VkDescriptorType type, VkShaderStageFlags stageFlags)
      -> DescriptorBuilder &;
  auto bind_image(std::uint32_t binding, VkDescriptorImageInfo *imageInfo,
                  VkDescriptorType type, VkShaderStageFlags stageFlags)
      -> DescriptorBuilder &;

  auto build(VkDescriptorSet &set, VkDescriptorSetLayout &layout) -> bool;
  auto build(VkDescriptorSet &set) -> bool;

private:
  std::vector<VkWriteDescriptorSet> writes;
  std::vector<VkDescriptorSetLayoutBinding> bindings;

  DescriptorLayoutCache *cache;
  DescriptorAllocator *alloc;
};

} // namespace vkutil