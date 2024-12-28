

project(Sandbox)

add_executable(Sandbox WIN32 EntryPoint.cpp)
target_include_directories(Sandbox PRIVATE ${CMAKE_SOURCE_DIR}/include $ENV{VULKAN_SDK}/Include)


target_link_libraries(Sandbox PRIVATE engine)


if(MSVC)
  target_compile_options(Sandbox PUBLIC "/ZI")
  target_link_options(Sandbox PUBLIC "/INCREMENTAL")
endif()