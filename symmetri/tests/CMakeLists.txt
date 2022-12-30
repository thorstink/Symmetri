find_package(Catch2 REQUIRED)
find_package(Eigen3 REQUIRED)

include(FetchContent)

FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY    https://github.com/cameron314/concurrentqueue
  GIT_TAG           master
)

# FetchContent_MakeAvailable(concurrentqueue)
# https://stackoverflow.com/questions/65527126/disable-install-for-fetchcontent
FetchContent_GetProperties(concurrentqueue)
if(NOT concurrentqueue_POPULATED)
  FetchContent_Populate(concurrentqueue)
  add_subdirectory(${concurrentqueue_SOURCE_DIR} ${concurrentqueue_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()


include_directories(${EIGEN3_INCLUDE_DIRS} ../include ..)

include(CTest)
include(Catch)

set(test_files
  assets
)

install(DIRECTORY ${test_files} DESTINATION ${PROJECT_SOURCE_DIR}/build/symmetri/tests)

add_executable(${PROJECT_NAME}_actions_test test_actions.cc)
target_link_libraries(${PROJECT_NAME}_actions_test PRIVATE Catch2::Catch2WithMain pthread ${PROJECT_NAME} concurrentqueue)
add_test(${PROJECT_NAME}_actions_test ${PROJECT_NAME}_actions_test)

add_executable(${PROJECT_NAME}_parser_test test_parser.cc)
target_link_libraries(${PROJECT_NAME}_parser_test PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME})
add_test(${PROJECT_NAME}_parser_test ${PROJECT_NAME}_parser_test)

add_executable(${PROJECT_NAME}_model_test test_model.cc)
target_link_libraries(${PROJECT_NAME}_model_test PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME} concurrentqueue)
add_test(${PROJECT_NAME}_model_test ${PROJECT_NAME}_model_test)

add_executable(${PROJECT_NAME}_symmetri_test test_symmetri.cc)
target_link_libraries(${PROJECT_NAME}_symmetri_test PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME} concurrentqueue)
add_test(${PROJECT_NAME}_symmetri_test ${PROJECT_NAME}_symmetri_test)

add_executable(${PROJECT_NAME}_maxplus_test maxplus.cc)
target_link_libraries(${PROJECT_NAME}_maxplus_test PRIVATE Catch2::Catch2WithMain)
add_test(${PROJECT_NAME}_maxplus_test ${PROJECT_NAME}_maxplus_test)

add_executable(${PROJECT_NAME}_symmetri_bugs test_bugs.cc)
target_link_libraries(${PROJECT_NAME}_symmetri_bugs PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME} concurrentqueue spdlog::spdlog)
add_test(${PROJECT_NAME}_symmetri_bugs ${PROJECT_NAME}_symmetri_bugs)