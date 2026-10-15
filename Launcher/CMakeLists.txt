cmake_minimum_required(VERSION 3.20)
project(PhysicsLauncher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск SDL3 (через find_package — как в остальных проектах репозитория)
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

add_executable(launcher launcher.cpp)

target_link_libraries(launcher PRIVATE SDL3::SDL3)

# АВТОМАТИЧЕСКИЙ ПЕРЕНОС DLL (Windows, стиль остальных CMakeLists проекта)
if(WIN32)
    add_custom_command(TARGET launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:launcher>
        $<TARGET_FILE_DIR:launcher>
        COMMAND_EXPAND_LISTS
    )
endif()
