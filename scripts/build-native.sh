#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake_bin=$(command -v cmake || true)
if [ -z "$cmake_bin" ] && [ -x /opt/homebrew/bin/cmake ]; then cmake_bin=/opt/homebrew/bin/cmake; fi
if [ -z "$cmake_bin" ]; then
  echo '请先安装 CMake：brew install cmake；图形依赖由固定版本源码构建。' >&2
  exit 1
fi
"$cmake_bin" -S "$project_dir" -B "$project_dir/build/native" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DH3_VENDOR_DEPENDENCIES=ON -DBUILD_TESTING=ON
"$cmake_bin" --build "$project_dir/build/native" -j 4
"$cmake_bin" --build "$project_dir/build/native" --target test
