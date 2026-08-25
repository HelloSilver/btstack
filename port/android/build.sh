#!/usr/bin/env bash
# Build the Android NDK port for arm64-v8a (override ABI via ANDROID_ABI).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ABI="${ANDROID_ABI:-arm64-v8a}"
API="${ANDROID_PLATFORM:-android-34}"
BUILD_DIR="${SCRIPT_DIR}/build-${ABI}"

if [[ -z "${ANDROID_NDK:-}" ]]; then
    for candidate in \
        "${ANDROID_NDK_HOME:-}" \
        /opt/android-ndk-r27d \
        /opt/android-ndk-r27c \
        "${HOME}/Android/Sdk/ndk/27.2.12479018"
    do
        if [[ -n "${candidate}" && -f "${candidate}/build/cmake/android.toolchain.cmake" ]]; then
            ANDROID_NDK="${candidate}"
            break
        fi
    done
fi

if [[ -z "${ANDROID_NDK:-}" || ! -f "${ANDROID_NDK}/build/cmake/android.toolchain.cmake" ]]; then
    echo "Set ANDROID_NDK to an Android NDK (r26+ recommended)." >&2
    exit 1
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="${ABI}" \
    -DANDROID_PLATFORM="${API}" \
    -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DIR}" --target gap_le_advertisements gatt_counter btstack_android -j"$(nproc)"
echo "Built ${BUILD_DIR}/gap_le_advertisements"
echo "Built ${BUILD_DIR}/libbtstack_android.so"
