cmake_minimum_required(VERSION 3.20)
project(zShot)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(UNICODE _UNICODE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})

add_subdirectory(third_party/zui/bindings/cpp)

add_executable(zShot WIN32 main.cpp resource.rc ui.g.cpp third_party/zui/bindings/cpp/zui_webview2.cpp)
target_include_directories(zShot PRIVATE third_party/wil/include)
target_include_directories(zShot PRIVATE $ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/1.0.2478.35/build/native/include)
target_link_libraries(zShot gdiplus user32 gdi32 shell32 ole32 zui version shlwapi)
target_link_libraries(zShot $ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/1.0.2478.35/build/native/x64/WebView2LoaderStatic.lib)
