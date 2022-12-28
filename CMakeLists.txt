cmake_minimum_required(VERSION 3.10)
# Read version from the package.xml file.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/package.xml package_xml_str)
if(NOT package_xml_str MATCHES "<version>([0-9]+.[0-9]+.[0-9]+)</version>")
  message(FATAL_ERROR "Could not parse project version from package.xml. Aborting.")
endif()

project(symmetri VERSION ${CMAKE_MATCH_1})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-uninitialized -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -O2")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer -Wno-maybe-uninitialized -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -O0")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=thread -fno-omit-frame-pointer -Wno-maybe-uninitialized -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -O0")

if(BUILD_TESTING)
  enable_testing()
endif()

# the lib
add_subdirectory(symmetri)

# some examples using the lib
if(BUILD_EXAMPLES)
    add_subdirectory(examples/flight)
    add_subdirectory(examples/hello_world)
endif()
    