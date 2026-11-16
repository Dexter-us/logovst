cmake_minimum_required(VERSION 3.25)
project(logo_midi_vst3 VERSION 0.1.0 LANGUAGES CXX)

smtg_add_vst3plugin(logo-midi-vst3
  resource/logo-midi.uidesc
  source/controller.cpp
  source/controller.hpp
  source/graphics_canvas_view.cpp
  source/graphics_canvas_view.hpp
  source/logo_workspace_view.cpp
  source/logo_workspace_view.hpp
  source/ids.hpp
  source/processor.cpp
  source/processor.hpp
  source/version.h
  source/entry.cpp
)

set_target_properties(logo-midi-vst3 PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
)

target_include_directories(logo-midi-vst3 PRIVATE
  ../include
  "${VST3_SDK_ROOT}"
)
target_link_libraries(logo-midi-vst3 PRIVATE logo_midi_engine sdk vstgui_support)
target_compile_features(logo-midi-vst3 PRIVATE cxx_std_17)
smtg_target_add_plugin_resources(logo-midi-vst3
  RESOURCES
    resource/logo-midi.uidesc
)

if(WIN32)
  target_sources(logo-midi-vst3 PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
elseif(APPLE)
  target_sources(logo-midi-vst3 PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/main/macmain.cpp")
else()
  target_sources(logo-midi-vst3 PRIVATE
    "${VST3_SDK_ROOT}/public.sdk/source/main/linuxmain.cpp")
endif()
