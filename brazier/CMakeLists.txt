# SPDX-License-Identifier: LGPL-3.0-or-later
cmake_minimum_required(VERSION 3.14)
project(brazier-core VERSION 0.1.6 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(WIN32)
    set(CMAKE_THREAD_PREFER_PTHREAD OFF)
    set(THREADS_PREFER_PTHREAD_FLAG OFF)
    set(CMAKE_USE_WIN32_THREADS_INIT ON)
    set(CMAKE_THREAD_LIBS_INIT "")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_PTHREADS_INIT 0)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(MSVC)
    add_compile_options(/utf-8 /bigobj)

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

find_package(Boost CONFIG REQUIRED COMPONENTS beast asio system thread filesystem)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(brazier-logger CONFIG REQUIRED)

include(CMakePackageConfigHelpers)

set(LIBRARIES
    Boost::beast
    Boost::asio
    Boost::system
    Boost::thread
    Boost::filesystem
    fmt::fmt
    nlohmann_json::nlohmann_json
    hiredis::hiredis
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
    brazier::logger 
)

if(WIN32)
    set(BRAZIER_PLATFORM_DIR "Windows")
elseif(APPLE)
    set(BRAZIER_PLATFORM_DIR "MacOS")
else()
    set(BRAZIER_PLATFORM_DIR "Linux")
endif()

message(STATUS "brazier: platform = ${BRAZIER_PLATFORM_DIR}")

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^src/Platform/")

file(GLOB_RECURSE BRAZIER_PLATFORM_SOURCES CONFIGURE_DEPENDS
    "src/Platform/${BRAZIER_PLATFORM_DIR}/*.cpp")

if(NOT BRAZIER_PLATFORM_SOURCES)
    message(FATAL_ERROR
        "No platform sources found in src/Platform/${BRAZIER_PLATFORM_DIR}/")
endif()

list(APPEND PROJECT_SOURCES ${BRAZIER_PLATFORM_SOURCES})

list(FILTER PROJECT_SOURCES EXCLUDE REGEX "app/Main\\.cpp$")

function(brazier_hide_platform platform)
    if(platform STREQUAL BRAZIER_PLATFORM_DIR)
        return()
    endif()
    file(GLOB_RECURSE OTHER "src/Platform/${platform}/*.cpp")
    if(OTHER)
        set_source_files_properties(${OTHER}
            PROPERTIES HEADER_FILE_ONLY TRUE)
    endif()
endfunction()

brazier_hide_platform(Linux)
brazier_hide_platform(MacOS)
brazier_hide_platform(Windows)

add_library(brazier_core_objects OBJECT ${PROJECT_SOURCES})

target_link_libraries(brazier_core_objects PUBLIC Boost::boost)

target_include_directories(brazier_core_objects PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(brazier_core_objects PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vendor
    ${CMAKE_SOURCE_DIR}/Database
    ${CMAKE_SOURCE_DIR}/Router
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/Storage
)

target_compile_features(brazier_core_objects PUBLIC cxx_std_20)
target_compile_definitions(brazier_core_objects PUBLIC
    BOOST_ASIO_HAS_CO_AWAIT
    BOOST_ASIO_HAS_STD_COROUTINE
)

target_precompile_headers(brazier_core_objects PRIVATE
    <boost/asio.hpp>
    <boost/beast.hpp>
    <fmt/format.h>
    <nlohmann/json.hpp>
    <openssl/ssl.h>
    <coroutine>
    <string>
    <vector>
    <memory>
)

add_library(brazier_core_static STATIC $<TARGET_OBJECTS:brazier_core_objects>)
add_library(brazier_core_shared SHARED $<TARGET_OBJECTS:brazier_core_objects>)

set_target_properties(brazier_core_static PROPERTIES EXPORT_NAME core_static)
set_target_properties(brazier_core_shared PROPERTIES EXPORT_NAME core_shared)

if(MSVC)
    set_target_properties(brazier_core_shared PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

target_compile_definitions(brazier_core_shared PRIVATE brazier_EXPORTS)
target_compile_definitions(brazier_core_shared PUBLIC brazier_SHARED)
target_compile_definitions(brazier_core_static PUBLIC)

set_target_properties(brazier_core_shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(brazier_core_static PUBLIC ${LIBRARIES})
target_link_libraries(brazier_core_shared PUBLIC ${LIBRARIES})

export(TARGETS brazier_core_static brazier_core_shared
    NAMESPACE brazier::
    FILE "${CMAKE_CURRENT_BINARY_DIR}/brazierTargets.cmake"
)

add_executable(brazier_app app/Main.cpp)

target_include_directories(brazier_app PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vendor
    ${CMAKE_SOURCE_DIR}/Database
    ${CMAKE_SOURCE_DIR}/Router
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/Storage
)

target_compile_features(brazier_app PRIVATE cxx_std_20)

target_compile_definitions(brazier_app PRIVATE
    BOOST_ASIO_HAS_CO_AWAIT
    BOOST_ASIO_HAS_STD_COROUTINE
)

target_link_libraries(brazier_app PRIVATE
    brazier_core_static
    ${LIBRARIES}
)

add_library(brazier_core INTERFACE)
set_target_properties(brazier_core PROPERTIES EXPORT_NAME core)

target_link_libraries(brazier_core INTERFACE
    brazier_core_static
    brazier_core_shared
)

install(TARGETS brazier_core_static
    EXPORT brazierTargets
    ARCHIVE DESTINATION lib
)

install(TARGETS brazier_core_shared
    EXPORT brazierTargets
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
)

install(TARGETS brazier_core
    EXPORT brazierTargets
)

install(DIRECTORY include/ DESTINATION include)

install(EXPORT brazierTargets
    NAMESPACE brazier::
    DESTINATION share/brazier-core
    FILE brazierTargets.cmake
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/brazier-coreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/brazier-coreConfig.cmake"
    INSTALL_DESTINATION share/brazier-core
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/brazier-coreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/brazier-coreConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/brazier-coreConfigVersion.cmake"
    DESTINATION share/brazier-core
)

option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    message(STATUS "Building tests")

    find_package(GTest CONFIG REQUIRED)

    get_target_property(GTEST_INCLUDE_DIRS GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)

    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "tests/*.cpp"
    )

    add_executable(brazier_tests ${TEST_SOURCES})

    target_include_directories(brazier_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
        ${fmt_INCLUDE_DIRS}
        ${nlohmann_json_INCLUDE_DIRS}
        ${hiredis_INCLUDE_DIRS}
        ${ZLIB_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        ${GTEST_INCLUDE_DIRS}
    )

    target_compile_definitions(brazier_tests PRIVATE
        BOOST_ASIO_HAS_CO_AWAIT
        BOOST_ASIO_HAS_STD_COROUTINE
    )

    target_compile_features(brazier_tests PRIVATE cxx_std_20)

    target_link_libraries(brazier_tests PRIVATE
        brazier_core_static
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        ${LIBRARIES}
    )

    add_custom_target(check
        COMMAND $<TARGET_FILE:brazier_tests>
        DEPENDS brazier_tests
        COMMENT "Running tests..."
    )

    message(STATUS "Tests configured. Run 'cmake --build . --target check' to run tests")
endif()