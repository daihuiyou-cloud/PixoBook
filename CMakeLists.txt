cmake_minimum_required(VERSION 3.16)
project(PixoBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
add_compile_options(/utf-8)

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql Concurrent Test)
enable_testing()
option(BUILD_TESTING "Build tests" ON)

# ── Clang-Tidy (opt-in during cmake configure, not on by default) ──
option(RUN_CLANG_TIDY "Run clang-tidy during compilation" OFF)
if(RUN_CLANG_TIDY)
    find_program(CLANG_TIDY_BIN clang-tidy)
    if(CLANG_TIDY_BIN)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_BIN}" --warnings-as-errors=*)
        message(STATUS "clang-tidy: ENABLED  (${CLANG_TIDY_BIN})")
    else()
        message(WARNING "RUN_CLANG_TIDY=ON but clang-tidy not found")
    endif()
endif()

# ── Collect source files ──
file(GLOB_RECURSE LIB_SOURCES src/*.cpp)
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
file(GLOB_RECURSE LIB_HEADERS src/*.h)

add_library(pixobook_lib STATIC ${LIB_SOURCES})
target_include_directories(pixobook_lib PUBLIC src)
target_link_libraries(pixobook_lib PUBLIC Qt5::Widgets Qt5::Sql Qt5::Concurrent)

add_executable(pixobook src/main.cpp resources/resources.qrc)
target_link_libraries(pixobook PRIVATE pixobook_lib)
target_include_directories(pixobook PRIVATE src)

# ── Tests ──
if(BUILD_TESTING)
    function(add_qt_test TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE pixobook_lib Qt5::Test)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()

    file(GLOB TEST_SOURCES tests/tst_*.cpp)
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_qt_test(${TEST_NAME} ${TEST_SOURCE})
    endforeach()
endif()

# ── clang-format targets (non-blocking) ──
find_program(CLANG_FORMAT_BIN clang-format)
if(CLANG_FORMAT_BIN)
    set(FORMAT_FILES ${LIB_SOURCES} ${LIB_HEADERS})
    list(APPEND FORMAT_FILES ${TEST_SOURCES})
    list(APPEND FORMAT_FILES "${CMAKE_SOURCE_DIR}/src/main.cpp")

    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT_BIN} --style=file --dry-run --Werror ${FORMAT_FILES}
        COMMENT "Checking code formatting with clang-format..."
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} --style=file -i ${FORMAT_FILES}
        COMMENT "Formatting code with clang-format..."
    )
    message(STATUS "clang-format: FOUND  (${CLANG_FORMAT_BIN})")
else()
    add_custom_target(check-format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found — install LLVM to enable format checking"
    )
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found — install LLVM to enable formatting"
    )
    message(STATUS "clang-format: not found (install LLVM for format targets)")
endif()

# ── clang-tidy target (runs on all sources, non-blocking) ──
if(CLANG_TIDY_BIN)
    add_custom_target(run-tidy
        COMMAND ${CLANG_TIDY_BIN} --quiet --format-style=file
                ${LIB_SOURCES} ${LIB_HEADERS} ${TEST_SOURCES}
                "${CMAKE_SOURCE_DIR}/src/main.cpp" --
        COMMENT "Running clang-tidy on all sources..."
    )
else()
    add_custom_target(run-tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found — install LLVM to enable static analysis"
    )
endif()

# ── Bug pattern scanner (zero-dependency PowerShell) ──
find_program(POWERSHELL_BIN powershell)
if(POWERSHELL_BIN)
    add_custom_target(scan-bugs
        COMMAND ${POWERSHELL_BIN} -ExecutionPolicy Bypass -File
                "${CMAKE_SOURCE_DIR}/ci/scan-bug-patterns.ps1"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running PixoBook bug pattern scanner..."
    )
endif()
