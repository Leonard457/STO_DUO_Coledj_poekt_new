# Минимальная версия CMake
cmake_minimum_required(VERSION 3.20)

# Название вашего проекта
project(MySDL3Game)

# Указываем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ищем библиотеку SDL3 в системе (MSYS2 установил её так, что CMake её найдет автоматически)
find_package(SDL3 REQUIRED)

# Добавляем ваш исходный файл для сборки в app.exe
add_executable(app D_Minkovsk.cpp)

# Линкуем (подключаем) найденную библиотеку SDL3 к вашей программе
target_link_libraries(app PRIVATE SDL3::SDL3)