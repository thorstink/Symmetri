
cmake_minimum_required(VERSION 3.10)
project(mpalg VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")

find_package(Eigen3 REQUIRED)
find_package(Seasocks REQUIRED)

include_directories(${EIGEN3_INCLUDE_DIRS} ${SEASOCKS_INCLUDE_DIRS} )

add_library(${PROJECT_NAME}_parser application/parser.cc)

#app
add_executable(${PROJECT_NAME}_app application/main.cc application/model.cc application/functions.cc)
target_link_libraries(${PROJECT_NAME}_app ${PROJECT_NAME}_parser pthread Seasocks::seasocks)

#test
add_executable(${PROJECT_NAME}_parser_test application/test_parse.cc)
target_link_libraries(${PROJECT_NAME}_parser_test ${PROJECT_NAME}_parser)


set(ui_files
  ${CMAKE_CURRENT_SOURCE_DIR}/application/web
)
set(targets
  ${PROJECT_NAME}_app
  ${PROJECT_NAME}_parser
)

install(DIRECTORY ${ui_files} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/install)
install(TARGETS ${targets} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/install)

# if(BUILD_TESTING)
# Enable testing for the project
enable_testing()
# concat testfiles 
set(TEST_SOURCES
catch.cc
tests.cc
# legged_loco_test.cc
)

# add_executable(${PROJECT_NAME}_test ${TEST_SOURCES})
# add_test(${PROJECT_NAME}_test ${PROJECT_NAME}_test)
# endif()