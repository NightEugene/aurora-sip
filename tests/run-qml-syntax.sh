#!/bin/bash
set -euo pipefail
repo_dir=$(cd "$(dirname "$0")/.." && pwd)
qt_dir=${AURORA_TEST_QT:-${AURORA_SDK_BT:?Set AURORA_TEST_QT or AURORA_SDK_BT}/lib/qmllive/Qt}
for page in "$repo_dir"/qml/*.qml; do
 LD_LIBRARY_PATH="$qt_dir/lib:${LD_LIBRARY_PATH:-}" "$qt_dir/bin/qmlformat" "$page" >/dev/null
done
echo 'QML syntax checks passed (imports and rendering require the device)'
