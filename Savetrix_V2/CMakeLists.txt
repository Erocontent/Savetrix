cmake_minimum_required(VERSION 3.24)

project(
    Savetrix
    VERSION 2.3.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the CommonLibSSE-NG build focused on flat Skyrim SE/AE.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_PATCH_SAFETY OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG v6.2.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(CommonLibSSE)

find_package(nlohmann_json CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIRS "SimpleIni.h" REQUIRED)
configure_file(src/Version.h.in generated/Version.h @ONLY)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/Savetrix.version" "${PROJECT_VERSION}\n")

add_library(
    Savetrix
    SHARED
        src/main.cpp
        src/TransferService.cpp
        src/TransferService.h
        src/CampaignTransfer.cpp
        src/CampaignTransfer.h
        src/Profile.cpp
        src/Profile.h
        src/FormRef.cpp
        src/FormRef.h
        src/InputSink.cpp
        src/InputSink.h
        src/Paths.cpp
        src/Paths.h
)

target_link_libraries(
    Savetrix
    PRIVATE
        CommonLibSSE::CommonLibSSE
        nlohmann_json::nlohmann_json
)
target_include_directories(Savetrix PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" ${SIMPLEINI_INCLUDE_DIRS})

target_compile_definitions(
    Savetrix
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(Savetrix PRIVATE /W4 /permissive- /EHsc)
endif()

set_target_properties(Savetrix PROPERTIES
    OUTPUT_NAME "Savetrix"
)

add_custom_command(
    TARGET Savetrix POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist/Data/SKSE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Savetrix>"
        "${CMAKE_BINARY_DIR}/dist/Data/SKSE/Plugins/Savetrix.dll"
    COMMENT "Staging Savetrix.dll"
)

option(SAVETRIX_BUILD_TESTS "Build Savetrix regression tests" ON)
if(SAVETRIX_BUILD_TESTS)
    enable_testing()
    add_executable(SavetrixSettingsTests tests/settings_tests.cpp)
    add_executable(SavetrixInputTests tests/input_tests.cpp src/InputSink.cpp)
    target_include_directories(SavetrixInputTests BEFORE PRIVATE tests/stubs)
    foreach(test_target SavetrixSettingsTests SavetrixInputTests)
        target_include_directories(${test_target} PRIVATE src ${SIMPLEINI_INCLUDE_DIRS})
        target_link_libraries(${test_target} PRIVATE spdlog::spdlog)
        target_compile_definitions(${test_target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endforeach()
    add_test(NAME settings COMMAND SavetrixSettingsTests "${CMAKE_CURRENT_BINARY_DIR}/settings-tests")
    add_test(NAME input COMMAND SavetrixInputTests)
endif()
