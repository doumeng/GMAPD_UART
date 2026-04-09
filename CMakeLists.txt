cmake_minimum_required(VERSION 3.0)

set(PROJECT_NAME GMAPD_UART)

project(${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 17)
set(TARGET_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/binOutput)

# 可执行文件路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TARGET_BIN_DIR})

set(CMAKE_C_COMPILER    "/home/tcspc/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER  "/home/tcspc/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++")


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fpermissive -fno-aggressive-loop-optimizations  -ffunction-sections -fdata-sections")
add_definitions(-D__aarch64__)

# find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/platform)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/periphery)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/task_module/header)

option(DEFAULT_LOG_LEVEL "default log level" INFO)

link_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/platform
)

file(GLOB SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/task_module/cpp/*.cpp
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME}  
    dl modules periphery rockchip_mpp rockit drm asound cli cjson pthread
    ${OpenCV_LIBS}
    ###### user link lib
)