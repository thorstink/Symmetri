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

include_directories(include ${catkin_INCLUDE_DIRS}  )

#an app
add_executable(${PROJECT_NAME}_app main.cc namespace_share_data.cc)
target_link_libraries(${PROJECT_NAME}_app Symmetri ${catkin_LIBRARIES})

#install
install(TARGETS ${PROJECT_NAME}_app DESTINATION ${PROJECT_SOURCE_DIR}/install)

