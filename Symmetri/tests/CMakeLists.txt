find_package(Catch2 REQUIRED)
find_package(Eigen3 REQUIRED)

include_directories(${EIGEN3_INCLUDE_DIRS})

include(CTest)
include(Catch)

add_executable(${PROJECT_NAME}_test test.cc)
target_link_libraries(${PROJECT_NAME}_test PRIVATE Catch2::Catch2WithMain)
add_test(${PROJECT_NAME}Test ${PROJECT_NAME}_test)

add_executable(${PROJECT_NAME}_maxplus_test maxplus.cc)
target_link_libraries(${PROJECT_NAME}_maxplus_test PRIVATE Catch2::Catch2WithMain)
add_test(${PROJECT_NAME}_maxplus_test ${PROJECT_NAME}_maxplus_test)
