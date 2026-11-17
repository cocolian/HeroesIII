#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
app_binary="$project_dir/build/native/HeroesIII.app/Contents/MacOS/HeroesIII"
if [ ! -x "$app_binary" ]; then "$project_dir/scripts/build-native.sh"; fi
exec "$app_binary" --data-root "$project_dir" "$@"
