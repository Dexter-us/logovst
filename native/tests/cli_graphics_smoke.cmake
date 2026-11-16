set(invalid_script "${TEST_OUTPUT}.invalid.lgo")
set(invalid_output "${TEST_OUTPUT}.invalid.svg")
if(NOT DEFINED EXPECTED_LABEL)
  set(EXPECTED_LABEL "Hello Logo")
endif()
file(REMOVE "${TEST_OUTPUT}" "${invalid_script}" "${invalid_output}")

execute_process(
  COMMAND "${LOGO_MIDI_EXECUTABLE}" --graphics "${GRAPHICS_SCRIPT}"
          --output "${TEST_OUTPUT}"
  RESULT_VARIABLE graphics_result
  OUTPUT_VARIABLE graphics_stdout
  ERROR_VARIABLE graphics_stderr
)
if(NOT graphics_result EQUAL 0)
  message(FATAL_ERROR
    "Graphics CLI failed: ${graphics_stdout}${graphics_stderr}")
endif()
if(NOT EXISTS "${TEST_OUTPUT}")
  message(FATAL_ERROR "Graphics CLI did not create its SVG output.")
endif()
file(READ "${TEST_OUTPUT}" svg_contents)
if(NOT svg_contents MATCHES "<svg" OR
   NOT svg_contents MATCHES "<line" OR
   NOT svg_contents MATCHES "${EXPECTED_LABEL}")
  message(FATAL_ERROR "Graphics CLI output is missing expected SVG content.")
endif()

file(WRITE "${invalid_script}" "FD 10\nUNKNOWN\n")
execute_process(
  COMMAND "${LOGO_MIDI_EXECUTABLE}" --graphics "${invalid_script}"
          --output "${invalid_output}"
  RESULT_VARIABLE invalid_result
  OUTPUT_VARIABLE invalid_stdout
  ERROR_VARIABLE invalid_stderr
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "Invalid graphics script unexpectedly succeeded.")
endif()
if(EXISTS "${invalid_output}")
  message(FATAL_ERROR "Invalid script left a partial SVG output file.")
endif()

file(REMOVE "${TEST_OUTPUT}" "${invalid_script}" "${invalid_output}")
