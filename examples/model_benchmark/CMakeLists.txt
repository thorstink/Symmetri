#an app
find_package(spdlog REQUIRED)

add_executable(${PROJECT_NAME}_benchmark model_benchmark.cc)
target_link_libraries(${PROJECT_NAME}_benchmark symmetri spdlog::spdlog)

#install
install(TARGETS ${PROJECT_NAME}_benchmark DESTINATION ${PROJECT_SOURCE_DIR}/install)
