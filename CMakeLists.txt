cmake_minimum_required(VERSION 3.22)
project(Romanticizm VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(RMZ_BUILD_TESTS "Build DSP test and UI snapshot tools" OFF)
option(RMZ_MINIMAL_LINUX_DEPS "Linux only: do not require ALSA/Xrandr/Xinerama/Xcursor" OFF)
set(RMZ_JUCE_PATH "" CACHE PATH "Path to an existing JUCE checkout (optional, otherwise it is downloaded)")

if(RMZ_JUCE_PATH AND EXISTS "${RMZ_JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${RMZ_JUCE_PATH}" juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(juce)
endif()

set(RMZ_SOURCES
    Source/PluginProcessor.cpp Source/PluginProcessor.h Source/Parameters.h Source/Presets.h
    Source/DSP/Utils.h Source/DSP/Formant.h Source/DSP/Doubler.h Source/DSP/VocalEQ.h Source/DSP/Compressor.h
    Source/DSP/Saturator.h Source/DSP/StereoDelay.h Source/DSP/Reverb.h Source/DSP/Width.h Source/DSP/Limiter.h
    Source/DSP/PitchShift.h Source/DSP/Tape.h
    Source/UI/Theme.h Source/UI/UI.h Source/PluginEditor.h Source/PluginEditor.cpp)

juce_add_binary_data(RMZ_Assets SOURCES Assets/logo.png Assets/legs.png)
set_target_properties(RMZ_Assets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(RMZ_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_JACK=0)
if(RMZ_MINIMAL_LINUX_DEPS AND UNIX AND NOT APPLE)
    list(APPEND RMZ_DEFS JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_ALSA=0)
endif()

set(RMZ_FORMATS VST3)
if(WIN32 OR APPLE)
    list(APPEND RMZ_FORMATS Standalone)
endif()
if(APPLE)
    list(APPEND RMZ_FORMATS AU)
endif()

juce_add_plugin(Romanticizm
    COMPANY_NAME                "Romanticizm"
    PLUGIN_MANUFACTURER_CODE    Rmnt
    PLUGIN_CODE                 Rmz1
    PRODUCT_NAME                "Romanticizm"
    DESCRIPTION                 "Romanticizm vocal plugin"
    FORMATS                     ${RMZ_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx
    AU_MAIN_TYPE                kAudioUnitType_Effect)

juce_generate_juce_header(Romanticizm)
target_sources(Romanticizm PRIVATE ${RMZ_SOURCES})
target_include_directories(Romanticizm PRIVATE Source)
target_compile_definitions(Romanticizm PUBLIC ${RMZ_DEFS})
target_link_libraries(Romanticizm
    PRIVATE RMZ_Assets juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MSVC)
    target_compile_options(Romanticizm PRIVATE /bigobj /utf-8)
endif()

if(RMZ_BUILD_TESTS)
    foreach(T RMZ_Test RMZ_Snap)
        juce_add_console_app(${T} PRODUCT_NAME "${T}")
        if(T STREQUAL "RMZ_Test")
            target_sources(${T} PRIVATE ${RMZ_SOURCES} Tests/dsp_test.cpp)
        else()
            target_sources(${T} PRIVATE ${RMZ_SOURCES} Tests/snapshot.cpp)
        endif()
        target_include_directories(${T} PRIVATE Source)
        target_compile_definitions(${T} PRIVATE ${RMZ_DEFS} JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_AU=0)
        target_link_libraries(${T} PRIVATE RMZ_Assets juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    endforeach()
endif()
