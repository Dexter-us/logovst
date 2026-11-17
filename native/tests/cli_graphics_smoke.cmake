set(invalid_script "${TEST_OUTPUT}.invalid.lgo")
set(invalid_output "${TEST_OUTPUT}.invalid.svg")
set(invalid_size_output "${TEST_OUTPUT}.invalid-size.svg")
if(NOT DEFINED EXPECTED_LABEL)
  set(EXPECTED_LABEL "Hello Logo")
endif()
file(REMOVE "${TEST_OUTPUT}" "${invalid_script}" "${invalid_output}"
     "${invalid_size_output}")

set(graphics_command
    "${LOGO_MIDI_EXECUTABLE}" --graphics "${GRAPHICS_SCRIPT}"
    --output "${TEST_OUTPUT}")
if(DEFINED CLI_CANVAS_SIZE)
  list(APPEND graphics_command --size "${CLI_CANVAS_SIZE}")
endif()
execute_process(
  COMMAND ${graphics_command}
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
if(DEFINED EXPECTED_WIDTH AND DEFINED EXPECTED_HEIGHT)
  set(expected_dimensions
    "width=\"${EXPECTED_WIDTH}\" height=\"${EXPECTED_HEIGHT}\"")
  string(FIND "${svg_contents}" "${expected_dimensions}" dimensions_position)
  if(dimensions_position EQUAL -1)
    message(FATAL_ERROR
      "Graphics CLI SVG dimensions do not match ${EXPECTED_WIDTH}x${EXPECTED_HEIGHT}.")
  endif()
endif()
if(DEFINED EXPECTED_QUERY)
  string(FIND "${graphics_stdout}" "${EXPECTED_QUERY}" query_position)
  if(query_position EQUAL -1)
    message(FATAL_ERROR
      "Graphics CLI did not print expected query output: ${EXPECTED_QUERY}")
  endif()
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

execute_process(
  COMMAND "${LOGO_MIDI_EXECUTABLE}" --graphics "${GRAPHICS_SCRIPT}"
          --output "${invalid_size_output}" --size 0x500
  RESULT_VARIABLE invalid_size_result
  OUTPUT_VARIABLE invalid_size_stdout
  ERROR_VARIABLE invalid_size_stderr
)
if(invalid_size_result EQUAL 0)
  message(FATAL_ERROR "Invalid canvas size unexpectedly succeeded.")
endif()
if(EXISTS "${invalid_size_output}")
  message(FATAL_ERROR "Invalid canvas size left an SVG output file.")
endif()

file(REMOVE "${TEST_OUTPUT}" "${invalid_script}" "${invalid_output}"
     "${invalid_size_output}")
