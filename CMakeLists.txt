cmake_minimum_required(VERSION 3.14)
project(midi2opl3)

set(CMAKE_CXX_STANDARD 14)

include(cmake/CPM.cmake)

CPMAddPackage(
        NAME libADLMIDI
        GITHUB_REPOSITORY Wohlstand/libADLMIDI
        GIT_TAG 84d27bc2bdbd6dd249537a7f7d2450cbd402482e
        GIT_SHALLOW ON
)

CPMAddPackage(
        NAME cxxopts
        GITHUB_REPOSITORY jarro2783/cxxopts
        VERSION 3.0.0
        GIT_SHALLOW ON
)

add_executable(midi2vgm_opl3 src/midi2vgm_opl3.cpp)
target_include_directories(midi2vgm_opl3 PUBLIC ${libADLMIDI_SOURCE_DIR}/src/)
target_link_libraries(midi2vgm_opl3 ADLMIDI_static cxxopts)
