find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NativeVideo REQUIRED IMPORTED_TARGET wayland-client wayland-egl egl gl xext)
pkg_check_modules(NativeDeclaration REQUIRED IMPORTED_TARGET stereo-declare>=1.7)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
set(VIEWPORTER_XML "${WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml")
add_custom_command(OUTPUT wayland-viewporter-client-protocol.h wayland-viewporter-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header ${VIEWPORTER_XML} wayland-viewporter-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code ${VIEWPORTER_XML} wayland-viewporter-protocol.c
    DEPENDS ${VIEWPORTER_XML} VERBATIM)
function(bino_native_video target source_root)
    target_sources(${target} PRIVATE ${source_root}/src/nativevideolayer.cpp ${source_root}/src/nativex11layer.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/wayland-viewporter-protocol.c)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${target} PRIVATE PkgConfig::NativeVideo PkgConfig::NativeDeclaration Qt6::GuiPrivate)
endfunction()
