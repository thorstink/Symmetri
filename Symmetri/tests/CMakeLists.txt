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


include_directories(${EIGEN3_INCLUDE_DIRS} ../includes ..)

include(CTest)
include(Catch)

add_executable(${PROJECT_NAME}_test test_actions.cc)
target_link_libraries(${PROJECT_NAME}_test PRIVATE Catch2::Catch2WithMain concurrentqueue)
add_test(${PROJECT_NAME}Test ${PROJECT_NAME}_test)

add_executable(${PROJECT_NAME}_maxplus_test maxplus.cc)
target_link_libraries(${PROJECT_NAME}_maxplus_test PRIVATE Catch2::Catch2WithMain)
add_test(${PROJECT_NAME}_maxplus_test ${PROJECT_NAME}_maxplus_test)
