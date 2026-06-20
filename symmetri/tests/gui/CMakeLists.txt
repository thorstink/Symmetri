
include(CTest)

set(GUI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../gui)

# Core imgui — no backend/display required for logic tests.
set(IMGUI_SOURCES
  ${GUI_SRC}/imgui/imgui.cpp
  ${GUI_SRC}/imgui/imgui_draw.cpp
  ${GUI_SRC}/imgui/imgui_widgets.cpp
  ${GUI_SRC}/imgui/imgui_tables.cpp
)

add_executable(${PROJECT_NAME}_farbart_doctest
  tests.cpp
  ${GUI_SRC}/model.cpp
  ${GUI_SRC}/reducers.cpp
  ${GUI_SRC}/rxdispatch.cpp
  ${IMGUI_SOURCES}
)

# The parent directories already set directory-level include_directories for all
# Farbart-specific paths (rpp, imgui, externals, …). Add the ones the test
# file needs that aren't covered by the source-relative search (which the
# Farbart sources rely on for draw_*.h etc.).
target_include_directories(${PROJECT_NAME}_farbart_doctest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..   # symmetri/tests/ — for doctest/doctest.h
  ${GUI_SRC}                       # symmetri/gui/ — draw_*.h, model.h, …
)

target_link_libraries(${PROJECT_NAME}_farbart_doctest PRIVATE static_symmetri pthread)
add_test(NAME ${PROJECT_NAME}_farbart_doctest COMMAND ${PROJECT_NAME}_farbart_doctest)
