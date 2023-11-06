find_package(spdlog REQUIRED)

#an app
add_executable(symmetri_composition composition.cc)
target_link_libraries(symmetri_composition symmetri spdlog::spdlog)
add_executable(symmetri_nest nest.cc)
target_link_libraries(symmetri_nest symmetri spdlog::spdlog)
