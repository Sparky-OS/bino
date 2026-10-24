cmake_minimum_required(VERSION 3.16)
project(BinoFramePath LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Multimedia Gui)
include(../../cmake/StereoDesktop.cmake)
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
target_link_libraries(widget-probe PRIVATE Qt6::OpenGLWidgets Qt6::Multimedia Stereo::Layer EGL GL)
qt_add_resources(widget-probe "fixture-resources" PREFIX "/" BASE ../.. FILES
    ../../src/shader-color.vert.glsl ../../src/shader-color.frag.glsl
    ../../src/shader-view.vert.glsl ../../src/shader-view.frag.glsl
    ../../src/shader-display.vert.glsl ../../${BINO_DISPLAY_SHADER}
    ../../res/bino-logo-small.svg ../../res/bino-logo-small-512.png
    ../../res/bino-fallback-frame.png)

if(BINO_STEREO_DESKTOP)
    bino_native_video(widget-probe ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

find_package(Qt6 REQUIRED COMPONENTS DBus)
add_executable(screen-capture capture.cpp)
target_link_libraries(screen-capture PRIVATE Qt6::Gui Qt6::DBus)

get_target_property(PROBE_SOURCES widget-probe SOURCES)
list(REMOVE_ITEM PROBE_SOURCES widget.cpp native-target.cpp)
add_executable(output-modes-probe output-modes.cpp ${PROBE_SOURCES})
target_include_directories(output-modes-probe PRIVATE ../../src)
target_compile_definitions(output-modes-probe PRIVATE HAVE_STEREO_LAYER)
target_link_libraries(output-modes-probe PRIVATE Qt6::OpenGLWidgets Qt6::Multimedia Stereo::Layer)
if(BINO_STEREO_DESKTOP)
    target_include_directories(output-modes-probe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(output-modes-probe PRIVATE PkgConfig::NativeVideo PkgConfig::NativeDeclaration PkgConfig::NativeContent Qt6::GuiPrivate Qt6::MultimediaPrivate)
endif()

add_executable(deferred-probe deferred.cpp ../../src/videoframe.cpp ../../src/modes.cpp ../../src/log.cpp)
target_include_directories(deferred-probe PRIVATE ../../src)
target_link_libraries(deferred-probe PRIVATE Qt6::Multimedia Qt6::Gui)

if(BINO_STEREO_DESKTOP)
    add_executable(texture-lifetime-probe texture-lifetime.cpp ../../src/videotextureimporter.cpp)
    target_include_directories(texture-lifetime-probe PRIVATE ../../src)
    target_link_libraries(texture-lifetime-probe PRIVATE Qt6::GuiPrivate Qt6::MultimediaPrivate)
endif()

add_executable(buttons-probe buttons.cpp ${PROBE_SOURCES})
target_include_directories(buttons-probe PRIVATE ../../src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(buttons-probe PRIVATE HAVE_STEREO_LAYER)
target_link_libraries(buttons-probe PRIVATE Qt6::OpenGLWidgets Qt6::Multimedia Stereo::Layer)
if(BINO_STEREO_DESKTOP)
    target_link_libraries(buttons-probe PRIVATE PkgConfig::NativeVideo PkgConfig::NativeDeclaration PkgConfig::NativeContent Qt6::GuiPrivate Qt6::MultimediaPrivate)
endif()
