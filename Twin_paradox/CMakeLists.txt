cmake_minimum_required(VERSION 3.20)
project(MySDL3Game)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ищем библиотеки в MSYS2 UCRT64
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)

# Создаем исполняемый файл
add_executable(app Twin_P.cpp)

# Линкуем библиотеки к проекту
target_link_libraries(app PRIVATE 
    SDL3::SDL3 
    SDL3_image::SDL3_image
)

# АВТОМАТИЧЕСКИЙ ПЕРЕНОС ВСЕХ ЗАВИСИМЫХ DLL БЕЗ РУЧНОГО ВВОДА
if(WIN32)
    add_custom_command(TARGET app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:app>
        $<TARGET_FILE_DIR:app>
        COMMAND_EXPAND_LISTS
    )
endif()