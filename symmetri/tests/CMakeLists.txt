include_directories(${EIGEN3_INCLUDE_DIRS} ../include ..)

include(CTest)

set(test_files
  assets
)

install(DIRECTORY ${test_files} DESTINATION ${PROJECT_SOURCE_DIR}/build/symmetri/tests)

add_executable(${PROJECT_NAME}_symmetri_doctest
  tests.cpp
  actions.cpp
  bugs.cpp
  callback.cpp
  colors.cpp
  external_input.cpp
  parser.cpp
  petri_fire.cpp
  petri.cpp
  priorities.cpp
  symmetri.cpp
  types.cpp
)
target_link_libraries(${PROJECT_NAME}_symmetri_doctest PRIVATE ${PROJECT_NAME})
add_test(${PROJECT_NAME}_symmetri_doctest ${PROJECT_NAME}_symmetri_doctest)
