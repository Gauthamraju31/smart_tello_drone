cmake_minimum_required(VERSION 3.20)
project(SmartTelloDrone VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find system dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libswscale libavutil)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio imgcodecs dnn video calib3d features2d)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(OpenGL REQUIRED)

# GLFW submodule
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glfw)

# ImGui library from submodule
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)
add_library(imgui 
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# Main executable
file(GLOB_RECURSE SRC_FILES "src/*.cpp")
add_executable(${PROJECT_NAME} main.cpp ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    glfw
    OpenGL::GL
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
    spdlog::spdlog
    yaml-cpp
    Eigen3::Eigen
    pthread
)

# Enable Testing
enable_testing()

add_executable(test_pid tests/test_pid.cpp src/utils/PID.cpp)
target_include_directories(test_pid PRIVATE src)
add_test(NAME test_pid COMMAND test_pid)

add_executable(test_telemetry_parser tests/test_telemetry_parser.cpp)
target_include_directories(test_telemetry_parser PRIVATE src)
add_test(NAME test_telemetry_parser COMMAND test_telemetry_parser)

add_executable(test_tello_sdk tests/test_tello_sdk.cpp src/core/TelloSDK.cpp)
target_include_directories(test_tello_sdk PRIVATE src)
# Needs pthread because TelloSDK creates threads on construction
target_link_libraries(test_tello_sdk PRIVATE pthread spdlog::spdlog)
add_test(NAME test_tello_sdk COMMAND test_tello_sdk)
