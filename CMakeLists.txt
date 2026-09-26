cmake_minimum_required(VERSION 3.15)
project(PoolCueBooster CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# เติม WIN32 เพื่อให้เป็น GUI Subsystem (ซ่อนหน้าต่าง CMD)
add_executable(PoolCueBooster WIN32 src/main.cpp ${IMGUI_SOURCES})

target_include_directories(PoolCueBooster PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(PoolCueBooster PRIVATE d3d11 d3dcompiler dwmapi)
