#include "vk_descriptors.hpp"
#include <algorithm>

namespace vkutil {

auto create_pool(VkDevice device,
                 const DescriptorAllocator::PoolSizes &poolSizes, int count,
                 VkDescriptorPoolCreateFlags flags) -> VkDescriptorPool {
  std::vector<VkDescriptorPoolSize> sizes;
  sizes.reserve(poolSizes.sizes.size());
  for (auto &&[desc, f] : poolSizes.sizes) {
    sizes.push_back({desc, std::uint32_t(f * count)});
  }
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.pNext = nullptr;

  pool_info.flags = flags;
  pool_info.maxSets = count;
  pool_info.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
  pool_info.pPoolSizes = sizes.data();

  VkDescriptorPool descriptorPool;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);

  return descriptorPool;
}

void DescriptorAllocator::reset_pools() {
  // Reset all used pools and add them to the free pools
  for (auto &&p : usedPools) {
    vkResetDescriptorPool(device, p, 0);
    freePools.push_back(p);
  }

  // Clear the used pools, since we've put them all in the free pools
  usedPools.clear();

  // Reset the current pool handle back to null
  currentPool = VK_NULL_HANDLE;
}

void DescriptorAllocator::init(VkDevice newDevice) { device = newDevice; }

void DescriptorAllocator::cleanup() {
  // Delete every pool held
  for (auto &&p : freePools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  for (auto &&p : usedPools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
}

auto DescriptorAllocator::allocate(VkDescriptorSet *set,
                                   VkDescriptorSetLayout layout) -> bool {
  // Initialize the currentPool handle if it's null
  if (currentPool == VK_NULL_HANDLE) {
    currentPool = grab_pool();
    usedPools.push_back(currentPool);
  }

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.pNext = nullptr;

  allocInfo.pSetLayouts = &layout;
  allocInfo.descriptorPool = currentPool;
  allocInfo.descriptorSetCount = 1;

  // Try to allocate the descriptor set
  VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
  bool needReallocate = false;

  switch (allocResult) {
  case VK_SUCCESS:
    return true;
  case VK_ERROR_FRAGMENTED_POOL:
  case VK_ERROR_OUT_OF_POOL_MEMORY:
    needReallocate = true;
    break;
  default:
    return false;
  }

  if (needReallocate) {
    // Allocate a new pool and retry
    currentPool = grab_pool();
    usedPools.push_back(currentPool);

    allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);

    // If it still fails then we have problems
    if (allocResult == VK_SUCCESS) {
      return true;
    }
  }
  return false;
}

auto DescriptorAllocator::grab_pool() -> VkDescriptorPool {
  if (freePools.size() > 0) {
    // There are reusable pools available
    // Grab pool from the back of the vector and remove it from there
    VkDescriptorPool pool = freePools.back();
    freePools.pop_back();
    return pool;
  }
  // No pools available, so create a new one
  return create_pool(device, descriptorSizes, 1000, 0);
}

void DescriptorLayoutCache::init(VkDevice newDevice) { device = newDevice; }

void DescriptorLayoutCache::cleanup() {
  // Delete every descriptor layout held
  for (auto &&[_, cache] : layoutCache) {
    vkDestroyDescriptorSetLayout(device, cache, nullptr);
  }
}

auto DescriptorLayoutCache::create_descriptor_layout(
    VkDescriptorSetLayoutCreateInfo *info) -> VkDescriptorSetLayout {
  DescriptorLayoutInfo layoutInfo;
  layoutInfo.bindings.reserve(info->bindingCount);
  bool isSorted = true;
  int lastBinding = -1;

  // Copy from the direct info struct into our own one
  for (int i = 0; i != info->bindingCount; ++i) {
    layoutInfo.bindings.push_back(info->pBindings[i]);

    // Check that the bindings are in strict increasing order
    if (info->pBindings[i].binding > lastBinding) {
      lastBinding = info->pBindings[i].binding;
    } else {
      isSorted = false;
    }
  }

  // Sort the bindings if they aren't in order
  if (!isSorted) {
    std::sort(
        layoutInfo.bindings.begin(), layoutInfo.bindings.end(),
        [](VkDescriptorSetLayoutBinding &a, VkDescriptorSetLayoutBinding &b) {
          return a.binding < b.binding;
        });
  }

  // Try to grab from the cache
  auto it = layoutCache.find(layoutInfo);
  if (it != layoutCache.end()) {
    return (*it).second;
  } else {
    // Create a new one (not found)
    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(device, info, nullptr, &layout);

    // Add to cache
    layoutCache[layoutInfo] = layout;
    return layout;
  }
}

auto DescriptorLayoutCache::DescriptorLayoutInfo::operator==(
    const DescriptorLayoutInfo &other) const -> bool {
  if (other.bindings.size() != bindings.size()) {
    return false;
  }
  // Compare each of the bindings is the same. Bindings are sorted so they will
  // match
  for (size_t i = 0; i != bindings.size(); ++i) {
    if (other.bindings[i].binding != bindings[i].binding) {
      return false;
    }
    if (other.bindings[i].descriptorType != bindings[i].descriptorType) {
      return false;
    }
    if (other.bindings[i].descriptorCount != bindings[i].descriptorCount) {
      return false;
    }
    if (other.bindings[i].stageFlags != bindings[i].stageFlags) {
      return false;
    }
  }
  return true;
}

auto DescriptorLayoutCache::DescriptorLayoutInfo::hash() const -> size_t {
  size_t result = std::hash<size_t>()(bindings.size());
  for (auto &&b : bindings) {
    // Pack the binding data into a single int64. Not fully correct but it's ok
    size_t binding_hash = b.binding | b.descriptorType << 8 |
                          b.descriptorCount << 16 << b.stageFlags << 24;

    // Shuffle the packed bidnign data and xor it with the main hash
    result ^= std::hash<size_t>()(binding_hash);
  }
  return result;
}

auto DescriptorBuilder::begin(DescriptorLayoutCache *layoutCache,
                              DescriptorAllocator *allocator)
    -> DescriptorBuilder {
  DescriptorBuilder builder;

  builder.cache = layoutCache;
  builder.alloc = allocator;
  return builder;
}

auto DescriptorBuilder::bind_buffer(std::uint32_t binding,
                                    VkDescriptorBufferInfo *bufferInfo,
                                    VkDescriptorType type,
                                    VkShaderStageFlags stageFlags)
    -> DescriptorBuilder & {
  // Crate the descriptor binding for the layout
  VkDescriptorSetLayoutBinding newBinding{};

  newBinding.descriptorCount = 1;
  newBinding.descriptorType = type;
  newBinding.pImmutableSamplers = nullptr;
  newBinding.stageFlags = stageFlags;
  newBinding.binding = binding;

  bindings.push_back(newBinding);

  // Create the descriptor write
  VkWriteDescriptorSet newWrite{};
  newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  newWrite.pNext = nullptr;

  newWrite.descriptorCount = 1;
  newWrite.descriptorType = type;
  newWrite.pBufferInfo = bufferInfo;
  newWrite.dstBinding = binding;

  writes.push_back(newWrite);
  return *this;
}

auto DescriptorBuilder::bind_image(std::uint32_t binding,
                                   VkDescriptorImageInfo *imageInfo,
                                   VkDescriptorType type,
                                   VkShaderStageFlags stageFlags)
    -> DescriptorBuilder & {
  VkDescriptorSetLayoutBinding newBinding{};

  newBinding.descriptorCount = 1;
  newBinding.descriptorType = type;
  newBinding.pImmutableSamplers = nullptr;
  newBinding.stageFlags = stageFlags;
  newBinding.binding = binding;

  bindings.push_back(newBinding);

  VkWriteDescriptorSet newWrite{};
  newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  newWrite.pNext = nullptr;

  newWrite.descriptorCount = 1;
  newWrite.descriptorType = type;
  newWrite.pImageInfo = imageInfo;
  newWrite.dstBinding = binding;

  writes.push_back(newWrite);
  return *this;
}

auto DescriptorBuilder::build(VkDescriptorSet &set,
                              VkDescriptorSetLayout &layout) -> bool {
  // Build layout first
  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.pNext = nullptr;

  layoutInfo.pBindings = bindings.data();
  layoutInfo.bindingCount = bindings.size();

  layout = cache->create_descriptor_layout(&layoutInfo);

  // Allocate descriptor
  bool success = alloc->allocate(&set, layout);
  if (!success) {
    return false;
  }

  // Write descriptor
  for (auto &&w : writes) {
    w.dstSet = set;
  }
  vkUpdateDescriptorSets(alloc->device, writes.size(), writes.data(), 0,
                         nullptr);
  return true;
}

auto DescriptorBuilder::build(VkDescriptorSet &set) -> bool {
  VkDescriptorSetLayout layout;
  return build(set, layout);
}

} // namespace vkutil