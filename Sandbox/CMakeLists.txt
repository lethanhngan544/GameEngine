

project(Sandbox)



add_executable(Sandbox WIN32 EntryPoint.cpp)
target_include_directories(Sandbox PRIVATE ${CMAKE_SOURCE_DIR}/include)




find_package(Vulkan REQUIRED)
target_link_libraries(engine PRIVATE Vulkan::Vulkan)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(Sandbox PRIVATE GPUOpen::VulkanMemoryAllocator)
find_package(unofficial-vulkan-memory-allocator-hpp CONFIG REQUIRED)
target_link_libraries(Sandbox PRIVATE unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp)


target_link_libraries(Sandbox PRIVATE engine)



