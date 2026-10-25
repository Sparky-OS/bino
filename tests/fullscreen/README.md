# Fullscreen pointer regression

Run case.sh in an isolated KWin virtual-output container with four CPUs and a render node.
The probe constructs the real Gui and Widget, using the same OpenGL initialization as Bino.
The driver uses KWin's fake-input protocol, not direct calls to the fullscreen slot or mouse handler.
The permission-check bypass is confined to the isolated compositor.

Compare F11 and two actual left clicks in the video, then in the ordinary overlay panel, and compare both return transitions.
Read window state from Qt and client placement from KWin; Wayland does not expose the top-level global position to Qt.
Include Qt's client-side decoration margins when sending absolute compositor input.
A white 1920x1080 SAR 2:1 marked SBS fixture must cover every pixel edge of both 1280x720 screen views in fullscreen.
No source film is needed.
Single clicks on the existing play/pause button must pause and resume.
Double-clicks on the video and overlay panel must retain playback state.
Repeat with mono, the ordinary Qt fallback, and Xwayland.

The recorded MouseButtonDblClick must reach Widget; merely maximizing the window is a failure.
The accepted pre-fix player is the negative control.
