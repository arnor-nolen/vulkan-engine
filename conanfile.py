from conans import ConanFile


class VulkanTutorialConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "sdl/2.0.18",
        "glm/0.9.9.8",
        "imgui/1.85",
        "stb/cci.20210713",
        "tinyobjloader/1.0.6",
        "vk-bootstrap/0.5",
        "vulkan-memory-allocator/3.0.0",
        "vulkan-headers/1.3.211",
        "volk/1.3.204",
        "lz4/1.9.3",
        "nlohmann_json/3.10.4",
        "spdlog/1.8.5",  # Any version >= 1.9.0 crashes on MacOS (see https://github.com/conan-io/conan-center-index/issues/8480)
        "msdfgen/1.9.1",
        # "msdf-atlas-gen/1.2.2",
    )
    generators = "CMakeDeps"
    default_options = {
        "sdl2:opengl": False,
        "sdl2:opengles": False,
        "sdl2:directx": False,
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
