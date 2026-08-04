#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  rk3588_v4_deploy_acceptance.sh build
  rk3588_v4_deploy_acceptance.sh deploy <board-host> [board-dir]
  rk3588_v4_deploy_acceptance.sh run <network-config> [seconds]
  rk3588_v4_deploy_acceptance.sh verify <log-file>

Run "build" in the Debian VM checkout. "deploy" copies the executable and Bin
runtime assets to an RK3588 reachable over SSH. Run "run" on the RK3588 while
the matching UDP stimulus and Windows VideoDisplay endpoints are available.
EOF
}

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="${ROOT}/HwaSim_IR/HwaSim_IR"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-rk3588-v4-release}"
RKMPP_ROOT="${RKMPP_ROOT:-/home/linaro/sysroots/rk3588-mpp}"
PANDA3D_ROOT="${PANDA3D_ROOT:-/opt/panda3d-aarch64}"
OPENCV_DIR="${OpenCV_DIR:-/usr/lib/aarch64-linux-gnu/cmake/opencv4}"
ACTION="${1:-}"

build_project() {
    "${ROOT}/tools/rk3588_mpp_compile_check.sh"
    cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DHWASIMIR_ENABLE_RKMPP=ON \
        -DRKMPP_ROOT="${RKMPP_ROOT}" \
        -DHWASIMIR_ENABLE_FFMPEG=OFF \
        -DPANDA3D_ROOT="${PANDA3D_ROOT}" \
        -DOpenCV_DIR="${OPENCV_DIR}"
    cmake --build "${BUILD_DIR}" --config Release --parallel
    test -x "${BUILD_DIR}/HwaSim_IR"
    file "${BUILD_DIR}/HwaSim_IR"
    echo "[RK3588V4Build] PASS binary=${BUILD_DIR}/HwaSim_IR"
}

deploy_project() {
    local board_host="${1:?board host is required}"
    local board_dir="${2:-/home/linaro/HwaSimIR-v4}"
    test -x "${BUILD_DIR}/HwaSim_IR"
    ssh "${board_host}" "mkdir -p '${board_dir}'"
    rsync -a --delete \
        --exclude '*.exe' --exclude '*.dll' --exclude '*.pdb' \
        "${ROOT}/HwaSim_IR/Bin/" "${board_host}:${board_dir}/"
    rsync -a "${BUILD_DIR}/HwaSim_IR" "${board_host}:${board_dir}/HwaSim_IR"
    rsync -a "$0" "${board_host}:${board_dir}/rk3588_v4_deploy_acceptance.sh"
    echo "[RK3588V4Deploy] PASS host=${board_host} dir=${board_dir}"
    echo "Next on board: cd '${board_dir}' && ./rk3588_v4_deploy_acceptance.sh run <loopback-or-production-network.ini>"
}

run_board() {
    local network_config="${1:?network config is required}"
    local seconds="${2:-45}"
    local log_file="${V4_LOG_FILE:-v4-rk3588-$(date +%Y%m%d-%H%M%S).log}"
    test -e /dev/mpp_service
    ldconfig -p | grep -q 'librockchip_mpp\.so'
    test -f "${network_config}"
    export RenderPresentationMode="${RenderPresentationMode:-HeadlessOffscreen}"
    export EnableH264Experimental=1
    export H264Encoder=mpp
    export H264FallbackToJpeg=0
    export TcpPacketVersion=3
    export TcpSendVideo=1
    export TcpSendAnnotation=1
    export TcpSendRealtimeData=1
    export TcpForwardInitControl=1
    set +e
    timeout --signal=INT "${seconds}" \
        ./HwaSim_IR --network-config "${network_config}" 2>&1 | tee "${log_file}"
    status=${PIPESTATUS[0]}
    set -e
    if [[ ${status} -ne 0 && ${status} -ne 124 && ${status} -ne 130 ]]; then
        echo "[RK3588V4Run] FAIL exit=${status} log=${log_file}" >&2
        exit "${status}"
    fi
    "$0" verify "${log_file}"
}

verify_log() {
    local log_file="${1:?log file is required}"
    test -s "${log_file}"
    grep -q 'requestedBackend=mpp' "${log_file}"
    grep -q 'activeBackend=mpp' "${log_file}"
    grep -q 'activeCodec=h264_annexb' "${log_file}"
    grep -q '\[MppPerf\].*payloadBytes=[1-9]' "${log_file}"
    grep -q '\[TcpFramePacket\].*packetVersion=3.*flags=0x7' "${log_file}"
    if grep -q '\[CodecFallback\].*activeCodec=jpeg' "${log_file}"; then
        echo "[RK3588V4Verify] FAIL unexpected JPEG fallback" >&2
        exit 4
    fi
    echo "[RK3588V4Verify] PASS log=${log_file}"
    echo "Manual checks still required: VideoDisplay continuity/FPS/latency, TCP reconnect IDR, init/reset IDR, and long-run queue stability."
}

case "${ACTION}" in
    build)
        build_project
        ;;
    deploy)
        deploy_project "${2:-}" "${3:-/home/linaro/HwaSimIR-v4}"
        ;;
    run)
        run_board "${2:-}" "${3:-45}"
        ;;
    verify)
        verify_log "${2:-}"
        ;;
    *)
        usage
        exit 1
        ;;
esac
