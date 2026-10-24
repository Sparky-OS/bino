#!/bin/bash
set -eu
export HOME=/work/home XDG_RUNTIME_DIR=/work/probe-runtime LANG=C.UTF-8 QT_QPA_PLATFORM=offscreen
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
vainfo --display drm --device /dev/dri/renderD128 > /work/evidence/vainfo.txt 2>&1
for kind in sw vaapi copy; do
 args=()
 if [[ $kind != sw ]]; then args=(-hwaccel vaapi -hwaccel_device /dev/dri/renderD128 -hwaccel_output_format vaapi); fi
 filter=()
 if [[ $kind == copy ]]; then filter=(-vf hwdownload,format=nv12); fi
 ffmpeg -hide_banner -nostdin -benchmark -threads 4 "${args[@]}" -i /work/clips/test-hsbs.mkv "${filter[@]}" -an -f null - > "/work/evidence/decode-$kind.log" 2>&1
done
for decode in sw vaapi; do
 for mapping in none map; do
  case_dir=/work/evidence/sink-$decode-$mapping${RUN_SUFFIX:-}
  test ! -e "$case_dir"
  mkdir -p "$case_dir"
  export QT_FFMPEG_DECODING_HW_DEVICE_TYPES=,
  if [[ $decode == vaapi ]]; then export QT_FFMPEG_DECODING_HW_DEVICE_TYPES=vaapi; fi
  export BINO_FRAME_TRACE="$case_dir/trace.csv" BINO_TRANSFER_TRACE="$case_dir/transfers.csv"
  export LD_PRELOAD=/work/probes/libtransfer-trace.so
  export QT_LOGGING_RULES='qt.multimedia.ffmpeg.*=true'
  /work/probes/sink-probe /work/clips/test-hsbs.mkv "$mapping" > "$case_dir/app.log" 2>&1
 done
done
