project(engine)


add_library(engine STATIC "Window.cpp" "Renderer/Renderer.cpp" "Loggers/Logger.cpp" "Loggers/FileLogger.cpp"  "Renderer/GPUBuffer.cpp"   "Renderer/Image.cpp" "Renderer/DefaultRenderPass.cpp" "Data/StaticModel.cpp" "Renderer/CPUBuffer.cpp" "Renderer/GlobalUniformBuffer.cpp" "Data/Camera.cpp"    "Data/PointLight.cpp" "Data/StaticModelRenderer.cpp"  "Data/LightRenderer.cpp" "Data/StaticModelCache.cpp" "Data/GameObjectManager.cpp" "Physics/Physics.cpp" "Input/Keyboard.cpp" "Input/Mouse.cpp" "Data/DebugRenderer.cpp")

target_include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_definitions(engine PRIVATE _WIN32_WINNT=0x0A00)


find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(engine PRIVATE glfw)

find_package(glm CONFIG REQUIRED)
target_link_libraries(engine PRIVATE glm::glm)

find_package(Vulkan REQUIRED)
target_link_libraries(engine PRIVATE Vulkan::Vulkan)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(engine PRIVATE GPUOpen::VulkanMemoryAllocator)
find_package(unofficial-vulkan-memory-allocator-hpp CONFIG REQUIRED)
target_link_libraries(engine PRIVATE unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp)

find_package(unofficial-shaderc CONFIG REQUIRED)
target_link_libraries(engine PRIVATE unofficial::shaderc::shaderc)


find_package(imgui CONFIG REQUIRED)
target_link_libraries(engine PRIVATE imgui::imgui)

find_package(asio CONFIG REQUIRED)
target_link_libraries(engine PRIVATE asio::asio)

find_package(Jolt CONFIG REQUIRED)
target_link_libraries(engine PRIVATE Jolt::Jolt)
