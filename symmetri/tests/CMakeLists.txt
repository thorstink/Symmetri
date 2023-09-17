find_package(Catch2 REQUIRED)
find_package(Eigen3 REQUIRED)

include(FetchContent)

FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY https://github.com/cameron314/concurrentqueue
  GIT_TAG master
)

# FetchContent_MakeAvailable(concurrentqueue)
# https://stackoverflow.com/questions/65527126/disable-install-for-fetchcontent
FetchContent_GetProperties(concurrentqueue)

include_directories(${EIGEN3_INCLUDE_DIRS} ../include ..)

include(CTest)
include(Catch)

set(test_files
  assets
)

install(DIRECTORY ${test_files} DESTINATION ${PROJECT_SOURCE_DIR}/build/symmetri/tests)

add_executable(${PROJECT_NAME}_symmetri_test
  test_actions.cc
  test_parser.cc
  test_petri.cc
  test_symmetri.cc
  maxplus.cc
  test_external_input.cc
  test_priorities.cc
  test_bugs.cc
  test_types.cc
  test_callback.cc
)
target_link_libraries(${PROJECT_NAME}_symmetri_test PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME} concurrentqueue)
add_test(${PROJECT_NAME}_symmetri_test ${PROJECT_NAME}_symmetri_test)
