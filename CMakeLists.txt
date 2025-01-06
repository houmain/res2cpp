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

install(TARGETS res2cpp DESTINATION "bin")

# package
set(CPACK_PACKAGE_NAME "res2cpp")
set(CPACK_PACKAGE_DESCRIPTION "Tool for embedding files in C++ source code")
set(CPACK_PACKAGE_VENDOR "Albert Kalchmair")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "res2cpp")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/houmain/res2cpp")
set(CPACK_PACKAGE_CONTACT ${CPACK_PACKAGE_HOMEPAGE_URL})
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")

if(VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+")
  string(REGEX MATCHALL "[0-9]+" VERSION_LIST "${VERSION}")
  list(GET VERSION_LIST 0 CPACK_PACKAGE_VERSION_MAJOR)
  list(GET VERSION_LIST 1 CPACK_PACKAGE_VERSION_MINOR)
  list(GET VERSION_LIST 2 CPACK_PACKAGE_VERSION_PATCH)
endif()

include(CPack)
