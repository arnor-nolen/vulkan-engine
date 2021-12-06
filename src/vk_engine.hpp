#pragma once

#include "vk_mesh.hpp"
#include "vk_types.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <vk_mem_alloc.h>

constexpr int window_w = 1700;
constexpr int window_h = 900;

constexpr unsigned int FRAME_OVERLAP = 2;

struct Texture {
  AllocatedImage image;
  VkImageView imageView;
};

struct UploadContext {
  VkFence _uploadFence;
  VkCommandPool _commandPool;
};

struct GPUObjectData {
  glm::mat4 modelMatrix;
};

struct GPUSceneData {
  glm::vec4 fogColor;     // w is for exponent
  glm::vec4 fogDistances; // x for min, y for max, zw unused
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection;
  glm::vec4 sunlightColor;
};

struct GPUCameraData {
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 viewproj;
};

struct FrameData {
  VkSemaphore _presentSemaphore, _renderSemaphore;
  VkFence _renderFence;

  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;

  // Buffer that holds a single GPUCameraData to use when rendering
  AllocatedBuffer cameraBuffer;
  VkDescriptorSet globalDescriptor;

  AllocatedBuffer objectBuffer;
  VkDescriptorSet objectDescriptor;
};

struct Material {
  VkDescriptorSet textureSet{VK_NULL_HANDLE};
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;
};

struct RenderObject {
  Mesh *mesh;
  Material *material;
  glm::mat4 transformMatrix;
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 render_matrix;
};

struct DeletionQueue {
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  std::deque<std::function<void()>> deletors;
  void push_function(std::function<void()> &&function) {
    deletors.push_back(function);
  }

  void flush() {
    // Reverse iterate the deletion queue to execute all the functions
    for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
      (*it)(); // Call the function
    }
    deletors.clear();
  }
};

class VulkanEngine {
public:
  VmaAllocator _allocator;

  DeletionQueue _mainDeletionQueue;

  VkDevice _device; // Vulkan device for commands

  // initializes everything in the engine
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  // run main loop
  void run();

  auto create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                     VmaMemoryUsage memoryUsage) -> AllocatedBuffer;

  void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);

private:
  // Members, all are public in Tutorial
  bool _isInitialized{false};
  int _frameNumber{0};
  int _selectedShader{0};

  VkExtent2D _windowExtent{window_w, window_h};

  struct SDL_Window *_window{nullptr};

  VkInstance _instance;                      // Vulkan library header
  VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
  VkPhysicalDevice _chosenGPU;               // GPU chosen as the default device
  VkSurfaceKHR _surface;                     // Vulkan window surface

  VkSwapchainKHR _swapchain;
  // image format expected by the windowing system
  VkFormat _swapchainImageFormat;
  // array of images from the swapchain
  std::vector<VkImage> _swapchainImages;
  // array of image-views from the swapchain
  std::vector<VkImageView> _swapchainImageViews;

  VkQueue _graphicsQueue;             // Queue we will submit to
  std::uint32_t _graphicsQueueFamily; // Family of the queue

  VkRenderPass _renderPass;
  std::vector<VkFramebuffer> _framebuffers;

  std::array<FrameData, FRAME_OVERLAP> _frames;

  Mesh _triangleMesh;
  Mesh _monkeyMesh;

  VkImageView _depthImageView;
  AllocatedImage _depthImage;

  // The format for depth image
  VkFormat _depthFormat;

  // Default array of renderable objects
  std::vector<RenderObject> _renderables;

  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;

  VkDescriptorSetLayout _globalSetLayout;
  VkDescriptorSetLayout _objectSetLayout;
  VkDescriptorSetLayout _singleTextureSetLayout;

  VkDescriptorPool _descriptorPool;

  VkPhysicalDeviceProperties _gpuProperties;

  GPUSceneData _sceneParameters;
  AllocatedBuffer _sceneParameterBuffer;

  UploadContext _uploadContext;

  std::unordered_map<std::string, Texture> _loadedTextures;

  // Functions
  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_default_renderpass();
  void init_framebuffers();
  void init_sync_structures();
  void init_pipelines();
  void init_scene();
  void init_descriptors();
  void init_imgui();
  void load_meshes();
  void load_images();
  void upload_mesh(Mesh &mesh);
  // Loads a shader module from a SPIR-V file. Returns false if it errors.
  auto load_shader_module(const std::filesystem::path &filePath,
                          VkShaderModule *outShaderModule) -> bool;

  // Create material and add it to the map
  auto create_material(VkPipeline pipeline, VkPipelineLayout layout,
                       const std::string &name) -> Material *;
  auto get_material(const std::string &name) -> Material *;
  auto get_mesh(const std::string &name) -> Mesh *;

  // Our draw function
  void draw_objects(VkCommandBuffer cmd, RenderObject *first, size_t count);

  // Getter for the fraem we are rendering to right now
  auto get_current_frame() -> FrameData &;

  auto pad_uniform_buffer_size(size_t originalSize) const -> size_t;
};

class PipelineBuilder {
public:
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkViewport _viewport;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkRect2D _scissor;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkPipelineColorBlendAttachmentState _colorBlendAttachment;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkPipelineMultisampleStateCreateInfo _multisampling;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkPipelineLayout _pipelineLayout;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  VkPipelineDepthStencilStateCreateInfo _depthStencil;

  auto build_pipeline(VkDevice device, VkRenderPass pass) -> VkPipeline;
};