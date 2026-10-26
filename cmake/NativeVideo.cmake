find_package(Qt6 REQUIRED COMPONENTS GuiPrivate MultimediaPrivate)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NativeVideo REQUIRED IMPORTED_TARGET wayland-client wayland-egl egl gl xext)
pkg_check_modules(NativeDeclaration REQUIRED IMPORTED_TARGET stereo-declare>=1.6)
pkg_check_modules(NativeContent REQUIRED IMPORTED_TARGET stereo-av>=1.12)
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES PkgConfig::NativeDeclaration)
check_cxx_source_compiles("#include <stereo-declare.h>
int main() {
    stereo_x11_child_place place = {0, 0, 1, 1, 1};
    return stereo_declare_x11_child_place(nullptr, 0, &place);
}" BINO_HAVE_X11_CHILD_PLACE)
cmake_pop_check_state()
add_compile_definitions(BINO_HAVE_X11_CHILD_PLACE=$<BOOL:${BINO_HAVE_X11_CHILD_PLACE}>)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES Qt6::Multimedia)
check_cxx_source_compiles("#include <QVideoSink>
int main() {
    QVideoSink sink;
    sink.setPreserveNativeSize(true);
    return !sink.preserveNativeSize();
}" BINO_HAVE_NATIVE_VIDEO_SIZE)
cmake_pop_check_state()
if(NOT BINO_HAVE_NATIVE_VIDEO_SIZE)
    message(FATAL_ERROR "The stereo desktop build needs Qt Multimedia's native-raster sink capability")
endif()

find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
set(VIEWPORTER_XML "${WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml")
add_custom_command(OUTPUT wayland-viewporter-client-protocol.h wayland-viewporter-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header ${VIEWPORTER_XML} wayland-viewporter-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${VIEWPORTER_XML} wayland-viewporter-protocol.c
    DEPENDS ${VIEWPORTER_XML} VERBATIM)
function(bino_native_video target source_root)
    target_sources(${target} PRIVATE ${source_root}/src/videotextureimporter.cpp ${source_root}/src/nativevideolayer.cpp ${source_root}/src/nativex11layer.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/wayland-viewporter-protocol.c)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${target} PRIVATE PkgConfig::NativeVideo PkgConfig::NativeDeclaration PkgConfig::NativeContent Qt6::GuiPrivate Qt6::MultimediaPrivate)
endfunction()
