

project(SandboxDedicatedServer)



add_executable(SandboxDedicatedServer WIN32 EntryPoint.cpp)
target_include_directories(SandboxDedicatedServer PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_definitions(SandboxDedicatedServer PRIVATE _WIN32_WINNT=0x0A00)

find_package(asio CONFIG REQUIRED)
target_link_libraries(SandboxDedicatedServer PRIVATE asio::asio)


target_link_libraries(SandboxDedicatedServer PRIVATE engine)



