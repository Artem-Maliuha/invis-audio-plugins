juce_add_plugin(InvisDemoPlugin
    COMPANY_NAME "Invis Audio"
    BUNDLE_ID "com.invisaudio.invisdemoplugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Invs
    PLUGIN_CODE Indm
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Invis Demo Plugin"
)

target_compile_definitions(InvisDemoPlugin PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_sources(InvisDemoPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_link_libraries(InvisDemoPlugin PRIVATE
    invis_core
    juce::juce_audio_utils
    juce::juce_dsp
)

juce_generate_juce_header(InvisDemoPlugin)
