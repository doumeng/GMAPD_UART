cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME GMAPD_UART)

project(${PROJECT_NAME})

set(OpenCV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/opencv4/lib/cmake/opencv4)
find_package(OpenCV 4 REQUIRED)

set(Open3D_DIR /usr/local/lib/open3d/lib/cmake/Open3D)
find_package(Open3D REQUIRED)

set(TARGET_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/binOutput)

# 可执行文件路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TARGET_BIN_DIR})

SET(CMAKE_CXX_COMPILER  g++)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fpermissive -fno-aggressive-loop-optimizations  -ffunction-sections -fdata-sections")
add_definitions(-D__aarch64__)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/platform)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/periphery)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/opencv4/include/opencv4)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/task_module/header)

link_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdPartyLib/opencv4/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/opencv4/lib
)


option(DEFAULT_LOG_LEVEL "default log level" INFO)

file(GLOB SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/task_module/cpp/*.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME}
        dl modules periphery rockchip_mpp rockit drm asound cli cjson pthread
        ${OpenCV_LIBS}
        Open3D::Open3D
        ###### user link lib
)