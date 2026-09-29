#!/usr/bin/env bash

set -euo pipefail

readonly script_directory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly market_lab_source="$(cd "${script_directory}/.." && pwd)"
readonly default_obz_source="$(cd "${market_lab_source}/../.." && pwd)/ObzLib/repo"
readonly obz_source="${OBZ_SOURCE_DIR:-${default_obz_source}}"
readonly image="obz-linux-dev:ubuntu24.04"
readonly build_volume="obz-linux-build"

print_usage() {
    cat <<'EOF'
Usage: scripts/linux-dev.sh <command>

Commands:
  build-image       Build the Ubuntu development image
  shell             Open an interactive Linux shell with sources mounted
  test-market-gcc   Configure, build and test Market Lab with GCC
  test-market-clang Configure, build and test Market Lab with Clang
  run-market-multicast Build and run the multicast publisher and receiver

Set OBZ_SOURCE_DIR to override the default adjacent ObzLib checkout path.
EOF
}

require_obz_source() {
    if [[ ! -f "${obz_source}/CMakeLists.txt" ]]; then
        echo "ObzLib source not found at: ${obz_source}" >&2
        echo "Set OBZ_SOURCE_DIR to the ObzLib repository root." >&2
        exit 1
    fi
}

run_linux() {
    docker run --rm \
        --mount "type=bind,source=${obz_source},target=/workspace/obz,readonly" \
        --mount "type=bind,source=${market_lab_source},target=/workspace/market-lab,readonly" \
        --mount "type=volume,source=${build_volume},target=/build" \
        "${image}" \
        bash -lc "$1"
}

test_market() {
    local compiler="$1"
    local build_name="$2"

    run_linux "set -e
cmake -S /workspace/market-lab -B /build/${build_name} \\
  -G Ninja \\
  -DCMAKE_BUILD_TYPE=Debug \\
  -DCMAKE_CXX_COMPILER=${compiler} \\
  -DOBZ_MARKET_LAB_OBZ_SOURCE_DIR=/workspace/obz \\
  -DOBZ_MARKET_LAB_BUILD_TESTS=ON \\
  -DOBZ_MARKET_LAB_BUILD_APPS=ON
cmake --build /build/${build_name} --parallel 4
ctest --test-dir /build/${build_name} --output-on-failure --timeout 60"
}

run_market_multicast() {
    run_linux 'set -e
cmake -S /workspace/market-lab -B /build/market-gcc \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DOBZ_MARKET_LAB_OBZ_SOURCE_DIR=/workspace/obz \
  -DOBZ_MARKET_LAB_BUILD_TESTS=ON \
  -DOBZ_MARKET_LAB_BUILD_APPS=ON
cmake --build /build/market-gcc \
  --target obz_market_exchange_feed_simulator obz_market_data_listener \
  --parallel 4

receiver_log="$(mktemp)"
timeout 10 /build/market-gcc/apps/market_data_listener/obz_market_data_listener \
  >"${receiver_log}" 2>&1 &
receiver_pid=$!

cleanup() {
  kill "${receiver_pid}" 2>/dev/null || true
  rm -f "${receiver_log}"
}
trap cleanup EXIT

for _ in $(seq 1 50); do
  if grep -q "^listening " "${receiver_log}"; then
    break
  fi
  sleep 0.1
done

if ! grep -q "^listening " "${receiver_log}"; then
  cat "${receiver_log}"
  echo "receiver did not become ready" >&2
  exit 1
fi

/build/market-gcc/apps/exchange_feed_simulator/obz_market_exchange_feed_simulator

if ! wait "${receiver_pid}"; then
  cat "${receiver_log}"
  echo "receiver failed or timed out" >&2
  exit 1
fi

cat "${receiver_log}"'
}

case "${1:-}" in
    build-image)
        docker build -t "${image}" "${market_lab_source}/docker/linux-dev"
        ;;
    shell)
        require_obz_source
        docker run --rm -it \
            --mount "type=bind,source=${obz_source},target=/workspace/obz,readonly" \
            --mount "type=bind,source=${market_lab_source},target=/workspace/market-lab,readonly" \
            --mount "type=volume,source=${build_volume},target=/build" \
            "${image}"
        ;;
    test-market-gcc)
        require_obz_source
        test_market g++ market-gcc
        ;;
    test-market-clang)
        require_obz_source
        test_market clang++ market-clang
        ;;
    run-market-multicast)
        require_obz_source
        run_market_multicast
        ;;
    *)
        print_usage >&2
        exit 2
        ;;
esac
