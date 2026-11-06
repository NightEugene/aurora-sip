#!/bin/bash
set -euo pipefail
repo_dir=$(cd "$(dirname "$0")/.." && pwd)
qt_dir=${AURORA_TEST_QT:-${AURORA_SDK_BT:?Set AURORA_TEST_QT or AURORA_SDK_BT}/lib/qmllive/Qt}
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
openssl req -newkey rsa:2048 -nodes -x509 -days 1 -subj '/CN=localhost' -addext 'subjectAltName=DNS:localhost' -keyout "$test_dir/key.pem" -out "$test_dir/cert.pem" >/dev/null 2>&1
cd "$test_dir"
"$qt_dir/bin/qmake" "$repo_dir/tests/contactsource.pro"
make -j2 >build.log 2>&1 || { cat build.log; exit 1; }
AURORA_TEST_STORAGE="$test_dir" XDG_CONFIG_HOME="$test_dir/config" XDG_DATA_HOME="$test_dir/data" TEST_CERT="$test_dir/cert.pem" TEST_KEY="$test_dir/key.pem" HISTORY_JS="$repo_dir/qml/HistoryFormat.js" LD_LIBRARY_PATH="$qt_dir/lib:${LD_LIBRARY_PATH:-}" ./contactsource_test
