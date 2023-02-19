#an app
find_package(spdlog REQUIRED)

add_executable(${PROJECT_NAME}_hello_world main.cc)
target_link_libraries(${PROJECT_NAME}_hello_world symmetri spdlog::spdlog)

#install
install(TARGETS ${PROJECT_NAME}_hello_world DESTINATION ${PROJECT_SOURCE_DIR}/install)
