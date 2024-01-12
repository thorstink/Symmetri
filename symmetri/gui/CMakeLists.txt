cmake_minimum_required(VERSION 3.26)
project(Farbart)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(glfw3 REQUIRED)
find_package(ogdf REQUIRED)

if(ASAN_BUILD AND NOT TSAN_BUILD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address,undefined -fno-omit-frame-pointer -O0")
elseif(TSAN_BUILD AND NOT ASAN_BUILD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=thread -fno-omit-frame-pointer -O0")
endif()

include_directories(
    rpp
    imgui
    imgui/backends
    ../externals
    gui
    extensions
    /usr/local/include
)

set(SOURCES
    main.mm
    redux.cpp
    symbol.cpp
    draw_graph.cpp
)

set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_metal.mm

    # extensions/crude_json.cpp
    # extensions/imgui_node_editor_api.cpp
    # extensions/imgui_node_editor.cpp
    # extensions/imgui_canvas.cpp
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
target_link_libraries(Farbart symmetri OGDF ${EXTRA_LIBS})
