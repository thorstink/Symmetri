# an app
find_package(spdlog REQUIRED)

if(ASAN_BUILD AND NOT TSAN_BUILD)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address,undefined -fno-omit-frame-pointer -O0")
elseif(TSAN_BUILD AND NOT ASAN_BUILD)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=thread -fno-omit-frame-pointer -O0")
endif()

add_executable(${PROJECT_NAME}_flight flight.cc)
target_link_libraries(${PROJECT_NAME}_flight symmetri spdlog::spdlog)
