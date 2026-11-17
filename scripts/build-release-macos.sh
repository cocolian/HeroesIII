#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
release_build="$project_dir/build/distribution"
cmake -S "$project_dir" -B "$release_build" -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DH3_VENDOR_DEPENDENCIES=ON -DH3_USE_SOURCE_ASSETS=OFF -DBUILD_TESTING=ON
python3 "$project_dir/scripts/import-community-scenarios.py"
python3 "$project_dir/scripts/generate-map-library.py"
python3 "$project_dir/scripts/generate-interface-colors.py"
python3 "$project_dir/scripts/stage-runtime.py" "$project_dir/build/staged-runtime/GameData"
cmake -S "$project_dir" -B "$release_build" -DH3_RUNTIME_DATA_DIR="$project_dir/build/staged-runtime/GameData"
cmake --build "$release_build" -j 6
ctest --test-dir "$release_build" --output-on-failure
python3 "$project_dir/scripts/verify-bundle.py" "$release_build/HeroesIII.app"
python3 "$project_dir/scripts/package-macos.py"
