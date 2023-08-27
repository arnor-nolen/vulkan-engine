## Vulkan engine

Simple rendering engine using Vulkan that works cross-platform on Windows, Linux and MacOS. Uses C++20 standard features. Currently, `fmt` library is used instead of `std::format` because it is not as of yet implemented in GCC.

## Dependencies

**Vulkan SDK** has to be installed in the system. **glslangValidator** usually comes with the Vulkan SDK, but you might need to install it separately (for Arch, use `yay -S glslang-git`). For the rest of dependencies, I use `conan` package manager to pull them all. This project's dependencies can be seen in [conanfile.py](./conanfile.py).

## How to compile it

To compile the project, first you'll need to install `conan` to manage the dependencies:

```sh
pip install conan
```

If you don't have `pip` installed, refer to [this](https://docs.conan.io/en/latest/installation.html) `conan` installation guide.

Next, we're installing dependencies (replace **Debug** with **Release** if needed):

```sh
cd build
# We're building missing packages from source
conan install .. -s build_type=Debug --output-folder . --build=missing
```

After that, the easiest way to build the application is by using VS Code [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension.

## Building manually (without CMake Tools)

If you're using other editor or don't want to use the CMake Tools extension, you'll have to build the project manually.
First, use CMake to generate the appropriate build files (replace **Debug** with **Release** if needed):

```sh
cd Release
cmake ../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Using generated files, you can compile the project:

```sh
cmake --build . # For MacOS/Linux
cmake --build . --config Debug # For Windows you have to specify build type
```

Now, enjoy your freshly minted binaries inside the **bin** folder!

## Baking assets

To load assets quicker, engine uses asset baking. Before launching it, you need to bake the assets using `asset_baker` that is one of the executables that were built:

```sh
# Execute this from project folder
./build/Release/bin/asset_baker ./assets/
```

## Launching the app

Internal code uses relative paths for loading models and shaders, so make sure that your working directory is the project root. Here's an example of how you can run the binaries:

```sh
./build/Release/bin/vulkan-engine
```

## Cleaning up build files

If you want to clean up the build files and binaries, you can use `git` from the project root directory:

```sh
git clean -dfX build
```

## Generating atlas maps

If you ever need to regenerate atlas maps for fonts, you need to use `msdf-atlas-gen`:

```sh
msdf-atlas-gen.exe -font ./assets/fonts/Roboto-Regular.ttf -format png -imageout ./assets/fonts/Roboto-Regular.png -charset ./assets/fonts/charset.txt -type mtsdf -dimensions 512 512 -pxrange 4
```
