# Librush Examples

Example apps and a test harness for librush, covering basic rendering API usage.

## How to build

The easiest method is to use CMake presets:
```
cmake --list-presets
cmake --preset <preset>
cmake --build --preset <preset> [--target Tests]
```
Build output goes to `Build/<preset>/...`.

#### Common presets: 

* Windows `vs2022-vk`, `vs2026-vk`
* Linux `ninja-debug-vk`, `ninja-release-vk`
* macOS `ninja-debug-vk`, `ninja-release-vk`, `ninja-debug-metal`, `ninja-release-metal`, `xcode-vk`, `xcode-metal`

## How to run

```
./Build/<preset>/01-HelloWorld/01-HelloWorld
./Build/<preset>/Tests/Tests
```

## External dependencies

- Vulkan builds require the Vulkan SDK (`VULKAN_SDK` or `VK_SDK_PATH` set). MoltenVK on macOS.
- Shader tools: `glslc` and `glslangValidator` are used for shader compilation; `spirv-cross` is used on macOS for Metal shader translation.
