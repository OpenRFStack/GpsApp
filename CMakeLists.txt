cmake_minimum_required(VERSION 3.20)
project(GpsApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# qpid-proton: try pkg-config first, fall back to direct find_library
pkg_check_modules(PROTON QUIET libqpid-proton-cpp)
if(NOT PROTON_FOUND)
    find_library(PROTON_CPP_LIB NAMES qpid-proton-cpp)
    find_path(PROTON_INCLUDE_DIR NAMES proton/connection.hpp)
    if(NOT PROTON_CPP_LIB OR NOT PROTON_INCLUDE_DIR)
        message(FATAL_ERROR "qpid-proton-cpp not found.\n"
            "  Ubuntu: sudo apt-get install libqpid-proton-cpp12-dev\n"
            "  Rocky:  sudo dnf install qpid-proton-cpp-devel")
    endif()
    set(PROTON_LIBRARIES ${PROTON_CPP_LIB})
    set(PROTON_INCLUDE_DIRS ${PROTON_INCLUDE_DIR})
    message(STATUS "qpid-proton-cpp found via find_library: ${PROTON_CPP_LIB}")
endif()

pkg_check_modules(TINYXML2 REQUIRED tinyxml2)
pkg_check_modules(SPDLOG REQUIRED spdlog)
pkg_check_modules(FMT REQUIRED fmt)

include(FetchContent)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_executable(sdr_gps
    src/main.cpp
    src/Config.cpp
    src/GpsdClient.cpp
    src/GpsLogger.cpp
    src/AmqpPublisher.cpp
)

target_include_directories(sdr_gps PRIVATE
    src
    ${PROTON_INCLUDE_DIRS}
    ${TINYXML2_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${FMT_INCLUDE_DIRS}
)

# Pass any extra compile flags from pkg-config (e.g. SPDLOG_FMT_EXTERNAL on Rocky 9)
target_compile_options(sdr_gps PRIVATE
    ${SPDLOG_CFLAGS_OTHER}
    ${FMT_CFLAGS_OTHER}
)

target_link_libraries(sdr_gps PRIVATE
    Threads::Threads
    ${PROTON_LIBRARIES}
    ${TINYXML2_LIBRARIES}
    ${SPDLOG_LIBRARIES}
    ${FMT_LIBRARIES}
    nlohmann_json::nlohmann_json
)

install(TARGETS sdr_gps RUNTIME DESTINATION bin)
