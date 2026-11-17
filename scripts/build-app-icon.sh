#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_icon="$project_dir/native/assets/app-icon.png"
iconset="$project_dir/native/macos/Assets/HeroesIII.iconset"
mkdir -p "$iconset"
for points in 16 32 128 256 512; do
  /usr/bin/sips -z "$points" "$points" "$source_icon" --out "$iconset/icon_${points}x${points}.png" >/dev/null
  pixels=$((points * 2))
  /usr/bin/sips -z "$pixels" "$pixels" "$source_icon" --out "$iconset/icon_${points}x${points}@2x.png" >/dev/null
done
/usr/bin/iconutil -c icns "$iconset" -o "$project_dir/native/macos/Assets/HeroesIII.icns"
echo 'Built HeroesIII.icns with 16–1024 pixel representations.'
