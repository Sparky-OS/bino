<picture>
  <source media="(prefers-color-scheme: dark)" srcset=".github/sparky-stereo-os/sparkyos-swirl-light-128.png">
  <img src=".github/sparky-stereo-os/sparkyos-swirl-dark-128.png" alt="SparkyOS" width="128" height="128">
</picture>

## Sparky Stereo OS

This is the stereo version of Bino by Sparky Stereo OS, forked from [marlam/bino](https://github.com/marlam/bino).
It is the distribution's Bino: it decodes every 3D input layout Bino knows, and hands each eye at its source resolution to the desktop as one declared full side-by-side picture; Stereo KWin then gives each screen what it needs (a 3D TV's HDMI 1.4a mode, anaglyph, or the left eye on a plain screen), so the output modes are not in its menu.
It also plays right-first stereo videos with the eyes in the right place: the "inverted" flag of the Stereo 3D side data, which ffprobe prints as a number, was never read, so Matroska StereoMode 11 and 2 (RFC 9559, right eye first) showed the eyes swapped.
The edition builds with `-DBINO_STEREO_DESKTOP=ON` (the default in this fork).
This keeps video in its own native surface and subtitles and controls in the ordinary 2D window.
Its settings live in their own folder, Bino3D-stereo, beside those of a stock Bino.
Build with `-DBINO_STEREO_DESKTOP=OFF` to retain upstream's output modes, menu actions, command-line options, conversions, shaders, manual and Bino3D settings.
That build does not require the native Wayland/X11 surface dependencies.

Where it comes from:

- [Bino](https://bino3d.org/) is written by Martin Lambers (the copyright in `src/main.cpp`).
- [Debian](https://www.debian.org/) is the base of the system.
- [SparkyLinux](https://sparkylinux.org/), by Paweł "pavroo" Pijanowski, builds on Debian.
- [Sparky Stereo OS](https://github.com/Sparky-OS/sparky-stereo-os) is the stereo 3D edition of SparkyLinux: SparkyOS, powered by Debian.

The `stereo3d` branch holds the version the distribution builds (`bino 2.8-1+stereo3d1`), on Bino 2.8.
The licence is unchanged; see [LICENSE.md](LICENSE.md).

Sparky Stereo OS, Daniel Ramos's edition of SparkyLinux (by Paweł "pavroo" Pijanowski).

---

# Bino: a 3D video player

Bino is a video player with a focus on 3D and Virtual Reality:

- Support for 3D images and videos in various formats

- Support for 180° and 360° surround images and videos, with and without 3D

- Support for 3D displays with various modes

- Support for Virtual Reality environments, including SteamVR, CAVEs,
  powerwalls, and other multi-display / multi-GPU / multi-host systems

Bino is based on [Qt](https://www.qt.io/). The optional Virtual Reality
and multi-GPU support is based on [QVR](https://marlam.de/qvr/). No other
libraries are required.

See [bino3d.org](https://bino3d.org/) and the [manual](https://bino3d.org/bino-manual.html).
