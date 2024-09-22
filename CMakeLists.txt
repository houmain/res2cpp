cmake_minimum_required(VERSION 3.5)
project(res2cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(res2cpp res2cpp.cpp)

option(ENABLE_TEST "Enable tests")
if(ENABLE_TEST)
  set(TEST_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  if(MSVC)
    set(TEST_DIRECTORY "${TEST_DIRECTORY}/Debug")
  endif()

  # generate resources and source with res2cpp
  add_executable(test-generate test.cpp)
  target_compile_definitions(test-generate PRIVATE TEST_GENERATE)
  add_dependencies(test-generate res2cpp)
  add_custom_target(run-test-generate
    WORKING_DIRECTORY ${TEST_DIRECTORY}
    COMMAND test-generate
  )

  # compile the generated source and verify
  add_executable(test-verify test.cpp)
  add_dependencies(test-verify run-test-generate)
  target_include_directories(test-verify PRIVATE ${TEST_DIRECTORY})

endif()
