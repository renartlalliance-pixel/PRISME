cmake_minimum_required(VERSION 3.22)
project(Prisme VERSION 4.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Prisme
  COMPANY_NAME "Atelier Prisme"
  BUNDLE_ID "com.atelierprisme.prisme"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE Aprm
  PLUGIN_CODE Prsm
  FORMATS VST3 Standalone
  PRODUCT_NAME "Prisme"
  DESCRIPTION "Resonateur couleur irise — brut vers tonal, melodique, vivant"
)

juce_generate_juce_header(Prisme)

# Raster embarques des skins :
#  - KAY<3 : 4 PNG decoratifs (licorne, plaque, punk, poupee)
#  - Soviet Analog : potards, faders, LEDs, vis, rivets, texture patinee
#  - Post Apo : fond de grille rouillee (fond_grille.jpg)
juce_add_binary_data(PrismeAssets
  NAMESPACE PrismeAssets
  SOURCES
    Assets/Skins/KAY3/punk_masque_ledge.png
    Assets/Skins/KAY3/doll_emo_goth_ledge.png
    Assets/Skins/KAY3/licorne_tete_filigrane.png
    Assets/Skins/KAY3/plaque_verre_kay.png
    Assets/Skins/KAY3/prisme_graffiti_logo.png
    Assets/Skins/KAY3/header_level_bars.png
    Assets/Skins/SovietAnalog/Textures/texture_patina.png
    Assets/Skins/SovietAnalog/Backgrounds/background_main.png
    Assets/Skins/SovietAnalog/Textures/texture_metal_dark.png
    Assets/Skins/SovietAnalog/Textures/texture_metal_brushed.png
    Assets/Skins/SovietAnalog/Textures/texture_scratches_overlay.png
    Assets/Skins/SovietAnalog/Textures/texture_grain.png
    Assets/Skins/SovietAnalog/Panels/screw_phillips.png
    Assets/Skins/SovietAnalog/Panels/rivet.png
    Assets/Skins/SovietAnalog/Panels/panel_metal_large.png
    Assets/Skins/SovietAnalog/Panels/panel_border.png
    Assets/Skins/SovietAnalog/Panels/panel_header.png
    Assets/Skins/SovietAnalog/Panels/separator.png
    Assets/Skins/SovietAnalog/Knobs/knob_large_base.png
    Assets/Skins/SovietAnalog/Knobs/knob_pointer_amber.png
    Assets/Skins/SovietAnalog/Sliders/fader_track_vertical.png
    Assets/Skins/SovietAnalog/Sliders/fader_handle_vertical.png
    Assets/Skins/SovietAnalog/Indicators/led_red.png
    Assets/Skins/SovietAnalog/Indicators/led_amber.png
    Assets/Skins/SovietAnalog/Indicators/led_off.png
    Assets/Skins/Mineprisme/texture_bois.png
    Assets/Skins/Analog03/texture_bois_dark.png
    Assets/Skins/PostApo/fond_grille.jpg
)
set_property(TARGET PrismeAssets PROPERTY POSITION_INDEPENDENT_CODE TRUE)

target_sources(Prisme PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/RecorderView.cpp
  Source/VariationLab.cpp
)

target_include_directories(Prisme PRIVATE Source)

target_compile_definitions(Prisme PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

# Fichiers sources en UTF-8 (accents de l'UI) quel que soit le codepage Windows
if (MSVC)
  target_compile_options(Prisme PRIVATE /utf-8)
endif()

target_link_libraries(Prisme PRIVATE
  PrismeAssets
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_formats
  juce::juce_gui_basics
  juce::juce_dsp
)

# Les cibles de tests unitaires du paquet d'integration Post Apo ne sont pas
# ajoutees ici : 3 de leurs sources (Tools/TestReverse.cpp, Tools/TestLimiter.cpp
# et Tools/TestVariation.cpp) n'existent pas dans ce projet — les declarer
# ferait echouer la configuration CMake.
