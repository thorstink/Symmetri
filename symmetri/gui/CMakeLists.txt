cmake_minimum_required(VERSION 3.26)
project(Farbart)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(glfw3 REQUIRED)

include_directories(imgui imgui/backends ../externals gui)

set(SOURCES
    main.mm
)

set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_metal.mm
    crude_json.cpp
    imgui_node_editor_api.cpp
    imgui_node_editor.cpp
    imgui_canvas.cpp
    )

if(APPLE)
  include_directories(/opt/homebrew/include)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(OPENGL_LIBRARY OpenGL)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(COREVIDEO_LIBRARY CoreVideo)
  set(EXTRA_LIBS
    ${COCOA_LIBRARY}
    ${OPENGL_LIBRARY}
    ${IOKIT_LIBRARY}
    ${COREVIDEO_LIBRARY}
    glfw
    "-framework Metal"
    "-framework MetalKit"
    "-framework AppKit"
    "-framework Foundation"
    "-framework QuartzCore"
    )
endif(APPLE)

add_executable(Farbart ${SOURCES} ${IMGUI_SOURCES})
target_link_libraries(Farbart symmetri ${EXTRA_LIBS} )
