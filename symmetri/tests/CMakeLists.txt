find_package(Catch2 REQUIRED)
find_package(Eigen3 REQUIRED)


include_directories(${EIGEN3_INCLUDE_DIRS} ../include ..)

include(CTest)
include(Catch)

set(test_files
  assets
)

install(DIRECTORY ${test_files} DESTINATION ${PROJECT_SOURCE_DIR}/build/symmetri/tests)

add_executable(${PROJECT_NAME}_symmetri_test
  test_actions.cpp
  test_parser.cpp
  test_petri_fire.cpp
  test_petri.cpp
  test_symmetri.cpp
  maxplus.cpp
  test_external_input.cpp
  test_priorities.cpp
  test_bugs.cpp
  test_colors.cpp
  test_types.cpp
  test_callback.cpp
)
target_link_libraries(${PROJECT_NAME}_symmetri_test PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME})
add_test(${PROJECT_NAME}_symmetri_test ${PROJECT_NAME}_symmetri_test)
