find_package(Catch2 REQUIRED)

include(CTest)
include(Catch)

add_executable(${PROJECT_NAME}_test test.cc)
target_link_libraries(${PROJECT_NAME}_test PRIVATE Catch2::Catch2WithMain)
add_test(${PROJECT_NAME}Test ${PROJECT_NAME}_test)
