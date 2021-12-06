## Vulkan engine

Simple rendering engine using Vulkan that works cross-platform on Windows, Linux and MacOS. Uses C++20 standard features.

## Dependencies

Vulkan SDK has to be installed in the system. For the rest of dependencies, I use `conan` package manager to pull them all. This project's dependencies can be seen in [conanfile.py](./conanfile.py).

## How to compile it

To compile the project, first you'll need to install `conan` to manage the dependencies:

```sh
pip install conan
```

If you don't have `pip` installed, refer to [this](https://docs.conan.io/en/latest/installation.html) `conan` installation guide.

Next, we're creating two profiles for Debug and Release:

```sh
cd build
# We're building missing packages from source
conan install .. -s build_type=Debug -if Debug --build=missing
conan install .. -s build_type=Release -if Release --build=missing
```

After that, the easiest way to build the application is by using VS Code [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension.

## Building manually

If you're using other editor or don't want to use the CMake Tools extension, you'll have to build the project manually.
First, use CMake to generate the appropriate build files (replace **Release** with **Debug** if needed):

```sh
cd Release
cmake ../.. -DCMAKE_BUILD_TYPE=Release
```

Using generated files, you can compile the project:

```sh
cmake --build . # For MacOS/Linux
cmake --build . --config Release # For Windows you have to specify build type
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
