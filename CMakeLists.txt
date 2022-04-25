cmake_minimum_required(VERSION 3.10)
project(Symmetri VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -Wno-error=deprecated-enum-enum-conversion")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -march=native -O2 -Wno-error=deprecated-enum-enum-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized -g -fsanitize=address -fno-omit-frame-pointer -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -O0 -Wno-error=deprecated-enum-enum-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized -g -fsanitize=address -fno-omit-frame-pointer -Wno-psabi -Wno-unused-parameter -Werror -Wall -Wextra -O0 -Wno-error=deprecated-enum-enum-conversion")

enable_testing()

# the lib
add_subdirectory(Symmetri)

# some examples using the lib
# add_subdirectory(examples/ros_application)
add_subdirectory(examples/flight)
add_subdirectory(examples/hello_world)
add_subdirectory(examples/hello_world_alt)
