project(engine)


add_library(engine STATIC core/Window.cpp "core/Renderer/Renderer.cpp" "core/Loggers/Logger.cpp" "core/Loggers/FileLogger.cpp" "core/Loggers/VisualStudioLogger.cpp" "core/Renderer/GPUBuffer.cpp"   "core/Renderer/Image.cpp" "core/Renderer/DefaultRenderPass.cpp" "core/Data/StaticModel.cpp" "core/Renderer/CPUBuffer.cpp" "core/Renderer/GlobalUniformBuffer.cpp" "core/Data/Camera.cpp"  "core/Renderer/AmbientLightPipeline.cpp" "core/Renderer/PointLightPipeline.cpp" "core/Data/PointLight.cpp" "core/Data/StaticModelRenderer.cpp")

target_include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/include)
#target_link_libraries(engine PRIVATE $ENV{VULKAN_SDK}/Lib/vulkan-1.lib)

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

