
cmake_minimum_required(VERSION 3.10)
project(Systemetri VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -s")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wno-psabi  -march=native -O2 -s")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wno-psabi -march=native -O2 -s")

find_package(Eigen3 REQUIRED)
find_package(Seasocks REQUIRED)

include_directories(${EIGEN3_INCLUDE_DIRS} ${SEASOCKS_INCLUDE_DIRS} submodules .)

add_library(${PROJECT_NAME}_pnml_parser Systemetri/pnml_parser.cc submodules/tinyxml2/tinyxml2.cpp)

#lib
add_library(lib${PROJECT_NAME} Systemetri/systemetri.cc Systemetri/model.cc Systemetri/actions.cc)
target_link_libraries(lib${PROJECT_NAME} pthread Seasocks::seasocks ${PROJECT_NAME}_pnml_parser)

#an app
add_executable(${PROJECT_NAME}_app application/main.cc application/transitions.cc)
target_link_libraries(${PROJECT_NAME}_app lib${PROJECT_NAME})

set(ui_files
  ${CMAKE_CURRENT_SOURCE_DIR}/Systemetri/web
)
set(targets
  ${PROJECT_NAME}_app
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
)

# add_executable(${PROJECT_NAME}_test ${TEST_SOURCES})
# add_test(${PROJECT_NAME}_test ${PROJECT_NAME}_test)
# endif()