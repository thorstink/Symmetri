
cmake_minimum_required(VERSION 3.10)
project(Symmetri VERSION 1.0.0)

# if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
# 	set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
# endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers -fsanitize=address -fno-omit-frame-pointer -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-missing-field-initializers -fsanitize=address -fno-omit-frame-pointer -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O0 -s")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")

find_package(Eigen3 REQUIRED)
find_package(Seasocks REQUIRED)
find_package(roscpp REQUIRED)
find_package(std_msgs REQUIRED)

set(catkin_INCLUDE_DIRS
  ${roscpp_INCLUDE_DIRS}
  ${std_msgs_INCLUDE_DIRS}
)

set(catkin_LIBRARIES
  ${roscpp_LIBRARIES}
  ${std_msgs_LIBRARIES}
)

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


include_directories(Symmetri/include ${EIGEN3_INCLUDE_DIRS} ${SEASOCKS_INCLUDE_DIRS} ${catkin_INCLUDE_DIRS}  submodules )

add_library(${PROJECT_NAME}_pnml_parser Symmetri/pnml_parser.cc submodules/tinyxml2/tinyxml2.cpp)

#lib
add_library(${PROJECT_NAME} Symmetri/symmetri.cc Symmetri/model.cc Symmetri/actions.cc)
target_link_libraries(${PROJECT_NAME} PRIVATE pthread Seasocks::seasocks ${PROJECT_NAME}_pnml_parser concurrentqueue readerwriterqueue)

#an app
add_executable(${PROJECT_NAME}_app examples/ros_application/main.cc examples/ros_application/namespace_share_data.cc)
target_link_libraries(${PROJECT_NAME}_app ${PROJECT_NAME} ${catkin_LIBRARIES} concurrentqueue readerwriterqueue)

#an app
add_executable(${PROJECT_NAME}_hello_world examples/hello_world/main.cc)
target_link_libraries(${PROJECT_NAME}_hello_world ${PROJECT_NAME} concurrentqueue readerwriterqueue)

set(ui_files
  ${CMAKE_CURRENT_SOURCE_DIR}/Symmetri/web
)

set(net_files   
  ${CMAKE_CURRENT_SOURCE_DIR}/nets
)

set(targets
  ${PROJECT_NAME}_hello_world
  ${PROJECT_NAME}_app
  ${PROJECT_NAME}
)

set_target_properties(${PROJECT_NAME}_app PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
# SET(CMAKE_INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/install")

install(DIRECTORY ${ui_files} Symmetri/include DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/install)
install(DIRECTORY ${net_files} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/install/web)
install(TARGETS ${targets} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/install)

# if(BUILD_TESTING)
# Enable testing for the project
enable_testing()
# concat testfiles 
set(TEST_SOURCES
catch.cc
tests.cc
)

# add_executable(${PROJECT_NAME}_test ${TEST_SOURCES})
# add_test(${PROJECT_NAME}_test ${PROJECT_NAME}_test)
# endif()