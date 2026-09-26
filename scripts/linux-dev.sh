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
  test-obz-gcc      Configure, build and test ObzLib with GCC
  test-obz-clang    Configure, build and test ObzLib with Clang
  test-obz-sanitize Configure, build and test ObzLib with GCC, ASan and UBSan
  test-market-gcc   Configure, build and test Market Lab with GCC
  test-market-clang Configure, build and test Market Lab with Clang

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

test_obz() {
    local compiler="$1"
    local build_name="$2"

    run_linux "set -e
cmake -S /workspace/obz -B /build/${build_name} \\
  -G Ninja \\
  -DCMAKE_BUILD_TYPE=Debug \\
  -DCMAKE_CXX_COMPILER=${compiler} \\
  -DOBZ_BUILD_TESTS=ON \\
  -DOBZ_BUILD_EXAMPLES=OFF
cmake --build /build/${build_name} --parallel 4
ctest --test-dir /build/${build_name} --output-on-failure --timeout 60"
}

test_obz_sanitized() {
    run_linux "set -e
cmake -S /workspace/obz -B /build/obz-gcc-asan-ubsan \\
  -G Ninja \\
  -DCMAKE_BUILD_TYPE=Debug \\
  -DCMAKE_CXX_COMPILER=g++ \\
  -DOBZ_BUILD_TESTS=ON \\
  -DOBZ_BUILD_EXAMPLES=OFF \\
  -DOBZ_ENABLE_ASAN_UBSAN=ON
cmake --build /build/obz-gcc-asan-ubsan --parallel 4
ctest --test-dir /build/obz-gcc-asan-ubsan \\
  --exclude-regex '^obz_package_consumer$' \\
  --output-on-failure \\
  --timeout 60"
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
    test-obz-gcc)
        require_obz_source
        test_obz g++ obz-gcc
        ;;
    test-obz-clang)
        require_obz_source
        test_obz clang++ obz-clang
        ;;
    test-obz-sanitize)
        require_obz_source
        test_obz_sanitized
        ;;
    test-market-gcc)
        require_obz_source
        test_market g++ market-gcc
        ;;
    test-market-clang)
        require_obz_source
        test_market clang++ market-clang
        ;;
    *)
        print_usage >&2
        exit 2
        ;;
esac
