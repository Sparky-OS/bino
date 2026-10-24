#!/bin/sh
set -eu
export HOME=/work/home
export DEBEMAIL=Capitain_Jack@yahoo.com DEBFULLNAME='Daniel Ramos (Capitain Jack)'
mkdir -p /work/home /work/evidence /work/clips
exec > /work/evidence/build.log 2>&1
cmake -S /work/bino -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPANDOC=NOTFOUND
cmake --build /work/build -j4
cmake -S /work/bino/tests/frame-path -B /work/probes -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /work/probes -j4
dpkg-query -W libqt6multimedia6 libqt6openglwidgets6 libgl1-mesa-dri kwin-wayland xwayland ffmpeg > /work/evidence/versions.txt
ffmpeg -hide_banner -nostdin -y -f lavfi -i 'testsrc2=size=1920x1080:rate=24000/1001' -frames:v 288 -c:v libx264 -threads 4 -preset fast -crf 20 -x264-params frame-packing=3 -metadata:s:v stereo_mode=left_right /work/clips/test-hsbs.mkv > /work/evidence/fixture.log 2>&1
ffprobe -v error -show_streams -show_format -of json /work/clips/test-hsbs.mkv > /work/evidence/fixture.json
