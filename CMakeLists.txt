cmake_minimum_required(VERSION 3.15)
project(PoolCueBooster CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ดึง ImGui เข้ามาในโปรเจกต์อัตโนมัติผ่าน FetchContent
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

# รวมไฟล์ Source Code ของ ImGui ที่จำเป็น
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# กำหนดไฟล์ Executable
add_executable(PoolCueBooster src/main.cpp ${IMGUI_SOURCES})

# Include Headers
target_include_directories(PoolCueBooster PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

# Link Libraries ของ Windows และ DirectX 11
target_link_libraries(PoolCueBooster PRIVATE d3d11 d3dcompiler dwmapi)
