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

include_directories(include submodules ${SEASOCKS_INCLUDE_DIRS})

#lib
add_library(${PROJECT_NAME} 
  actions.cpp 
  symmetri.cc 
  model.cc 
  pnml_parser.cc 
  retry.cpp 
  ws_interface.cpp 
  types.cpp 
  submodules/tinyxml2/tinyxml2.cpp
  )

target_link_libraries(${PROJECT_NAME} PRIVATE pthread Seasocks::seasocks spdlog::spdlog concurrentqueue)
target_include_directories(${PROJECT_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

set(ui_files
  ${CMAKE_CURRENT_SOURCE_DIR}/web
)

set(targets
  ${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include
  DESTINATION include
)

install(DIRECTORY ${ui_files} DESTINATION share)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
