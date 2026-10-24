FROM debian:testing
COPY sparky-stereo-repo.asc /usr/share/keyrings/sparky-stereo-repo.asc
COPY deb-multimedia-keyring.pgp /usr/share/keyrings/deb-multimedia-keyring.pgp
RUN printf 'deb [signed-by=/usr/share/keyrings/sparky-stereo-repo.asc] http://repo.sparkyos.org mashtabba main\n' > /etc/apt/sources.list.d/edition.list \
 && printf 'Package: *\nPin: release o=Sparky Stereo OS\nPin-Priority: 1001\n' > /etc/apt/preferences.d/edition \
 && printf 'deb [signed-by=/usr/share/keyrings/deb-multimedia-keyring.pgp] http://www.deb-multimedia.org testing main non-free\n' > /etc/apt/sources.list.d/multimedia.list \
 && DEBIAN_FRONTEND=noninteractive apt-get -y update \
 && DEBIAN_FRONTEND=noninteractive apt-get -y install --no-install-recommends build-essential cmake ninja-build pkg-config qt6-base-dev qt6-multimedia-dev qt6-l10n-tools qt6-tools-dev libstereo-layer-dev kwin-wayland xwayland dbus-x11 mesa-utils x11-utils xdotool libgl1-mesa-dri mesa-va-drivers vainfo ffmpeg python3 python3-pil python3-numpy \
 && rm -rf /var/lib/apt/lists/*
