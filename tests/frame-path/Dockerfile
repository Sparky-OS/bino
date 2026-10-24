FROM a10-hwdec-regression-234-qt:1
COPY sparky-stereo-repo.asc /pub/sparky-stereo-repo.asc
RUN rm -f /etc/apt/sources.list.d/multimedia.list \
 && sed -i 's|file:/pub|http://repo.sparkyos.org|' /etc/apt/sources.list.d/edition.list \
 && DEBIAN_FRONTEND=noninteractive apt-get -y update \
 && DEBIAN_FRONTEND=noninteractive apt-get -y install --no-install-recommends kwin-wayland xwayland dbus-x11 qt6-l10n-tools qt6-tools-dev mesa-utils x11-utils xdotool libqt6multimedia6 libqt6multimediawidgets6 libgl1-mesa-dri mesa-va-drivers vainfo libstereo-layer-dev \
 && rm -rf /var/lib/apt/lists/*
