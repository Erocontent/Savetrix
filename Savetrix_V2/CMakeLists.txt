cmake_minimum_required(VERSION 3.24)

project(
    Savetrix
    VERSION 2.0.0
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
