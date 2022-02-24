find_package(Seasocks REQUIRED)
find_package(spdlog REQUIRED)

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

FetchContent_Declare(
  readerwriterqueue
  GIT_REPOSITORY    https://github.com/cameron314/readerwriterqueue
  GIT_TAG           master
)

# FetchContent_MakeAvailable(readerwriterqueue)
# https://stackoverflow.com/questions/65527126/disable-install-for-fetchcontent
FetchContent_GetProperties(readerwriterqueue)
if(NOT readerwriterqueue)
  FetchContent_Populate(readerwriterqueue)
  add_subdirectory(${readerwriterqueue_SOURCE_DIR} ${readerwriterqueue_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# build shared and static versions of the library
message(STATUS "Building Symmetri - shared: ${BUILD_SHARED_LIBS}")


include_directories(include includes submodules ${EIGEN3_INCLUDE_DIRS} ${SEASOCKS_INCLUDE_DIRS} ${catkin_INCLUDE_DIRS} )

#lib
add_library(${PROJECT_NAME} symmetri.cc model.cc actions.cc pnml_parser.cc submodules/tinyxml2/tinyxml2.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE pthread Seasocks::seasocks readerwriterqueue spdlog::spdlog concurrentqueue)
target_include_directories(${PROJECT_NAME} INTERFACE include)

set(ui_files
  ${CMAKE_CURRENT_SOURCE_DIR}/web
)

set(targets
  ${PROJECT_NAME}
)

install(DIRECTORY ${ui_files} include DESTINATION ${PROJECT_SOURCE_DIR}/install)

if(BUILD_TESTING)
add_subdirectory(tests)
endif()
