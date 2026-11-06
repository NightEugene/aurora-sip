#!/bin/bash
set -euo pipefail
repo_dir=$(cd "$(dirname "$0")/.." && pwd)
qt_dir=${AURORA_TEST_QT:-${AURORA_SDK_BT:?Set AURORA_TEST_QT or AURORA_SDK_BT}/lib/qmllive/Qt}
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
cd "$test_dir"
"$qt_dir/bin/qmake" "$repo_dir/tests/credentialstore.pro"
make -j2 >build.log 2>&1 || { cat build.log; exit 1; }
LD_LIBRARY_PATH="$qt_dir/lib:${LD_LIBRARY_PATH:-}" ./credentialstore_test
