import os
from conan import ConanFile
from conan.tools.files import copy
from conan.tools.files import replace_in_file
from conan.tools.cmake import cmake_layout, CMakeToolchain

class VulkanTutorialConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    default_options = {
        "sdl/*:opengl": False,
        "sdl/*:opengles": False,
        "sdl/*:directx": False,
        "sdl/*:vulkan": True,
        "sdl/*:nas": False,
        "fmt/*:header_only": True,
        "spdlog/*:header_only": True,
    }

    def requirements(self):
        assert(self.requires is not None)

        self.requires("vulkan-headers/1.3.224.0", override=True)
        self.requires("sdl/2.28.2")
        self.requires("glm/0.9.9.8")
        self.requires("imgui/1.89.8")
        self.requires("stb/cci.20210713")
        self.requires("tinyobjloader/1.0.6")
        self.requires("vk-bootstrap/0.7")
        self.requires("vulkan-memory-allocator/3.0.1")
        self.requires("volk/1.3.224.0")
        self.requires("lz4/1.9.3")
        self.requires("nlohmann_json/3.11.2")
        self.requires("spdlog/1.10.0")  # Any version >= 1.9.0 crashes on MacOS (might be fixed by now, see https://github.com/conan-io/conan-center-index/issues/8480)
        self.requires("fmt/9.1.0", override=True)
        self.requires("wayland/1.22.0", override=True)  # Version 1.21 segfaults
        self.requires("libxml2/2.11.4", override=True)  # For Wayland
        # "msdf-atlas-gen/1.2.2",

    def generate(self):
        assert(self.source_folder is not None)

        # Copy ImGui headers
        copy(self,
             "*sdl2*", 
             src=os.path.join(self.dependencies["imgui"].package_folder, "res", "bindings"),
             dst=os.path.join(self.source_folder, "src", "bindings"),
             keep_path=False
        )

        copy(self,
             "*vulkan*", 
             src=os.path.join(self.dependencies["imgui"].package_folder, "res", "bindings"),
             dst=os.path.join(self.source_folder, "src", "bindings"),
             keep_path=False
        )

        # Fix HIDPI issue with SDL2 bindings
        replace_in_file(self, os.path.join(self.source_folder, "src", "bindings", "imgui_impl_sdl2.cpp"), "SDL_GetWindowSize", "SDL_GetWindowSizeInPixels")

        # Copy shared libraries
        copy(self, "*.dll", src=".", dst="bin", keep_path=False)
        copy(self, "*.dylib", src=".", dst="lib", keep_path=False)
        copy(self, "*.so", src=".", dst="lib", keep_path=False)

        # Configure CMakeToolchain
        tc = CMakeToolchain(self)
        tc.user_presets_path = "ConanPresets.json"
        tc.generate()
        
    def layout(self):
        cmake_layout(self)
