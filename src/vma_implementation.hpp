// Include Vulkan Memory allocator implementation
#ifdef _WIN32
#pragma warning(disable : 4127 4100 4189 4324)
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#ifdef _WIN32
#pragma warning(default : 4127 4100 4189 4324)
#endif