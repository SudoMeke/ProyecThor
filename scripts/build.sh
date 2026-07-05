#!/usr/bin/env bash
set -e
PLATFORM="$1"

case "$PLATFORM" in
    windows)
        cmake -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE="$(pwd)/toolchain-mingw.cmake"
        cmake --build build-win
        ;;
    linux)
        cmake -B build-linux -G Ninja
        cmake --build build-linux
        ;;
    windows-run)
        cmake -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE="$(pwd)/toolchain-mingw.cmake"
        cmake --build build-win
        cd build-win
        wine ProyecThor.exe
        ;;
    *)
        echo "Plataforma desconocida: $PLATFORM"
        exit 1
        ;;
esac