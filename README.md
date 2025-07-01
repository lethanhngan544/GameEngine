# 🚀 A custom 3D game engine built from scratch using Vulkan, C++, CMake, and vcpkg. Supports real-time rendering, deferred shading, and physics integration.

![image](https://github.com/user-attachments/assets/e7865d32-0b62-45de-be31-af5853add139)

# ✨ Features
Custom engine architecture

Real-time rendering with Vulkan

Deferred shading pipeline

Physics integration (e.g., Jolt Physics)

Asset management system


# 📦 Build Instructions
Prerequisites
- CMake (version >= 3.20)
- C++17 compatible compiler (Visual Studio 2022, GCC 11+, Clang 13+)
- vcpkg
- Vulkan SDK installed (from LunarG)

# Install Dependencies
If you haven't set up vcpkg yet:
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat   # (Windows)
or
./bootstrap-vcpkg.sh    # (Linux)
Navigate to project folder and run the following command to install dependencies:
./vcpkg install

# 🛠️ Technologies Used
- Vulkan API

- C++

- CMake

- vcpkg

- GLFW

- GLM

- SPIR-V

- Jolt Physics (optional)
