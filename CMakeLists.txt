cmake_minimum_required(VERSION 3.22)

project(InvisAudioPlugins VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Fetch JUCE 9
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        juce9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# Register Invis Core JUCE Module
juce_add_module(modules/invis_core)

# Subdirectories for plugins
add_subdirectory(plugins/InvisDemoPlugin)
