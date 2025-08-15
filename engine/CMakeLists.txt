project(engine)


add_library(engine STATIC "Window.cpp" "ImGuiFileDialog.cpp"  "Renderer/Renderer.cpp" "Loggers/Logger.cpp" "Loggers/FileLogger.cpp"  "Renderer/GPUBuffer.cpp"   "Renderer/Image.cpp" "Renderer/DefaultRenderPass.cpp" "Components/StaticModel.cpp" "Renderer/CPUBuffer.cpp" "Renderer/GlobalUniformBuffer.cpp" "Components/Camera.cpp"    "Components/PointLight.cpp" "Data/StaticModelRenderer.cpp"  "Data/LightRenderer.cpp"  "World/GameObjectManager.cpp" "Physics/Physics.cpp" "Input/Keyboard.cpp" "Input/Mouse.cpp" "Data/DebugRenderer.cpp" "Data/Data.cpp" "Data/ParticleRenderer.cpp" "Components/ParticleEmiter.cpp"  "Components/RigidBody.cpp" "Components/ModelCache.cpp" "Components/AnimatedModel.cpp" "Data/AnimatedModelRenderer.cpp" "Components/Animator.cpp" "Components/Animation.cpp" "Data/SkyRenderer.cpp" "Components/CameraFrustumCuller.cpp" "Components/Animator2DBlend.cpp" "Renderer/PostprocessingRenderPass.cpp" "Renderer/Atmosphere.cpp")

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

find_package(Stb REQUIRED)
target_include_directories(engine PRIVATE ${Stb_INCLUDE_DIR})
