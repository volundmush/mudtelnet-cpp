cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 23)

project(mudtelnet)

find_package(Boost REQUIRED COMPONENTS program_options regex)

set(MAIN_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MAIN_PROJECT ON)
endif()


file(GLOB MUDTELNET_INCLUDE include/mudtelnet/*.h)
file(GLOB MUDTELNET_SRC src/*.cpp)

add_library(mudtelnet ${MUDTELNET_INCLUDE} ${MUDTELNET_SRC})
link_libraries(mudtelnet ${Boost_LIBRARIES})

include_directories(PUBLIC include
        ${BOOST_LIBRARY_INCLUDES}
        )