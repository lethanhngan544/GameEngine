project(engine)


add_library(engine STATIC core/Window.cpp "core/Renderer.cpp" "core/Loggers/Logger.cpp")
target_include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/include $ENV{VULKAN_SDK}/Include)
target_link_libraries(engine PRIVATE $ENV{VULKAN_SDK}/Lib/vulkan-1.lib)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(engine PRIVATE glfw)

find_package(glm CONFIG REQUIRED)
target_link_libraries(engine PRIVATE glm::glm)



if(MSVC)
  target_compile_options(engine PUBLIC "/ZI")
  target_link_options(engine PUBLIC "/INCREMENTAL")
endif()