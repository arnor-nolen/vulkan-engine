cmake_minimum_required (VERSION 3.16)
project (vulkan-engine)

# Additional options
#
# Prevent command line window from spawning
# ENABLE_CMD_WINDOW=OFF

# Set binary output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<0:>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<0:>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<0:>)

# Add to find cmake config dependencies 
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

# Adding Vulkan SDK
find_package(Vulkan REQUIRED)

# Setting up dependencies (using CONFIG so find_package doesn't conflict with cmake's own findX.cmake)
find_package(glm REQUIRED CONFIG)
find_package(sdl REQUIRED CONFIG)
find_package(imgui REQUIRED CONFIG)
find_package(stb REQUIRED CONFIG)
find_package(tinyobjloader REQUIRED CONFIG)
find_package(vk-bootstrap REQUIRED CONFIG)
find_package(VulkanHeaders REQUIRED CONFIG)
find_package(vulkan-memory-allocator REQUIRED CONFIG)
find_package(volk REQUIRED CONFIG)
find_package(lz4 REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)


# Set standard (C++20)
set(TARGET_COMPILE_FEATURES cxx_std_20)

# -------------------------------------
# Set compiler-specific stuff
# -------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

  add_compile_options(/W4)

  # Prevent command line window from spawning
  if(NOT ENABLE_CMD_WINDOW)
    add_link_options(/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")

  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

else()

  message(WARNING "Unknown compiler, proceeding without additional compiler options")

endif()
# -------------------------------------
# End of compiler-specific stuff
# -------------------------------------

# -------------------------------------
# Building executables 
# -------------------------------------

# Build ImGui bindings
file(GLOB_RECURSE SOURCE_FILES "src/bindings/*.cpp")
add_library(imgui_lib OBJECT ${SOURCE_FILES})
target_compile_features(imgui_lib PUBLIC ${TARGET_COMPILE_FEATURES})
target_link_libraries(imgui_lib imgui::imgui SDL2 Vulkan::Headers)
# Disable clang-tidy for imgui_lib
set_target_properties(imgui_lib PROPERTIES CXX_CLANG_TIDY "")

# Build asset_lib 
# TODO: make asset_lib shared
file(GLOB_RECURSE SOURCE_FILES "src/assetlib/*.cpp")
add_library(asset_lib STATIC ${SOURCE_FILES})
target_compile_features(asset_lib PUBLIC ${TARGET_COMPILE_FEATURES})
target_link_libraries(asset_lib lz4::lz4 nlohmann_json::nlohmann_json)

# Build asset_baker
file(GLOB_RECURSE SOURCE_FILES "src/asset-baker/*.cpp")
add_executable(asset_baker ${SOURCE_FILES})
target_compile_features(asset_baker PUBLIC ${TARGET_COMPILE_FEATURES})
target_link_libraries(asset_baker asset_lib stb::stb tinyobjloader::tinyobjloader)

# Build the main app 
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")
list(FILTER SOURCE_FILES EXCLUDE REGEX "/bindings/")
list(FILTER SOURCE_FILES EXCLUDE REGEX "/assetlib/")
list(FILTER SOURCE_FILES EXCLUDE REGEX "/asset-baker/")
add_executable (${PROJECT_NAME} ${SOURCE_FILES})
target_compile_features(${PROJECT_NAME} PUBLIC ${TARGET_COMPILE_FEATURES})

# Add src to the include path
target_include_directories(${PROJECT_NAME} PUBLIC src)
# -------------------------------------
# End of building executables 
# -------------------------------------

# -------------------------------------
# Compiling shaders for Vulkan 
# -------------------------------------
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

## find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

## iterate each shader
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  ##execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )

# -------------------------------------
# End of Compiling shaders for Vulkan 
# -------------------------------------

# Link libraries
target_link_libraries(${PROJECT_NAME} asset_lib imgui_lib Vulkan::Vulkan vulkan-memory-allocator::vulkan-memory-allocator glm::glm spdlog::spdlog vk-bootstrap::vk-bootstrap stb::stb tinyobjloader::tinyobjloader)

# Add shaders check
add_dependencies(${PROJECT_NAME} Shaders)
