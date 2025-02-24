cmake_minimum_required(VERSION 3.20)
project(mouse LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置为 Windows GUI 应用程序
add_executable(mouse WIN32 main.cpp)

# 设置 Windows 运行时库为动态链接 (/MD)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# 链接 Windows API 库
target_link_libraries(mouse PRIVATE
        user32
        gdi32
        comdlg32
        advapi32
        shell32
        ole32
        oleaut32
        uuid
)

# 针对 MinGW 的额外配置
if(MINGW)
    # 静态链接标准库
    target_link_options(mouse PRIVATE -static -static-libgcc -static-libstdc++)

    # 设置 MinGW 的 bin 目录路径（根据你的实际路径修改）
    set(MINGW_BIN_DIR "C:/msys64/mingw64/bin")  # 示例路径

    # 定义需要复制的 DLL
    set(MINGW_DLLS
            "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
            "${MINGW_BIN_DIR}/libstdc++-6.dll"
            "${MINGW_BIN_DIR}/libwinpthread-1.dll"
    )

    # 在构建后复制 DLL 到输出目录
    add_custom_command(TARGET mouse POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MINGW_DLLS}
            $<TARGET_FILE_DIR:mouse>
            COMMENT "Copying MinGW runtime DLLs"
    )
endif()