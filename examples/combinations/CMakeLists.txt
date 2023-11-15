#an app
add_executable(symmetri_composition composition.cc)
target_link_libraries(symmetri_composition symmetri)
add_executable(symmetri_nest nest.cc)
target_link_libraries(symmetri_nest symmetri)
