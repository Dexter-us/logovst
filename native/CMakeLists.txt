cmake_minimum_required(VERSION 3.20)
project(logo_midi LANGUAGES CXX)

option(LOGO_MIDI_BUILD_TESTS "Build the engine tests" ON)
option(LOGO_MIDI_BUILD_VST3 "Build the VST3 MIDI effect" OFF)
set(VST3_SDK_ROOT "" CACHE PATH "Path to a local Steinberg VST3 SDK checkout")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Logo MIDI requires a 64-bit toolchain.")
endif()

add_library(logo_midi_engine
  src/graphics_engine.cpp
  src/logo_music_interpreter.cpp
  src/music_api.cpp
  src/music_engine.cpp
  src/music_script.cpp
  src/svg_renderer.cpp
  src/turtle_interpreter.cpp
)
target_include_directories(logo_midi_engine PUBLIC include)
target_compile_features(logo_midi_engine PUBLIC cxx_std_17)
set_target_properties(logo_midi_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(logo_midi_engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(logo_midi_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(logo-midi
  src/main.cpp
)
target_link_libraries(logo-midi PRIVATE logo_midi_engine)

if(LOGO_MIDI_BUILD_TESTS)
  enable_testing()
  add_executable(logo_midi_engine_tests
    tests/music_engine_tests.cpp
  )
  target_link_libraries(logo_midi_engine_tests PRIVATE logo_midi_engine)
  add_test(NAME logo_midi_engine_tests COMMAND logo_midi_engine_tests)

  add_executable(logo_midi_script_tests
    tests/music_script_tests.cpp
  )
  target_link_libraries(logo_midi_script_tests PRIVATE logo_midi_engine)
  add_test(NAME logo_midi_script_tests COMMAND logo_midi_script_tests)

  add_executable(logo_midi_graphics_tests
    tests/graphics_engine_tests.cpp
  )
  target_link_libraries(logo_midi_graphics_tests PRIVATE logo_midi_engine)
  add_test(NAME logo_midi_graphics_tests COMMAND logo_midi_graphics_tests)

  add_executable(logo_midi_svg_tests
    tests/svg_renderer_tests.cpp
  )
  target_link_libraries(logo_midi_svg_tests PRIVATE logo_midi_engine)
  add_test(NAME logo_midi_svg_tests COMMAND logo_midi_svg_tests)

  add_executable(logo_midi_turtle_tests
    tests/turtle_interpreter_tests.cpp
  )
  target_link_libraries(logo_midi_turtle_tests PRIVATE logo_midi_engine)
  add_test(NAME logo_midi_turtle_tests COMMAND logo_midi_turtle_tests)

  add_test(
    NAME logo_midi_cli_graphics_tests
    COMMAND ${CMAKE_COMMAND}
      -DLOGO_MIDI_EXECUTABLE=$<TARGET_FILE:logo-midi>
      -DGRAPHICS_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/examples/turtle-square.lgo
      -DTEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-graphics-test.svg
      "-DEXPECTED_LABEL=Hello Logo"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_graphics_smoke.cmake
  )
  add_test(
    NAME logo_midi_cli_conditionals_tests
    COMMAND ${CMAKE_COMMAND}
      -DLOGO_MIDI_EXECUTABLE=$<TARGET_FILE:logo-midi>
      -DGRAPHICS_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/examples/conditional-shapes.lgo
      -DTEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-conditionals-test.svg
      "-DEXPECTED_LABEL=IFELSE chooses the color"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_graphics_smoke.cmake
  )
  add_test(
    NAME logo_midi_cli_queries_and_canvas_size_tests
    COMMAND ${CMAKE_COMMAND}
      -DLOGO_MIDI_EXECUTABLE=$<TARGET_FILE:logo-midi>
      -DGRAPHICS_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/examples/cli-queries.lgo
      -DTEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-query-test.svg
      -DCLI_CANVAS_SIZE=640x360
      "-DEXPECTED_LABEL=CLI query output"
      "-DEXPECTED_QUERY=PIXEL [12 34 56]"
      -DEXPECTED_WIDTH=640
      -DEXPECTED_HEIGHT=360
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_graphics_smoke.cmake
  )
  add_test(
    NAME logo_midi_cli_music_tests
    COMMAND ${CMAKE_COMMAND}
      -DLOGO_MIDI_EXECUTABLE=$<TARGET_FILE:logo-midi>
      -DMUSIC_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/examples/music-command-tour.lgo
      -DTEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-music-test.mid
      "-DEXPECTED_SUMMARY=across 5 beats at 60 BPM"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_music_smoke.cmake
  )
  add_test(
    NAME logo_midi_cli_graphics_tour_tests
    COMMAND ${CMAKE_COMMAND}
      -DLOGO_MIDI_EXECUTABLE=$<TARGET_FILE:logo-midi>
      -DGRAPHICS_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/examples/graphics-command-tour.lgo
      -DTEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-graphics-tour.svg
      "-DEXPECTED_LABEL=Logo command tour"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_graphics_smoke.cmake
  )
endif()

if(LOGO_MIDI_BUILD_VST3)
  if(NOT VST3_SDK_ROOT OR NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
      "LOGO_MIDI_BUILD_VST3 requires VST3_SDK_ROOT to point to a VST3 SDK checkout.")
  endif()

  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
  set(VSTGUI_TOOLS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/vst3-sdk"
    EXCLUDE_FROM_ALL)
  add_subdirectory(vst3)
endif()
