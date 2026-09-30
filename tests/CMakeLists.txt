find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(
  NAME lint
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/lint_sources.py
          ${CMAKE_SOURCE_DIR})

if(TARGET alienfx_cli)
  add_test(
    NAME pipe
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/pipe_tests.py
            $<TARGET_FILE:alienfx_cli>)
endif()
