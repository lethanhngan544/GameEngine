

project(Sandbox)



add_executable(Sandbox WIN32 EntryPoint.cpp "Player.cpp" "MapObject.cpp" "PlayerControlled.cpp" "MapPhysicsObject.cpp")
target_include_directories(Sandbox PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(Sandbox PRIVATE ${CMAKE_SOURCE_DIR}/SandboxInclude)

target_compile_definitions(Sandbox PRIVATE _WIN32_WINNT=0x0A00)


find_package(Vulkan REQUIRED)
target_link_libraries(engine PRIVATE Vulkan::Vulkan)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(Sandbox PRIVATE GPUOpen::VulkanMemoryAllocator)
find_package(unofficial-vulkan-memory-allocator-hpp CONFIG REQUIRED)
target_link_libraries(Sandbox PRIVATE unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp)
find_package(asio CONFIG REQUIRED)
target_link_libraries(Sandbox PRIVATE asio::asio)

find_package(Jolt CONFIG REQUIRED)
target_link_libraries(Sandbox PRIVATE Jolt::Jolt)


target_link_libraries(Sandbox PRIVATE engine)



