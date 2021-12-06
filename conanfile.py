from conans import ConanFile


class VulkanTutorialConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "sdl2/2.0.16@bincrafters/stable",
        "glm/0.9.9.8",
        "imgui/1.85",
        "stb/cci.20210713",
        "tinyobjloader/1.0.6",
        "vk-bootstrap/0.4",
        "vulkan-memory-allocator/2.3.0",
        "vulkan-headers/1.2.195",
        "volk/1.2.195",
        "lz4/1.9.3",
        "nlohmann_json/3.10.4"
    )
    generators = "cmake"
    default_options = {
        "sdl2:opengl": False,
        "sdl2:opengles": False,
    }

    def imports(self):
        # Copy shared libraries
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.dylib", dst="lib", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)

        # Copy ImGui headers
        self.copy(
            "imgui_impl_sdl.cpp",
            dst="../../src/bindings",
            src="./res/bindings",
        )
        self.copy(
            "imgui_impl_sdl.h", dst="../../src/bindings", src="./res/bindings"
        )
        self.copy(
            "imgui_impl_vulkan.cpp",
            dst="../../src/bindings",
            src="./res/bindings",
        )
        self.copy(
            "imgui_impl_vulkan.h",
            dst="../../src/bindings",
            src="./res/bindings",
        )

    def configure(self):
        # Disable iconv compilation on Windows, since it is not possible
        # Also disable directx since we're not going to use it
        if self.settings.os == "Windows":
            self.options['sdl2'].iconv = False
            self.options['sdl2'].directx = False