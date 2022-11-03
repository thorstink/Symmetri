#an app
add_executable(${PROJECT_NAME}_flight flight.cc)
target_link_libraries(${PROJECT_NAME}_flight symmetri)

#install
install(TARGETS ${PROJECT_NAME}_flight DESTINATION ${PROJECT_SOURCE_DIR}/install)
