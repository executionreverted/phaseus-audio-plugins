set(TARGET_NAME __PLUGIN_NAME__)

juce_add_plugin(${TARGET_NAME}
    COMPANY_NAME "VSTLibrary"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE VLib
    PLUGIN_CODE __PLUGIN_CODE__
    FORMATS VST3 Standalone
    VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
    PRODUCT_NAME "__PLUGIN_NAME__"
)

juce_generate_juce_header(${TARGET_NAME})

target_sources(${TARGET_NAME}
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(${TARGET_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(APPLE)
    add_custom_target(${TARGET_NAME}_CopyVST3 ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_artefacts/VST3/${TARGET_NAME}.vst3"
            "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/${TARGET_NAME}.vst3"
        DEPENDS ${TARGET_NAME}_VST3
        COMMENT "Copying ${TARGET_NAME}.vst3 to user VST3 folder"
    )
endif()
