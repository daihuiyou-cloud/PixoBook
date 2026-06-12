cmake_minimum_required(VERSION 3.16)
project(AIMaterialLibrary LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql Concurrent Test)

file(GLOB_RECURSE LIB_SOURCES src/*.cpp)
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

add_library(aimateriallib_lib STATIC ${LIB_SOURCES})
target_include_directories(aimateriallib_lib PUBLIC src)
target_link_libraries(aimateriallib_lib PUBLIC Qt5::Widgets Qt5::Sql Qt5::Concurrent)

add_executable(aimateriallib src/main.cpp resources/resources.qrc)
target_link_libraries(aimateriallib PRIVATE aimateriallib_lib)
target_include_directories(aimateriallib PRIVATE src)

function(add_qt_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE aimateriallib_lib Qt5::Test)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

file(GLOB TEST_SOURCES tests/tst_*.cpp)
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_qt_test(${TEST_NAME} ${TEST_SOURCE})
endforeach()
