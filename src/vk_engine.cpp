#include "vk_engine.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/gtx/transform.hpp>

#include "vk_fonts.hpp"
#include "vk_initializers.hpp"
#include "vk_textures.hpp"
#include "vk_types.hpp"

#include "bindings/imgui_impl_sdl2.h"
#include "bindings/imgui_impl_vulkan.h"
#include <VkBootstrap.h>

#include "utils/timer.hpp"
#include <array>
#include <cstdint>
#include <fmt/core.h>
#include <fstream>
#include <numeric>

#include "./implementations/vma_implementation.hpp"

// We want to immediately abort when there is an error
constexpr void VK_CHECK(VkResult err) {
  if (err != 0) {
    utils::logger.dump(
        fmt::format("Detected Vulkan error: {}", std::to_string(err)),
        spdlog::level::err);
    abort();
  }
}

void VulkanEngine::init() {
  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);

  auto window_flags = static_cast<SDL_WindowFlags>(
      SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

  // NOLINTNEXTLINE(hicpp-signed-bitwise)
  _window = SDL_CreateWindow(
      // NOLINTNEXTLINE(hicpp-signed-bitwise)
      "Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
      // NOLINTNEXTLINE(hicpp-signed-bitwise)
      SDL_WINDOWPOS_UNDEFINED, static_cast<int>(_windowExtent.width),
      static_cast<int>(_windowExtent.height), window_flags);

  int width = 0;
  int height = 0;
  SDL_GetWindowSizeInPixels(_window, &width, &height);
  _windowExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

  // Trap mouse inside the window
  SDL_SetRelativeMouseMode(SDL_TRUE);

  // Initialization
  init_vulkan();
  init_swapchain();
  init_commands();
  init_default_renderpass();
  init_framebuffers();
  init_sync_structures();
  init_descriptors();
  init_pipelines();
  load_images();
  load_meshes();
  init_scene();
  init_imgui();

  // Everything went fine
  _isInitialized = true;

  _camera = {.position = {0.F, 10.F, 0.F},
             .velocity = {0.F, 0.F, 0.F},
             .inputAxis = {0.F, 0.F, 0.F}};
}

void VulkanEngine::init_vulkan() {
  vkb::InstanceBuilder builder;

  // Make the Vulkan instance, with basic debug features
  auto inst_ret = builder.set_app_name("Example Vulkan Application")
                      .request_validation_layers(true)
                      // Use 1.1 because MacOS doesn't yet have 1.2
                      .require_api_version(1, 1, 0)
                      .use_default_debug_messenger();

// For MacOS (SDL uses deprecated API, see
// https://github.com/libsdl-org/SDL/issues/3906)
#ifdef __APPLE__
  inst_ret = inst_ret.enable_extension("VK_MVK_macos_surface");
#endif

  vkb::Instance vkb_inst = inst_ret.build().value();

  _instance = vkb_inst.instance;
  _debug_messenger = vkb_inst.debug_messenger;

  // Get the surface of the window we opened with SDL
  SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

  // Use vkbootstrap to select a GPU.
  // We want a GPU that can write to the SDL surface and supports Vulkan 1.1
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::PhysicalDevice physicalDevice =
      selector
          // Use 1.1 because MacOS doesn't yet have 1.2
          .set_minimum_version(1, 1)
          .set_surface(_surface)
          .add_desired_extension("VK_KHR_portability_subset")
          .select()
          .value();

  // Used for gl_BaseInstance in shader code
  // VkPhysicalDeviceVulkan11Features vk11features = {};
  // vk11features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  // vk11features.pNext = nullptr;

  // vk11features.shaderDrawParameters = VK_TRUE;
  // Create the final Vulkan device
  vkb::DeviceBuilder deviceBuilder{physicalDevice};
  vkb::Device vkbDevice = deviceBuilder.build().value();

  // Get the VkDevice handle used in the rest of a Vulkan application
  _device = vkbDevice.device;
  _chosenGPU = physicalDevice.physical_device;

  // Use vkbootstrap to get a Graphics queue
  _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamily =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  // Initialize the memory allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _chosenGPU;
  allocatorInfo.device = _device;
  allocatorInfo.instance = _instance;
  vmaCreateAllocator(&allocatorInfo, &_allocator);

  vkGetPhysicalDeviceProperties(_chosenGPU, &_gpuProperties);
}

void VulkanEngine::init_imgui() {
  // 1: create descriptor pool for ImGui
  // The size of the pool is very oversize, but it's copied from ImGui demo
  // itself.
  std::array<VkDescriptorPoolSize, 11> pool_sizes = {{
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
  }};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
  pool_info.pPoolSizes = pool_sizes.data();

  VkDescriptorPool imguiPool;
  VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

  // 2: initialize ImGui library

  // This initializes the core structures of ImGui
  ImGui::CreateContext();

  // This initializes ImGui for SDL
  ImGui_ImplSDL2_InitForVulkan(_window);

  // This initializes ImGui for vulkan
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = _instance;
  init_info.PhysicalDevice = _chosenGPU;
  init_info.Device = _device;
  init_info.Queue = _graphicsQueue;
  init_info.DescriptorPool = imguiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.MSAASamples = _samples;

  ImGui_ImplVulkan_Init(&init_info, _renderPass);

  // Execute a GPU command to upload ImGui font textures
  immediate_submit(
      [&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

  // Clear font textures from CPU data
  ImGui_ImplVulkan_DestroyFontUploadObjects();

  // Add teh destroy to ImGui created structures
  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyDescriptorPool(_device, imguiPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
  });
}

void VulkanEngine::init_swapchain() {
  vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
  vkb::Swapchain vkbSwapchain =
      swapchainBuilder
          .use_default_format_selection()
          // VSync on
          // .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          // VSync off
          .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
          .set_desired_extent(_windowExtent.width, _windowExtent.height)
          .build()
          .value();

  // Store swapchain and its related images
  _swapchain = vkbSwapchain.swapchain;
  _swapchainImages = vkbSwapchain.get_images().value();
  _swapchainImageViews = vkbSwapchain.get_image_views().value();
  _swapchainImageFormat = vkbSwapchain.image_format;

  // color image size will match the window
  VkExtent3D colorImageExtent = {_windowExtent.width, _windowExtent.height, 1};

  // Hardcoding the color format to 32 bit float
  _colorFormat = VK_FORMAT_B8G8R8A8_SRGB;

  // The color image will be an image with the format we selected and color
  // Attachment usage flag
  auto cimg_info =
      vkinit::image_create_info(_colorFormat,
                                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                colorImageExtent, _samples);

  // For the color image we watn to allocate it from GPU local memory
  VmaAllocationCreateInfo cimg_allocinfo = {};
  cimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  cimg_allocinfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Allocate and create the image
  vmaCreateImage(_allocator, &cimg_info, &cimg_allocinfo, &_colorImage._image,
                 &_colorImage._allocation, nullptr);

  // Build and image-view for the color image to use for rendering
  auto cview_info = vkinit::imageview_create_info(
      _colorFormat, _colorImage._image, VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(vkCreateImageView(_device, &cview_info, nullptr, &_colorImageView));

  // Depth image size will match the window
  VkExtent3D depthImageExtent = {_windowExtent.width, _windowExtent.height, 1};

  // Hardcoding the depth format to 32 bit float
  _depthFormat = VK_FORMAT_D32_SFLOAT;

  // The depth image will be an image with the format we selected and Depth
  // Attachment usage flag
  auto dimg_info = vkinit::image_create_info(
      _depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      depthImageExtent, _samples);

  // For the depth image we watn to allocate it from GPU local memory
  VmaAllocationCreateInfo dimg_allocinfo = {};
  dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  dimg_allocinfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Allocate and create the image
  vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image,
                 &_depthImage._allocation, nullptr);

  // Build and image-view for the depth image to use for rendering
  auto dview_info = vkinit::imageview_create_info(
      _depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyImageView(_device, _depthImageView, nullptr);
    vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
    vkDestroyImageView(_device, _colorImageView, nullptr);
    vmaDestroyImage(_allocator, _colorImage._image, _colorImage._allocation);
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
  });
}

void VulkanEngine::init_commands() {
  // Create a command pool for commands submitted to the graphics queue
  auto commandPoolInfo = vkinit::command_pool_create_info(
      _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  auto uploadCommandPoolInfo =
      vkinit::command_pool_create_info(_graphicsQueueFamily);

  VK_CHECK(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr,
                               &_uploadContext._commandPool));
  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyCommandPool(_device, _uploadContext._commandPool, nullptr);
  });

  for (auto &&frame : _frames) {
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr,
                                 &frame._commandPool));
    // Allocate the default command buffer that we will use for rendering
    auto cmdAllocInfo =
        vkinit::command_buffer_allocate_info(frame._commandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo,
                                      &frame._mainCommandBuffer));

    _mainDeletionQueue.push_function([=, this]() {
      vkDestroyCommandPool(_device, frame._commandPool, nullptr);
    });
  }
}

void VulkanEngine::init_default_renderpass() {
  // The renderpass will use this color attachment
  VkAttachmentDescription color_attachment = {};
  // The attachment will have the format needed by the swapchain
  color_attachment.format = _swapchainImageFormat;
  // 1 sample, we won't be doing MSAA
  color_attachment.samples = _samples;
  // We clear when this attachment is loaded
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  // We keep the attachment stored when the renderpass ends
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  // We don't care about stencil
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  // We don't know or care about the starting layout of the attachment
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // After the renderpass ends, the image has to be on a layout ready for
  // display
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_attachment_ref = {};
  // Attachment number will index into the pAttachments array in the parent
  // rederpass itself
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Depth attachment
  VkAttachmentDescription depth_attachment = {};
  depth_attachment.flags = 0;
  depth_attachment.format = _depthFormat;
  depth_attachment.samples = _samples;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref = {};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription color_attachment_resolve{};
  color_attachment_resolve.format = _swapchainImageFormat;
  color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_resolve_ref = {};
  color_attachment_resolve_ref.attachment = 2;
  color_attachment_resolve_ref.layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // We are going to create 1 subpass, which is the minimum you can do
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;
  subpass.pResolveAttachments = &color_attachment_resolve_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  // Array of 2 attachments, 1 for color, and other for depth
  auto attachments = std::array<VkAttachmentDescription, 3>(
      {color_attachment, depth_attachment, color_attachment_resolve});

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

  // Connect the color attachment to the info
  render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  render_pass_info.pAttachments = attachments.data();
  // Connect the subpass to the info
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  VK_CHECK(
      vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));

  _mainDeletionQueue.push_function(
      [=, this]() { vkDestroyRenderPass(_device, _renderPass, nullptr); });
}

void VulkanEngine::init_framebuffers() {
  // Create the framebuffers for the swapchain images. This will connect the
  // render-pass to the images for rendering
  VkFramebufferCreateInfo fb_info = {};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.pNext = nullptr;

  fb_info.renderPass = _renderPass;
  fb_info.width = _windowExtent.width;
  fb_info.height = _windowExtent.height;
  fb_info.layers = 1;

  // Grab how many images we have in the swapchain
  auto swapchain_imagecount = static_cast<uint32_t>(_swapchainImages.size());
  _framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

  // Create framebuffers for each of the swapchain image views
  for (uint32_t i = 0; i != swapchain_imagecount; ++i) {
    auto attachments = std::array<VkImageView, 3>{
        _colorImageView, _depthImageView, _swapchainImageViews[i]};

    fb_info.pAttachments = attachments.data();
    fb_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    VK_CHECK(
        vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

    _mainDeletionQueue.push_function([=, this]() {
      vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
      vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    });
  }
}

void VulkanEngine::init_sync_structures() {
  // Create synchronization structures
  // We want ot create the fence with the Create Signaled flag, so we can wait
  // on it before using it on a GPU command (for the first time)
  auto fenceCreateInfo =
      vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

  auto uploadFenceCreateInfo = vkinit::fence_create_info();

  VK_CHECK(vkCreateFence(_device, &uploadFenceCreateInfo, nullptr,
                         &_uploadContext._uploadFence));
  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyFence(_device, _uploadContext._uploadFence, nullptr);
  });

  // For the semaphores we don't need any flags
  auto semaphoreCreateInfo = vkinit::semaphore_create_info();

  for (auto &&frame : _frames) {
    VK_CHECK(
        vkCreateFence(_device, &fenceCreateInfo, nullptr, &frame._renderFence));
    _mainDeletionQueue.push_function(
        [=, this]() { vkDestroyFence(_device, frame._renderFence, nullptr); });

    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                               &frame._presentSemaphore));
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                               &frame._renderSemaphore));

    _mainDeletionQueue.push_function([=, this]() {
      vkDestroySemaphore(_device, frame._presentSemaphore, nullptr);
      vkDestroySemaphore(_device, frame._renderSemaphore, nullptr);
    });
  }
}

void VulkanEngine::init_pipelines() {
  VkShaderModule vertexShader;
  VkShaderModule texturedShader;
  VkShaderModule textVertShader;
  VkShaderModule textFragShader;

  // Compile shaders
  {
    if (!load_shader_module("./shaders/tri_mesh.vert.spv", &vertexShader)) {
      utils::logger.dump(
          "Error when building the triangle vertex shader module",
          spdlog::level::err);
    } else {
      utils::logger.dump("Triangle vertex shader successfully loaded");
    }

    // Compile textured shader
    if (!load_shader_module("./shaders/textured_lit.frag.spv",
                            &texturedShader)) {
      utils::logger.dump("Error when building the textured shader module",
                         spdlog::level::err);
    } else {
      utils::logger.dump("Textured shader successfully loaded");
    }

    // Compile text vert shader
    if (!load_shader_module("./shaders/text.vert.spv", &textVertShader)) {
      utils::logger.dump("Error when building the text vertex shader module",
                         spdlog::level::err);
    } else {
      utils::logger.dump("Text vertex shader successfully loaded");
    }

    // Compile text shader
    if (!load_shader_module("./shaders/text.frag.spv", &textFragShader)) {
      utils::logger.dump("Error when building the text fragment shader module",
                         spdlog::level::err);
    } else {
      utils::logger.dump("Text fragment shader successfully loaded");
    }
  }

  // Build the stage-create-info for both vertex and fragment stages. This
  // lets the pieline know the shader modules per stage
  PipelineBuilder pipelineBuilder;

  // Build the pipeline layout that controls the inputs/outputs of the
  // shader
  auto textured_pipeline_layout_info = vkinit::pipeline_layout_create_info();

  // Setup push constants
  VkPushConstantRange push_constant;
  push_constant.offset = 0;
  push_constant.size = sizeof(MeshPushConstants);
  // This push constant range is accessible only in the vertex shader
  push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  // Push constant setup
  textured_pipeline_layout_info.pPushConstantRanges = &push_constant;
  textured_pipeline_layout_info.pushConstantRangeCount = 1;

  // Vertex input controls how to read vertices from vertex buffers. We
  // aren't using it yet.
  pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

  // Input assembly is the configuration for drawing triangle lists, strips,
  // or individual points. We are just going to draw triangle list
  pipelineBuilder._inputAssembly =
      vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // Build viewport and scissor from the swapchain extents
  pipelineBuilder._viewport.x = 0.0F;
  pipelineBuilder._viewport.y = 0.0F;
  pipelineBuilder._viewport.width = static_cast<float>(_windowExtent.width);
  pipelineBuilder._viewport.height = static_cast<float>(_windowExtent.height);
  pipelineBuilder._viewport.minDepth = 0.0F;
  pipelineBuilder._viewport.maxDepth = 1.0F;

  pipelineBuilder._scissor.offset = {0, 0};
  pipelineBuilder._scissor.extent = _windowExtent;

  // Configure the rasterizer to draw filled triangles
  pipelineBuilder._rasterizer =
      vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

  // We don't use multisampling, so just run the default one
  pipelineBuilder._multisampling =
      vkinit::multisampling_state_create_info(_samples);

  // A single blend attachemnt with now blending and writing to RGBA
  pipelineBuilder._colorBlendAttachment =
      vkinit::color_blend_attachment_state();

  // Add depth testing
  pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(
      true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

  // Build the mesh pipeline
  VertexInputDescription vertexDescription = Vertex::get_vertex_description();

  // Connect the pipeline builder vertex input info to the one we get from
  // Vertex
  pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions =
      vertexDescription.attributes.data();
  pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(vertexDescription.attributes.size());

  pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions =
      vertexDescription.bindings.data();
  pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount =
      static_cast<uint32_t>(vertexDescription.bindings.size());

  // Create pipeline for textured drawing
  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                vertexShader));
  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                texturedShader));

  // Create pipeline layout for the textured mesh, which has 3 descriptor sets
  // We start from the normal mesh layout
  auto texturedSetLayouts = std::array<VkDescriptorSetLayout, 3>{
      _globalSetLayout, _objectSetLayout, _singleTextureSetLayout};
  textured_pipeline_layout_info.setLayoutCount =
      static_cast<uint32_t>(texturedSetLayouts.size());
  textured_pipeline_layout_info.pSetLayouts = texturedSetLayouts.data();

  VkPipelineLayout texturedPipelineLayout;
  VK_CHECK(vkCreatePipelineLayout(_device, &textured_pipeline_layout_info,
                                  nullptr, &texturedPipelineLayout));

  pipelineBuilder._pipelineLayout = texturedPipelineLayout;
  VkPipeline texturePipeline =
      pipelineBuilder.build_pipeline(_device, _renderPass);

  create_material(texturePipeline, texturedPipelineLayout, "terrain");
  create_material(texturePipeline, texturedPipelineLayout, "character");

  // ------------------------------
  // Text pipeline
  // ------------------------------

  // Build the pipeline layout that controls the inputs/outputs of the
  // shader
  auto text_pipeline_layout_info = vkinit::pipeline_layout_create_info();

  // // Setup push constants
  // VkPushConstantRange push_constant;
  // push_constant.offset = 0;
  // push_constant.size = sizeof(MeshPushConstants);
  // // This push constant range is accessible only in the vertex shader
  // push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  // Push constant setup
  text_pipeline_layout_info.pPushConstantRanges = &push_constant;
  text_pipeline_layout_info.pushConstantRangeCount = 1;

  auto textSetLayouts = std::array<VkDescriptorSetLayout, 3>{
      _globalSetLayout, _objectSetLayout, _singleTextureSetLayout};
  text_pipeline_layout_info.setLayoutCount =
      static_cast<uint32_t>(textSetLayouts.size());
  text_pipeline_layout_info.pSetLayouts = textSetLayouts.data();

  pipelineBuilder._shaderStages.clear();
  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                textVertShader));
  pipelineBuilder._shaderStages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                textFragShader));

  VkPipelineLayout textPipelineLayout;
  VK_CHECK(vkCreatePipelineLayout(_device, &text_pipeline_layout_info, nullptr,
                                  &textPipelineLayout));

  pipelineBuilder._pipelineLayout = textPipelineLayout;
  VkPipeline textPipeline =
      pipelineBuilder.build_pipeline(_device, _renderPass);
  create_material(textPipeline, textPipelineLayout, "text");

  // Destroy all shader modules, outside of the queue
  vkDestroyShaderModule(_device, vertexShader, nullptr);
  vkDestroyShaderModule(_device, texturedShader, nullptr);
  vkDestroyShaderModule(_device, textFragShader, nullptr);
  vkDestroyShaderModule(_device, textVertShader, nullptr);

  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyPipeline(_device, textPipeline, nullptr);
    vkDestroyPipelineLayout(_device, textPipelineLayout, nullptr);

    vkDestroyPipeline(_device, texturePipeline, nullptr);
    vkDestroyPipelineLayout(_device, texturedPipelineLayout, nullptr);
  });
}

void VulkanEngine::init_scene() {
  // Create a sampler for the texture
  auto blockySamplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

  VkSampler blockySampler;
  vkCreateSampler(_device, &blockySamplerInfo, nullptr, &blockySampler);

  _mainDeletionQueue.push_function(
      [=, this]() { vkDestroySampler(_device, blockySampler, nullptr); });

  // Sampler for text
  auto textSamplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR);

  VkSampler textSampler;
  vkCreateSampler(_device, &textSamplerInfo, nullptr, &textSampler);

  _mainDeletionQueue.push_function(
      [=, this]() { vkDestroySampler(_device, textSampler, nullptr); });

  Material *terrainMat = get_material("terrain");
  Material *characterMat = get_material("character");
  Material *textMat = get_material("text");

  // Allocate the descriptor set for single-texture to use on the material
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.pNext = nullptr;

  allocInfo.descriptorPool = _descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &_singleTextureSetLayout;

  vkAllocateDescriptorSets(_device, &allocInfo, &terrainMat->textureSet);
  vkAllocateDescriptorSets(_device, &allocInfo, &characterMat->textureSet);
  vkAllocateDescriptorSets(_device, &allocInfo, &textMat->textureSet);

  // Write to the descriptor set so that it points to our diffuse texture
  VkDescriptorImageInfo terrainIBI = {
      .sampler = blockySampler,
      .imageView = _loadedTextures["terrain_diffuse"].imageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  auto terrain_texture =
      vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     terrainMat->textureSet, &terrainIBI, 0);

  vkUpdateDescriptorSets(_device, 1, &terrain_texture, 0, nullptr);

  glm::vec2 gridSize = {50, 50};
  glm::vec2 gridOffset = gridSize / -2.F;

  for (size_t i = 0; i != gridSize.x; ++i) {
    for (size_t j = 0; j != gridSize.y; ++j) {

      RenderObject terrain = {
          .mesh = get_mesh("terrain"),
          .material = get_material("terrain"),
          .transformMatrix = glm::translate(
              glm::vec3{(i + gridOffset.x + (j % 2) * 0.5F) * std::sqrt(3), 0.F,
                        (j + gridOffset.y) * 1.5F})};

      _renderables.push_back(terrain);
    }
  }

  // Write to the descriptor set so that it points to our diffuse texture
  VkDescriptorImageInfo characterIBI = {
      .sampler = blockySampler,
      .imageView = _loadedTextures["character_diffuse"].imageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  auto character_texture = vkinit::write_descriptor_image(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, characterMat->textureSet,
      &characterIBI, 0);

  vkUpdateDescriptorSets(_device, 1, &character_texture, 0, nullptr);

  RenderObject character = {.mesh = get_mesh("character"),
                            .material = get_material("character"),
                            .transformMatrix = glm::translate(
                                glm::mat4{1.F}, glm::vec3{0.F, 0.F, -5.F})};

  _renderables.push_back(character);

  // Write to the descriptor set so that it points to our diffuse texture
  VkDescriptorImageInfo textIBI = {
      .sampler = textSampler,
      .imageView = _loadedTextures["text_msdf"].imageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  auto text_texture =
      vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     textMat->textureSet, &textIBI, 0);

  vkUpdateDescriptorSets(_device, 1, &text_texture, 0, nullptr);

  // RenderObject text = {
  //     .mesh = get_mesh("text"),
  //     .material = get_material("text"),
  //     .transformMatrix =
  //         glm::scale(glm::translate(glm::mat4{1.F}, glm::vec3{0.F, 3.F,
  //         -10.F}),
  //                    glm::vec3(10.F))};

  // _renderables.push_back(text);

  // Load fonts
  FontInfo fontInfo;
  fontInfo.load_from_json("./assets/fonts/Roboto-Regular.json");

  const std::string textString = "a";
  for (auto &&c : textString) {
    unsigned int unicode = static_cast<unsigned int>(c);
    auto atlas = fontInfo._glyphs[unicode].atlasBounds.value_or(Bounds{});

    RenderObject textChar = {
        .mesh = get_mesh("text"),
        .material = get_material("text"),
        .transformMatrix = glm::scale(
            glm::translate(glm::mat4{1.F}, glm::vec3{0.F, 3.F, -10.F}),
            glm::vec3(10.F))};

    _renderables.push_back(textChar);

    utils::logger.dump(fmt::format(
        "Glyph {}, atlasBottom {}, atlasLeft {}, atlasRight {}, atlasTop {}",
        unicode, atlas.bottom, atlas.left, atlas.right, atlas.top));
  }
}

void VulkanEngine::init_descriptors() {
  const size_t sceneParamBufferSize =
      _frames.size() * pad_uniform_buffer_size(sizeof(GPUSceneData));
  _sceneParameterBuffer =
      create_buffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VMA_MEMORY_USAGE_CPU_TO_GPU);

  // Create a descriptor pool that will hold 10 uniform buffers
  std::vector<VkDescriptorPoolSize> sizes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = 0;
  pool_info.maxSets = 10;
  pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
  pool_info.pPoolSizes = sizes.data();

  vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptorPool);

  // Information about the binding
  auto cameraBind = vkinit::descriptorset_layout_binding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

  auto sceneBind = vkinit::descriptorset_layout_binding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
      static_cast<unsigned int>(VK_SHADER_STAGE_VERTEX_BIT) |
          static_cast<unsigned int>(VK_SHADER_STAGE_FRAGMENT_BIT),
      1);

  auto bindings =
      std::array<VkDescriptorSetLayoutBinding, 2>{cameraBind, sceneBind};

  VkDescriptorSetLayoutCreateInfo setInfo = {};
  setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  setInfo.pNext = nullptr;

  setInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  setInfo.flags = 0;
  setInfo.pBindings = bindings.data();

  vkCreateDescriptorSetLayout(_device, &setInfo, nullptr, &_globalSetLayout);

  auto objectBind = vkinit::descriptorset_layout_binding(
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

  VkDescriptorSetLayoutCreateInfo set2info = {};
  set2info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set2info.pNext = nullptr;

  set2info.bindingCount = 1;
  set2info.flags = 0;
  set2info.pBindings = &objectBind;

  vkCreateDescriptorSetLayout(_device, &set2info, nullptr, &_objectSetLayout);

  // Another set, one that holds a single texture
  auto textureBind = vkinit::descriptorset_layout_binding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
      0);

  VkDescriptorSetLayoutCreateInfo set3info = {};
  set3info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set3info.pNext = nullptr;

  set3info.bindingCount = 1;
  set3info.flags = 0;
  set3info.pBindings = &textureBind;

  vkCreateDescriptorSetLayout(_device, &set3info, nullptr,
                              &_singleTextureSetLayout);

  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyDescriptorSetLayout(_device, _singleTextureSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    vmaDestroyBuffer(_allocator, _sceneParameterBuffer._buffer,
                     _sceneParameterBuffer._allocation);
  });

  for (auto &&frame : _frames) {
    const int MAX_OBJECTS = 10000;
    frame.objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                       VMA_MEMORY_USAGE_CPU_TO_GPU);

    frame.cameraBuffer =
        create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);

    // Allocate one descriptor set for each frame
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;

    // Using the pool we just set
    allocInfo.descriptorPool = _descriptorPool;
    // Only 1 descriptor
    allocInfo.descriptorSetCount = 1;
    // Using the global data layout
    allocInfo.pSetLayouts = &_globalSetLayout;

    vkAllocateDescriptorSets(_device, &allocInfo, &frame.globalDescriptor);

    // Allocate the descriptor set that will point to object buffer
    VkDescriptorSetAllocateInfo objectSetAlloc = {};
    objectSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objectSetAlloc.pNext = nullptr;

    objectSetAlloc.descriptorPool = _descriptorPool;
    objectSetAlloc.descriptorSetCount = 1;
    objectSetAlloc.pSetLayouts = &_objectSetLayout;

    vkAllocateDescriptorSets(_device, &objectSetAlloc, &frame.objectDescriptor);

    // Information about the buffer we want to point at in the descriptor
    VkDescriptorBufferInfo cameraInfo;
    // It will be the camera buffer
    cameraInfo.buffer = frame.cameraBuffer._buffer;
    // At 0 offset
    cameraInfo.offset = 0;
    // Of the size of a camera data struct
    cameraInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo sceneInfo;
    sceneInfo.buffer = _sceneParameterBuffer._buffer;
    sceneInfo.offset = 0;
    sceneInfo.range = sizeof(GPUSceneData);

    VkDescriptorBufferInfo objectBufferInfo;
    objectBufferInfo.buffer = frame.objectBuffer._buffer;
    objectBufferInfo.offset = 0;
    objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;

    auto cameraWrite =
        vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        frame.globalDescriptor, &cameraInfo, 0);

    auto sceneWrite = vkinit::write_descriptor_buffer(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, frame.globalDescriptor,
        &sceneInfo, 1);

    auto objectWrite = vkinit::write_descriptor_buffer(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frame.objectDescriptor,
        &objectBufferInfo, 0);

    auto setWrites = std::array<VkWriteDescriptorSet, 3>{
        cameraWrite, sceneWrite, objectWrite};

    vkUpdateDescriptorSets(_device, static_cast<uint32_t>(setWrites.size()),
                           setWrites.data(), 0, nullptr);

    _mainDeletionQueue.push_function([=, this]() {
      vmaDestroyBuffer(_allocator, frame.cameraBuffer._buffer,
                       frame.cameraBuffer._allocation);
      vmaDestroyBuffer(_allocator, frame.objectBuffer._buffer,
                       frame.objectBuffer._allocation);
    });
  }
}

void VulkanEngine::load_meshes() {
  Mesh terrain{};
  Mesh character{};
  Mesh text{};
  {
    utils::Timer timer("Loading mesh took");

    terrain.load_from_meshasset("./assets/terrain/terrain.mesh");
    upload_mesh(terrain);

    character.load_from_meshasset("./assets/character/character.mesh");
    upload_mesh(character);

    text._vertices.resize(6);
    text._vertices[0] = {.position = glm::vec3(1.F, 0.F, 1.F),
                         .normal = glm::vec3(0.F),
                         .color = glm::vec3(0.F),
                         .uv = glm::vec2(1.F, 1.F)};
    text._vertices[1] = {.position = glm::vec3(1.F, 0.F, -1.F),
                         .normal = glm::vec3(0.F),
                         .color = glm::vec3(0.F),
                         .uv = glm::vec2(1.F, 0.F)};
    text._vertices[2] = {.position = glm::vec3(-1.F, 0.F, -1.F),
                         .normal = glm::vec3(0.F),
                         .color = glm::vec3(0.F),
                         .uv = glm::vec2(0.F, 0.F)};
    text._vertices[3] = {.position = glm::vec3(-1.F, 0.F, -1.F),
                         .normal = glm::vec3(0.F),
                         .color = glm::vec3(0.F),
                         .uv = glm::vec2(0.F, 0.F)};
    text._vertices[4] = {.position = glm::vec3(-1.F, 0.F, 1.F),
                         .normal = glm::vec3(0.F),
                         .color = glm::vec3(0.F),
                         .uv = glm::vec2(0.F, 1.F)};
    text._vertices[5] = {.position = glm::vec3(1.F, 0.F, 1.F),
                         .normal = glm::vec3(0.F),
                         .color = glm::vec3(0.F),
                         .uv = glm::vec2(1.F, 1.F)};

    upload_mesh(text);
  }

  _meshes["terrain"] = terrain;
  _meshes["character"] = character;
  _meshes["text"] = text;
}

void VulkanEngine::load_images() {
  Texture terrain;
  {
    utils::Timer timer("Loading asset took");
    vkutil::load_image_from_asset(
        *this, "./assets/terrain/Textures/Tiled_Stone_Grey_Flat_Albedo.tx",
        terrain.image);
  }

  auto terrainImageInfo = vkinit::imageview_create_info(
      VK_FORMAT_R8G8B8A8_SRGB, terrain.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
  vkCreateImageView(_device, &terrainImageInfo, nullptr, &terrain.imageView);

  _mainDeletionQueue.push_function(
      [=, this]() { vkDestroyImageView(_device, terrain.imageView, nullptr); });

  _loadedTextures["terrain_diffuse"] = terrain;

  Texture character;
  {
    utils::Timer timer("Loading asset took");
    vkutil::load_image_from_asset(
        *this, "./assets/character/Textures/Character_Albedo.tx",
        character.image);
  }

  auto characterImageInfo = vkinit::imageview_create_info(
      VK_FORMAT_R8G8B8A8_SRGB, character.image._image,
      VK_IMAGE_ASPECT_COLOR_BIT);
  vkCreateImageView(_device, &characterImageInfo, nullptr,
                    &character.imageView);

  _mainDeletionQueue.push_function([=, this]() {
    vkDestroyImageView(_device, character.imageView, nullptr);
  });
  _loadedTextures["character_diffuse"] = character;

  Texture text;
  {
    utils::Timer timer("Loading asset took");
    vkutil::load_image_from_asset(*this, "./assets/fonts/Roboto-Regular.tx",
                                  text.image);
  }

  auto textImageInfo = vkinit::imageview_create_info(
      VK_FORMAT_R8G8B8A8_SRGB, text.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
  vkCreateImageView(_device, &textImageInfo, nullptr, &text.imageView);

  _mainDeletionQueue.push_function(
      [=, this]() { vkDestroyImageView(_device, text.imageView, nullptr); });
  _loadedTextures["text_msdf"] = text;
}

void VulkanEngine::upload_mesh(Mesh &mesh) {
  const size_t bufferSize = mesh._vertices.size() * sizeof(Vertex);
  // Allocate staging buffer
  VkBufferCreateInfo stagingBufferInfo = {};
  stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  stagingBufferInfo.pNext = nullptr;

  stagingBufferInfo.size = bufferSize;
  stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  // Let the VMA library know that this data should be on CPU RAM
  VmaAllocationCreateInfo vmaallocInfo = {};
  vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

  AllocatedBuffer stagingBuffer;

  // Allocate the buffer
  VK_CHECK(vmaCreateBuffer(_allocator, &stagingBufferInfo, &vmaallocInfo,
                           &stagingBuffer._buffer, &stagingBuffer._allocation,
                           nullptr));

  // Copy vertex data
  void *data;
  vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
  memcpy(data, mesh._vertices.data(), bufferSize);
  vmaUnmapMemory(_allocator, stagingBuffer._allocation);

  // Allocate vertex buffer
  VkBufferCreateInfo vertexBufferInfo = {};
  vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertexBufferInfo.pNext = nullptr;

  vertexBufferInfo.size = bufferSize;
  // This buffer is going to be used as a Vertex Buffer
  vertexBufferInfo.usage =
      static_cast<unsigned int>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
      static_cast<unsigned int>(VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  // Let the VMA library know that this data should be GPU native
  vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  // Allocate the buffer
  VK_CHECK(vmaCreateBuffer(_allocator, &vertexBufferInfo, &vmaallocInfo,
                           &mesh._vertexBuffer._buffer,
                           &mesh._vertexBuffer._allocation, nullptr));

  immediate_submit([=](VkCommandBuffer cmd) {
    VkBufferCopy copy;
    copy.dstOffset = 0;
    copy.srcOffset = 0;
    copy.size = bufferSize;
    vkCmdCopyBuffer(cmd, stagingBuffer._buffer, mesh._vertexBuffer._buffer, 1,
                    &copy);
  });

  // Add the destruction of triangle mesh buffer to the deletion queue
  _mainDeletionQueue.push_function([=, this]() {
    vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer,
                     mesh._vertexBuffer._allocation);
  });

  vmaDestroyBuffer(_allocator, stagingBuffer._buffer,
                   stagingBuffer._allocation);
}

auto VulkanEngine::load_shader_module(const std::filesystem::path &filePath,
                                      VkShaderModule *outShaderModule) -> bool {
  // Open the file with cursor at the end
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  // Find what the size of the file is by looking up hte location of the
  // cursor Because the cursor is at the end, it gives the size directly in
  // bytes
  size_t fileSize = static_cast<size_t>(file.tellg());

  // SPIR-V expects the buffer to be on uint32, so make sure to reserve an
  // int vector big enough for the entire file
  std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

  // Put file cursor at the beginning
  file.seekg(0);

  // Load the entire file into the buffer

  // I have no choice becase of SPIR-V only accepting uint32_t
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  file.read(reinterpret_cast<char *>(buffer.data()), fileSize);

  // Now that the file is loaded into the buffer, we can close it
  file.close();

  // Create a new shader module, using the buffer we loaded
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pNext = nullptr;

  // codeSize has to be in bytes
  createInfo.codeSize = buffer.size() * sizeof(uint32_t);
  createInfo.pCode = buffer.data();

  // Check that the creation goes well
  VkShaderModule shaderModule;
  if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    return false;
  }
  *outShaderModule = shaderModule;
  return true;
}

auto VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout,
                                   const std::string &name) -> Material * {
  Material mat = {.pipeline = pipeline, .pipelineLayout = layout};
  _materials[name] = mat;
  return &_materials[name];
}

auto VulkanEngine::get_material(const std::string &name) -> Material * {
  // Search for the object and return nullptr if not found
  auto it = _materials.find(name);
  if (it == _materials.end()) {
    return nullptr;
  }
  return &(*it).second;
}

auto VulkanEngine::get_mesh(const std::string &name) -> Mesh * {
  auto it = _meshes.find(name);
  if (it == _meshes.end()) {
    return nullptr;
  }
  return &(*it).second;
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject *first,
                                size_t count) {
  // Make a model view matrix for rendering the object
  // Camera view
  glm::mat4 view = _camera.get_view_matrix();
  glm::mat4 projection = _camera.get_projection_matrix();

  // Fill a GPU camera data struct
  GPUCameraData camData = {
      .view = view, .projection = projection, .viewproj = projection * view};

  // And copy it to the buffer
  void *data;
  vmaMapMemory(_allocator, get_current_frame().cameraBuffer._allocation, &data);
  memcpy(data, &camData, sizeof(GPUCameraData));
  vmaUnmapMemory(_allocator, get_current_frame().cameraBuffer._allocation);

  void *objectData;
  vmaMapMemory(_allocator, get_current_frame().objectBuffer._allocation,
               &objectData);
  GPUObjectData *objectSSBO = (GPUObjectData *)objectData;
  for (size_t i = 0; i != count; ++i) {
    RenderObject &object = first[i];
    objectSSBO[i].modelMatrix = object.transformMatrix;
  }
  vmaUnmapMemory(_allocator, get_current_frame().objectBuffer._allocation);

  Mesh *lastMesh = nullptr;
  Material *lastMaterial = nullptr;

  for (size_t i = 0; i != count; ++i) {
    RenderObject &object = first[i];

    // Only bind the pipeline if it doesn't match with the already bound one
    if (object.material != lastMaterial) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        object.material->pipeline);
      lastMaterial = object.material;

      unsigned int frameIndex = _frameNumber % _frames.size();

      // Offset for our scene buffer
      auto uniform_offset = static_cast<uint32_t>(
          pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex);
      // Bind the descriptor set when changing the pipeline
      vkCmdBindDescriptorSets(
          cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout,
          0, 1, &get_current_frame().globalDescriptor, 1, &uniform_offset);

      // Object data descriptor
      vkCmdBindDescriptorSets(
          cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout,
          1, 1, &get_current_frame().objectDescriptor, 0, nullptr);

      if (object.material->textureSet != VK_NULL_HANDLE) {
        // Texture descriptor
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                object.material->pipelineLayout, 2, 1,
                                &object.material->textureSet, 0, nullptr);
      }
    }

    MeshPushConstants constants = {.data = glm::vec4(0),
                                   .render_matrix = object.transformMatrix};

    // Upload the mesh to the GPU via push constants
    vkCmdPushConstants(cmd, object.material->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &constants);

    // Only bind the mesh if it's a different one from last bind
    if (object.mesh != lastMesh) {
      // Bind the mesh vertex buffer with offset 0
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer,
                             &offset);
      lastMesh = object.mesh;
    }
    // We can draw now
    vkCmdDraw(cmd, static_cast<uint32_t>(object.mesh->_vertices.size()), 1, 0,
              static_cast<uint32_t>(i));
  }
}

auto VulkanEngine::get_current_frame() -> FrameData & {
  return _frames[_frameNumber % _frames.size()];
}

auto VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                 VmaMemoryUsage memoryUsage)
    -> AllocatedBuffer {
  // Allocate vertex buffer
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.pNext = nullptr;

  bufferInfo.size = allocSize;
  bufferInfo.usage = usage;

  VmaAllocationCreateInfo vmaallocInfo = {};
  vmaallocInfo.usage = memoryUsage;

  AllocatedBuffer newBuffer;

  VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
                           &newBuffer._buffer, &newBuffer._allocation,
                           nullptr));

  return newBuffer;
}

auto VulkanEngine::pad_uniform_buffer_size(size_t originalSize) const
    -> size_t {
  // Calculate required alignment based on minimum device offset alignment
  size_t minUboAlignment =
      _gpuProperties.limits.minUniformBufferOffsetAlignment;
  size_t alignedSize = originalSize;
  if (minUboAlignment > 0) {
    alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
  }
  return alignedSize;
}

void VulkanEngine::immediate_submit(
    std::function<void(VkCommandBuffer cmd)> &&function) {
  // Allocate the default command buffer that we will use for the instant
  // commands
  auto cmdAllocInfo =
      vkinit::command_buffer_allocate_info(_uploadContext._commandPool, 1);

  VkCommandBuffer cmd;
  VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &cmd));

  // Begin the command buffer recording. We will use this command buffer exactly
  // once, so we want to let vulkan know that
  auto cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  // Execute the function
  function(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));

  auto submit = vkinit::submit_info(&cmd);

  // Sumbit command buffer to the queue and execute it.
  // _uploadFence will now block until the graphic commands finish execution
  VK_CHECK(
      vkQueueSubmit(_graphicsQueue, 1, &submit, _uploadContext._uploadFence));

  vkWaitForFences(_device, 1, &_uploadContext._uploadFence,
                  static_cast<VkBool32>(true), 9999999999);
  vkResetFences(_device, 1, &_uploadContext._uploadFence);

  // Clear the command pool. This will free the command buffer too
  vkResetCommandPool(_device, _uploadContext._commandPool, 0);
}

void VulkanEngine::cleanup() {
  if (_isInitialized) {
    // Make sure the GPU has stopped doing its things
    constexpr int timeout = 1000000000;
    --_frameNumber;
    vkWaitForFences(_device, 1, &get_current_frame()._renderFence,
                    static_cast<VkBool32>(true), timeout);
    ++_frameNumber;

    _mainDeletionQueue.flush();

    vmaDestroyAllocator(_allocator);

    vkDestroyDevice(_device, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
    vkDestroyInstance(_instance, nullptr);

    SDL_DestroyWindow(_window);
  }
}

void VulkanEngine::draw() {
  // Wait until the GPU has finished rendering the last frame. Timeout of 1
  // second.
  VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence,
                           VK_TRUE, 1000000000));
  VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

  // Now that we are sure that the commands finished executing, we can
  // safely reset the command buffer to begin recording again
  VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

  // Request image from the swapchain, one second timeout
  uint32_t swapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000,
                                 get_current_frame()._presentSemaphore, 0,
                                 &swapchainImageIndex));

  VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

  // Begin the command buffer recording. We will use this command buffer
  // exactly once, so wa want to let Vulkan know that
  auto cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  // Make a clear-color from frame number. This will falsh with a 120*pi
  // frame period.
  VkClearValue clearValue;
  const float colorMagic = 120.0F;
  float framed = static_cast<float>(_frameNumber) / colorMagic;
  float flash = abs(sin(framed));
  clearValue.color = {{0.0F, 0.0F, flash, 1.0F}};

  _sceneParameters.ambientColor = {sin(framed), 0, cos(framed), 1};

  char *sceneData;
  vmaMapMemory(_allocator, _sceneParameterBuffer._allocation,
               (void **)&sceneData);

  unsigned int frameIndex = _frameNumber % _frames.size();
  sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
  memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
  vmaUnmapMemory(_allocator, _sceneParameterBuffer._allocation);

  // Clear depth at 1
  VkClearValue depthClear;
  depthClear.depthStencil.depth = 1.F;

  // Start the main renderpass.
  // We will use the clear color from above, and the framebuffer of the
  // index the swapchain gave us
  auto rpInfo = vkinit::renderpass_begin_info(
      _renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

  // Connect clear values
  auto clearValues = std::array<VkClearValue, 2>{clearValue, depthClear};
  rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  rpInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  draw_objects(cmd, _renderables.data(), _renderables.size());

  // ImGui draw stuff
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  // Finalize the render pass
  vkCmdEndRenderPass(cmd);
  // Finalize the comamnd buffer(we can no longer add commands, but it can
  // now be executed)
  VK_CHECK(vkEndCommandBuffer(cmd));

  // Prepare the submission to the queue
  // We want to wait on the _presentSemaphore, as that semaphore is signaled
  // when the swapchain is ready. We will signal the _renderSemaphore, to
  // signal that rendering has finished
  auto submit = vkinit::submit_info(&cmd);

  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit.pWaitDstStageMask = &waitStage;

  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &get_current_frame()._presentSemaphore;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &get_current_frame()._renderSemaphore;

  // Submit command buffer to the queue and execute it.
  // _renderFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit,
                         get_current_frame()._renderFence));

  // This will put the image we just rendered into the visible window
  // We want to wait on the _renderSemaphore for that,
  // as it's necessary that drawing commands have finished before the image
  // is displayed to the user
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;

  presentInfo.pSwapchains = &_swapchain;
  presentInfo.swapchainCount = 1;
  presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

  // Increase the number of frames drawn
  ++_frameNumber;
}

void VulkanEngine::run() {
  SDL_Event e;
  bool bQuit = false;

  auto start = std::chrono::system_clock::now();
  auto end = start;
  auto render_start = std::chrono::system_clock::now();
  auto render_end = render_start;

  // Main loop
  while (!bQuit) {
    render_start = std::chrono::system_clock::now();

    end = std::chrono::system_clock::now();
    std::chrono::duration<float> elapsed_seconds = end - start;
    auto frametime = elapsed_seconds.count() * 1000.F;
    // Log if frametime is slow
    if (frametime > 17) {
      utils::logger.dump(fmt::format("Frame time: {}ms", frametime),
                         spdlog::level::warn);
    }
    start = std::chrono::system_clock::now();

    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      ImGui_ImplSDL2_ProcessEvent(&e);
      _camera.process_input_event(&e);
      // Close the window when user alt-f4s or clicks the X button
      if (e.type == SDL_QUIT) {
        bQuit = true;
      } else if (e.type == SDL_KEYDOWN) {
        // Key handler
      }
    }

    // ImGui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(_window);

    ImGui::NewFrame();

    // ImGui commands
    // ImGui::ShowDemoWindow();

    // Render console window at a fixed position (top-left corner)
    ImVec2 console_window_size = ImVec2(520, 540);
    ImVec2 console_window_pos = ImVec2(0, 0);

    ImGui::SetNextWindowPos(console_window_pos);
    ImGui::SetNextWindowSize(console_window_size);

    ImGui::Begin("Console", NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove);

    for (auto logs = utils::logger.get_logs(); auto &&log : logs) {
      ImGui::TextUnformatted(log.c_str());
    }

    ImGui::End();

    // Render fps window at a fixed position (top-right corner)
    ImVec2 fps_window_size = ImVec2(50, 50);
    ImVec2 fps_window_pos = ImVec2(window_w - fps_window_size.x, 0);

    ImGui::SetNextWindowPos(fps_window_pos);
    ImGui::SetNextWindowSize(fps_window_size);
    ImGui::Begin("FPS", NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove);
    ImGui::Text(fmt::format("{:.2f}", 1000.F / frametime).c_str());

    ImGui::End();
    ImGui::Render();

    _camera.update_camera(frametime);

    draw();

    render_end = std::chrono::system_clock::now();
    std::chrono::duration<float> render_elapsed = render_end - render_start;
    auto rendertime = render_elapsed.count() * 1000.F;

    // Cap FPS at 60
    int sleep_for = std::floor(1000.F / 60.F - rendertime);
    if (sleep_for > 0) {
      SDL_Delay(sleep_for);
    }
  }
}

auto PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
    -> VkPipeline {
  // Make viewport state from our stored viewpor and scissor.
  // At the moment we won't support multiple viewports or scissors.
  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.pNext = nullptr;

  viewportState.viewportCount = 1;
  viewportState.pViewports = &_viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &_scissor;

  // Setup dummy color blending. We aren't using transparent objects yet the
  // blending is just "no blend", but we do write to the color attachment
  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.pNext = nullptr;

  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &_colorBlendAttachment;

  // Build the actual pipeline
  // We now use all of the info structs we have been writing into this one
  // to create the pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = nullptr;

  pipelineInfo.stageCount = static_cast<uint32_t>(_shaderStages.size());
  pipelineInfo.pStages = _shaderStages.data();
  pipelineInfo.pVertexInputState = &_vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.renderPass = pass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.pDepthStencilState = &_depthStencil;

  // It's easy to error out on create graphics pipeline, so we handle it a
  // bit better that the common VK_CHECK case
  VkPipeline newPipeline;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &newPipeline) != VK_SUCCESS) {
    utils::logger.dump("Failed to create pipeline", spdlog::level::err);
    return VK_NULL_HANDLE;
  }
  return newPipeline;
}
