cmake_minimum_required(VERSION 3.10)
project(symmetri VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -O2")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer -Wno-maybe-uninitialized -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -O0")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=thread -fno-omit-frame-pointer -Wno-maybe-uninitialized -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -O0")

if(BUILD_TESTING)
  enable_testing()
endif()

# the lib
add_subdirectory(symmetri)

# some examples using the lib
# if(BUILD_EXAMPLES)
    add_subdirectory(examples/flight)
    add_subdirectory(examples/hello_world)
# endif()

set(CMAKE_EXPORT_PACKAGE_REGISTRY ON)
export(TARGETS ${PROJECT_NAME} concurrentqueue NAMESPACE ${PROJECT_NAME}:: FILE "${PROJECT_NAME}Config.cmake" EXPORT_LINK_INTERFACE_LIBRARIES)
export(PACKAGE ${PROJECT_NAME})
