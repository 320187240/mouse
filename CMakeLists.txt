cmake_minimum_required(VERSION 3.20)
project(mouse)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置为 Windows GUI 应用程序
add_executable(mouse WIN32 main.cpp)

# 链接 Windows API 库
target_link_libraries(mouse
        user32
        gdi32
        comdlg32
        advapi32
        shell32
        ole32
        oleaut32
        uuid
)

# 移除 /MT 标志（如果存在）
if (CMAKE_CXX_FLAGS MATCHES "/MT")
    string(REGEX REPLACE "/MT" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()
