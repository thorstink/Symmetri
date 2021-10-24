
Include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        devel
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

add_executable(${PROJECT_NAME}_test catch.cc test.cc)
target_link_libraries(${PROJECT_NAME}_test PRIVATE Catch2::Catch2)
add_test(${PROJECT_NAME}Test ${PROJECT_NAME}_test)
