#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RKMPP_ROOT="${RKMPP_ROOT:-/home/linaro/sysroots/rk3588-mpp}"
CXX="${CXX:-/usr/bin/aarch64-linux-gnu-g++}"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-rk3588-mpp-check}"
SOURCE="${ROOT}/tools/rk3588_mpp_compile_check.cpp"

# Windows checkout uses a repository root above HwaSim_IR/HwaSim_IR, while the
# existing CLion deployment maps that CMake subproject directly to the remote
# project root.  Accept both layouts so the same check compiles the production
# encoder source in either environment.
if [[ -f "${ROOT}/HwaSim_IR/HwaSim_IR/Video/H264MppEncoder.cpp" ]]; then
    PRODUCTION_ROOT="${ROOT}/HwaSim_IR/HwaSim_IR"
elif [[ -f "${ROOT}/Video/H264MppEncoder.cpp" ]]; then
    PRODUCTION_ROOT="${ROOT}"
else
    echo "[RKMPPCompileCheck] FAIL production source root not found under ${ROOT}" >&2
    exit 2
fi

PRODUCTION_SOURCE="${PRODUCTION_ROOT}/Video/H264MppEncoder.cpp"
VIDEO_INCLUDE="${PRODUCTION_ROOT}/Video"
HEADER="${RKMPP_ROOT}/usr/include/rockchip/rk_mpi.h"
LIBRARY="${RKMPP_ROOT}/usr/lib/aarch64-linux-gnu/librockchip_mpp.so"
OUTPUT="${BUILD_DIR}/rk3588_mpp_compile_check"

for required in "${CXX}" "${SOURCE}" "${PRODUCTION_SOURCE}" "${HEADER}" "${LIBRARY}"; do
    if [[ ! -e "${required}" ]]; then
        echo "[RKMPPCompileCheck] FAIL missing=${required}" >&2
        exit 2
    fi
done

mkdir -p "${BUILD_DIR}"
"${CXX}" \
    -std=c++14 -O2 -Wall -Wextra -Werror \
    -DHWASIMIR_HAS_RKMPP=1 \
    -I"${VIDEO_INCLUDE}" \
    -I"${RKMPP_ROOT}/usr/include/rockchip" \
    "${SOURCE}" "${PRODUCTION_SOURCE}" \
    -L"${RKMPP_ROOT}/usr/lib/aarch64-linux-gnu" \
    -Wl,-rpath-link,"${RKMPP_ROOT}/usr/lib/aarch64-linux-gnu" \
    -lrockchip_mpp \
    -o "${OUTPUT}"

machine="$("${CXX}" -dumpmachine)"
file_text="$(file "${OUTPUT}")"
if [[ "${machine}" != aarch64-* ]] || [[ "${file_text}" != *"ARM aarch64"* ]]; then
    echo "[RKMPPCompileCheck] FAIL compiler=${machine} file=${file_text}" >&2
    exit 3
fi

echo "[RKMPPCompileCheck] PASS"
echo "compiler=${CXX}"
echo "target=${machine}"
echo "header=${HEADER}"
echo "library=${LIBRARY}"
echo "binary=${OUTPUT}"
echo "productionSource=${PRODUCTION_SOURCE}"
echo "cmakeOptions=-DHWASIMIR_ENABLE_RKMPP=ON -DRKMPP_ROOT=${RKMPP_ROOT} -DHWASIMIR_ENABLE_FFMPEG=OFF"
echo "note=run this binary only on RK3588 with /dev/mpp_service and the matching librockchip_mpp.so"
