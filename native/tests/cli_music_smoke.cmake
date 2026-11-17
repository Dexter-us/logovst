set(invalid_script "${TEST_OUTPUT}.invalid.lgo")
set(invalid_output "${TEST_OUTPUT}.invalid.mid")
file(REMOVE "${TEST_OUTPUT}" "${invalid_script}" "${invalid_output}")

execute_process(
  COMMAND "${LOGO_MIDI_EXECUTABLE}" "${MUSIC_SCRIPT}" --output "${TEST_OUTPUT}"
  RESULT_VARIABLE music_result
  OUTPUT_VARIABLE music_stdout
  ERROR_VARIABLE music_stderr
)
if(NOT music_result EQUAL 0)
  message(FATAL_ERROR "Music CLI failed: ${music_stdout}${music_stderr}")
endif()
if(NOT EXISTS "${TEST_OUTPUT}")
  message(FATAL_ERROR "Music CLI did not create its MIDI output.")
endif()
file(READ "${TEST_OUTPUT}" midi_header LIMIT 4)
if(NOT midi_header STREQUAL "MThd")
  message(FATAL_ERROR "Music CLI output does not have a Standard MIDI header.")
endif()
if(NOT music_stdout MATCHES "Wrote [0-9]+ MIDI events")
  message(FATAL_ERROR "Music CLI did not report its output summary.")
endif()
if(DEFINED EXPECTED_SUMMARY AND NOT music_stdout MATCHES "${EXPECTED_SUMMARY}")
  message(FATAL_ERROR "Music CLI summary did not include ${EXPECTED_SUMMARY}.")
endif()

file(WRITE "${invalid_script}" "NOTE 200 1\n")
execute_process(
  COMMAND "${LOGO_MIDI_EXECUTABLE}" "${invalid_script}" --output "${invalid_output}"
  RESULT_VARIABLE invalid_result
  OUTPUT_VARIABLE invalid_stdout
  ERROR_VARIABLE invalid_stderr
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "Invalid music script unexpectedly succeeded.")
endif()
if(EXISTS "${invalid_output}")
  message(FATAL_ERROR "Invalid music script left a MIDI output file.")
endif()

file(REMOVE "${TEST_OUTPUT}" "${invalid_script}" "${invalid_output}")
