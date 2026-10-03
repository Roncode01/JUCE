cmake_minimum_required(VERSION 3.22)
project(D2DRepro VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(D2DRepro PRODUCT_NAME "D2D Repro")
target_sources(D2DRepro PRIVATE Main.cpp)
target_compile_definitions(D2DRepro PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(D2DRepro PRIVATE
    juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
