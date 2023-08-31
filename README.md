## Vulkan engine

Simple rendering engine using Vulkan that works cross-platform on Windows, Linux and MacOS. Uses C++20 standard features. Currently, `fmt` library is used instead of `std::format` because it is not as of yet implemented in GCC.

## Dependencies

**Vulkan SDK** has to be installed in the system. **glslangValidator** usually comes with the Vulkan SDK, but you might need to install it separately (for Arch, use `yay -S glslang-git`). For the rest of dependencies, I use `conan` package manager to pull them all. This project's dependencies can be seen in [conanfile.py](./conanfile.py).

## How to compile it

To compile the project, first you'll need to install `conan` to manage the dependencies. To install it, please refer to [this](https://docs.conan.io/en/latest/installation.html) `conan` installation guide.

Next, we're installing dependencies (replace **Debug** with **Release** if needed):

```sh
# We're building missing packages from source
conan install . -s build_type=Debug --build=missing
```

## Building with CMake

You can use a workflow preset to build the project:
```sh
cmake --workflow --preset debug
```

Or you can do the same thing in two separate commands:
```sh
cmake --preset debug # Configure
cmake --build --preset debug # Build
```

Now, enjoy your freshly minted binaries inside the **build/Debug/bin** folder!

## Baking assets

To load assets quicker, engine uses asset baking. Before launching it, you need to bake the assets using `asset_baker` that is one of the executables that were built:

```sh
# Execute this from project folder
./build/Release/bin/asset_baker ./assets/
```

## Starting the engine

Internal code uses relative paths for loading models and shaders, so make sure that your working directory is the project root. Here's an example of how you can run the binaries:

```sh
./build/Debug/bin/vulkan-engine
```

## Cleaning up build files

If you want to clean up the build files and binaries, you can just remove build folder:

```sh
rm -r build
```

## Generating atlas maps

If you ever need to regenerate atlas maps for fonts, you need to use `msdf-atlas-gen`:

```sh
msdf-atlas-gen.exe -font ./assets/fonts/Roboto-Regular.ttf -format png -imageout ./assets/fonts/Roboto-Regular.png -charset ./assets/fonts/charset.txt -type mtsdf -dimensions 512 512 -pxrange 4
```
