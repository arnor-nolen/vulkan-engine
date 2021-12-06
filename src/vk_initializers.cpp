#include "vk_initializers.hpp"
#include <cstdint>

// Create a command pool for commands submitted to the graphics queue
auto vkinit::command_pool_create_info(uint32_t queueFamilyIndex,
                                      VkCommandPoolCreateFlags flags)
    -> VkCommandPoolCreateInfo {
  VkCommandPoolCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.pNext = nullptr;

  info.queueFamilyIndex = queueFamilyIndex;
  info.flags = flags;

  return info;
}

// Allocate the command buffer that will be used for rendering
auto vkinit::command_buffer_allocate_info(VkCommandPool pool,
                                          std::uint32_t count,
                                          VkCommandBufferLevel level)
    -> VkCommandBufferAllocateInfo {
  VkCommandBufferAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.pNext = nullptr;

  info.commandPool = pool;
  info.commandBufferCount = count;
  info.level = level;

  return info;
}

auto vkinit::pipeline_shader_stage_create_info(VkShaderStageFlagBits stage,
                                               VkShaderModule shaderModule)
    -> VkPipelineShaderStageCreateInfo {
  VkPipelineShaderStageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.pNext = nullptr;

  // Shader stage
  info.stage = stage;
  // Module containing the code for this shdaer stage
  info.module = shaderModule;
  // The entry point of the shader
  info.pName = "main";
  return info;
}

auto vkinit::vertex_input_state_create_info()
    -> VkPipelineVertexInputStateCreateInfo {
  VkPipelineVertexInputStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  info.pNext = nullptr;

  // No vertex bindings or attributes
  info.vertexBindingDescriptionCount = 0;
  info.vertexAttributeDescriptionCount = 0;
  return info;
}

auto vkinit::input_assembly_create_info(VkPrimitiveTopology topology)
    -> VkPipelineInputAssemblyStateCreateInfo {
  VkPipelineInputAssemblyStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.topology = topology;
  // We are not going to use primitive restart on the entire tutorial so leave
  // it on false
  info.primitiveRestartEnable = VK_FALSE;
  return info;
}

auto vkinit::rasterization_state_create_info(VkPolygonMode polygonMode)
    -> VkPipelineRasterizationStateCreateInfo {
  VkPipelineRasterizationStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.depthClampEnable = VK_FALSE;
  // Discards all primitives before the rasterization stage if enabled which we
  // don't want
  info.rasterizerDiscardEnable = VK_FALSE;

  info.polygonMode = polygonMode;
  info.lineWidth = 1.0F;
  // No backface cull
  info.cullMode = VK_CULL_MODE_NONE;
  info.frontFace = VK_FRONT_FACE_CLOCKWISE;
  // No depth bias
  info.depthBiasEnable = VK_FALSE;
  info.depthBiasConstantFactor = 0.0F;
  info.depthBiasClamp = 0.0F;
  info.depthBiasSlopeFactor = 0.0F;

  return info;
}

auto vkinit::multisampling_state_create_info()
    -> VkPipelineMultisampleStateCreateInfo {
  VkPipelineMultisampleStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.sampleShadingEnable = VK_FALSE;
  // Multisampling defaulted to no multisampling (1 sample per pixel)
  info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  info.minSampleShading = 1.0F;
  info.pSampleMask = nullptr;
  info.alphaToCoverageEnable = VK_FALSE;
  info.alphaToOneEnable = VK_FALSE;

  return info;
}

auto vkinit::color_blend_attachment_state()
    -> VkPipelineColorBlendAttachmentState {
  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask =
      // NOLINTNEXTLINE(hicpp-signed-bitwise)
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  colorBlendAttachment.blendEnable = VK_FALSE;
  return colorBlendAttachment;
}

auto vkinit::pipeline_layout_create_info() -> VkPipelineLayoutCreateInfo {
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pNext = nullptr;

  // Empty defaults
  info.flags = 0;
  info.setLayoutCount = 0;
  info.pSetLayouts = nullptr;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges = nullptr;

  return info;
}

auto vkinit::image_create_info(VkFormat format, VkImageUsageFlags usageFlags,
                               VkExtent3D extent) -> VkImageCreateInfo {
  VkImageCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = nullptr;

  info.imageType = VK_IMAGE_TYPE_2D;

  info.format = format;
  info.extent = extent;

  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usageFlags;

  return info;
}

auto vkinit::imageview_create_info(VkFormat format, VkImage image,
                                   VkImageAspectFlags aspectFlags)
    -> VkImageViewCreateInfo {
  // Build and image-view for the depth image to use for rendering
  VkImageViewCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext = nullptr;

  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = image;
  info.format = format;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspectFlags;

  return info;
}

auto vkinit::depth_stencil_create_info(bool bDepthTest, bool bDepthWrite,
                                       VkCompareOp compareOp)
    -> VkPipelineDepthStencilStateCreateInfo {

  VkPipelineDepthStencilStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE;
  info.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE;
  info.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
  info.depthBoundsTestEnable = VK_FALSE;
  info.minDepthBounds = 0.F;
  info.maxDepthBounds = 1.F;
  info.stencilTestEnable = VK_FALSE;

  return info;
}

auto vkinit::fence_create_info(VkFenceCreateFlags flags) -> VkFenceCreateInfo {
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.pNext = nullptr;

  info.flags = flags;
  return info;
}

auto vkinit::semaphore_create_info(VkSemaphoreCreateFlags flags)
    -> VkSemaphoreCreateInfo {
  VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.pNext = nullptr;

  info.flags = flags;
  return info;
}

auto vkinit::renderpass_begin_info(VkRenderPass renderPass,
                                   VkExtent2D windowExtent,
                                   VkFramebuffer framebuffer)
    -> VkRenderPassBeginInfo {
  VkRenderPassBeginInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  info.pNext = nullptr;

  info.renderPass = renderPass;
  info.renderArea.offset.x = 0;
  info.renderArea.offset.y = 0;
  info.renderArea.extent = windowExtent;
  info.clearValueCount = 1;
  info.pClearValues = nullptr;
  info.framebuffer = framebuffer;

  return info;
}

auto vkinit::command_buffer_begin_info(VkCommandBufferUsageFlags flags)
    -> VkCommandBufferBeginInfo {
  VkCommandBufferBeginInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.pNext = nullptr;

  info.pInheritanceInfo = nullptr;
  info.flags = flags;
  return info;
}

auto vkinit::descriptorset_layout_binding(VkDescriptorType type,
                                          VkShaderStageFlags stageFlags,
                                          uint32_t binding)
    -> VkDescriptorSetLayoutBinding {
  VkDescriptorSetLayoutBinding setbind = {};
  setbind.binding = binding;
  setbind.descriptorCount = 1;
  setbind.descriptorType = type;
  setbind.pImmutableSamplers = nullptr;
  setbind.stageFlags = stageFlags;

  return setbind;
}

auto vkinit::write_descriptor_buffer(VkDescriptorType type,
                                     VkDescriptorSet dstSet,
                                     VkDescriptorBufferInfo *bufferInfo,
                                     uint32_t binding) -> VkWriteDescriptorSet {
  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = nullptr;

  write.dstBinding = binding;
  write.dstSet = dstSet;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pBufferInfo = bufferInfo;

  return write;
}

auto vkinit::submit_info(VkCommandBuffer *cmd) -> VkSubmitInfo {
  VkSubmitInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.pNext = nullptr;

  info.waitSemaphoreCount = 0;
  info.pWaitSemaphores = nullptr;
  info.pWaitDstStageMask = nullptr;
  info.commandBufferCount = 1;
  info.pCommandBuffers = cmd;
  info.signalSemaphoreCount = 0;
  info.pSignalSemaphores = nullptr;

  return info;
}

auto vkinit::sampler_create_info(VkFilter filters,
                                 VkSamplerAddressMode samplerAddressMode)
    -> VkSamplerCreateInfo {
  VkSamplerCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.pNext = nullptr;

  info.magFilter = filters;
  info.minFilter = filters;
  info.addressModeU = samplerAddressMode;
  info.addressModeV = samplerAddressMode;
  info.addressModeW = samplerAddressMode;

  return info;
}

auto vkinit::write_descriptor_image(VkDescriptorType type,
                                    VkDescriptorSet dstSet,
                                    VkDescriptorImageInfo *imageInfo,
                                    std::uint32_t binding)
    -> VkWriteDescriptorSet {
  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = nullptr;

  write.dstBinding = binding;
  write.dstSet = dstSet;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pImageInfo = imageInfo;

  return write;
}
