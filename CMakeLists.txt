cmake_minimum_required(VERSION 2.8)

project(ulhw)

file(GLOB ulhw_SRC
    "*.h"
    "*.cpp"
    "3rdparty/json.hpp"
    "3rdparty/loguru/*.cpp"
    "3rdparty/loguru/*.h"
    "3rdparty/loguru/*.hpp"
)

include_directories(3rdparty)

add_definitions(-DLOGURU_WITH_STREAMS)
add_definitions(-DNC_ENABLED_SSH)

add_executable(${PROJECT_NAME} ${ulhw_SRC})

target_link_libraries(${PROJECT_NAME} pthread dl yang netconf2)
