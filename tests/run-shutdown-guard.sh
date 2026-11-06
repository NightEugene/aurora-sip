#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
c++ -std=c++14 -pthread -Wall -Wextra -Werror tests/shutdown_guard_test.cpp -o "$test_dir/test"
"$test_dir/test"
echo 'Shutdown guard tests passed'
