cmake_minimum_required(VERSION 3.22)
project(TranceModuleXR1 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE -------------------------------------------------------------------
# Usa -DJUCE_SOURCE_DIR=/ruta/a/JUCE para una copia local; si no, la descarga.
set(JUCE_SOURCE_DIR "" CACHE PATH "Carpeta local de JUCE (opcional)")
if(JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# --- Samples incrustados en el plugin ---------------------------------------
file(GLOB SAMPLE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Samples/*.flac)
juce_add_binary_data(XR1Samples SOURCES ${SAMPLE_FILES})
set_target_properties(XR1Samples PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Plugin -----------------------------------------------------------------
juce_add_plugin(TranceModuleXR1
    COMPANY_NAME "TranceModule"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Tmxr
    PLUGIN_CODE Xr01
    FORMATS VST3 Standalone
    PRODUCT_NAME "Trance Module XR-1"
    VST3_CATEGORIES Instrument Synth Sampler)

target_sources(TranceModuleXR1 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SoundBank.cpp)

target_compile_definitions(TranceModuleXR1 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_FLAC=1)

target_link_libraries(TranceModuleXR1
    PRIVATE
        XR1Samples
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
