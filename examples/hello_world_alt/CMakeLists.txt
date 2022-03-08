#an app
add_executable(${PROJECT_NAME}_hello_world_alt main.cc)
target_link_libraries(${PROJECT_NAME}_hello_world_alt Symmetri)

#install
install(TARGETS ${PROJECT_NAME}_hello_world_alt DESTINATION ${PROJECT_SOURCE_DIR}/install)
