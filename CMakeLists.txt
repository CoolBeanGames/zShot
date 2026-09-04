cmake_minimum_required(VERSION 3.10)
project(zShot)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(zShot WIN32 main.cpp)
target_link_libraries(zShot gdiplus user32 gdi32 shell32 ole32)
