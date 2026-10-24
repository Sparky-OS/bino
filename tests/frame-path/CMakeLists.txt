cmake_minimum_required(VERSION 3.16)
project(BinoFramePath LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Multimedia Gui)
add_executable(sink-probe sink.cpp ../../src/videoframe.cpp ../../src/modes.cpp ../../src/log.cpp)
target_include_directories(sink-probe PRIVATE ../../src)
target_link_libraries(sink-probe PRIVATE Qt6::Multimedia Qt6::Gui)
add_library(transfer-trace SHARED transfer.c)
target_link_libraries(transfer-trace PRIVATE dl)

set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
find_package(StereoLayer REQUIRED CONFIG)
add_executable(widget-probe widget.cpp native-target.cpp
    ../../src/log.cpp ../../src/tools.cpp ../../src/screen.cpp ../../src/modes.cpp
    ../../src/metadata.cpp ../../src/playlist.cpp ../../src/videoframe.cpp
    ../../src/videosink.cpp ../../src/bino.cpp ../../src/widget.cpp
    ../../src/commandinterpreter.cpp ../../src/overlay.cpp ../../src/overlay-audio.cpp
    ../../src/overlay-subtitle.cpp ../../src/overlay-ui.cpp
    ../../src/digestiblemedia.cpp ../../src/urlloader.cpp
    ../../src/gui.cpp ../../src/playlisteditor.cpp)
target_include_directories(widget-probe PRIVATE ../../src)
target_compile_definitions(widget-probe PRIVATE HAVE_STEREO_LAYER)
target_link_libraries(widget-probe PRIVATE Qt6::OpenGLWidgets Qt6::Multimedia Stereo::Layer)
qt_add_resources(widget-probe "fixture-resources" PREFIX "/" BASE ../.. FILES
    ../../src/shader-color.vert.glsl ../../src/shader-color.frag.glsl
    ../../src/shader-view.vert.glsl ../../src/shader-view.frag.glsl
    ../../src/shader-display.vert.glsl ../../src/shader-display.frag.glsl
    ../../res/bino-logo-small.svg ../../res/bino-logo-small-512.png
    ../../res/bino-fallback-frame.png)

include(../../cmake/NativeVideo.cmake)
bino_native_video(widget-probe ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(Qt6 REQUIRED COMPONENTS DBus)
add_executable(screen-capture capture.cpp)
target_link_libraries(screen-capture PRIVATE Qt6::Gui Qt6::DBus)
