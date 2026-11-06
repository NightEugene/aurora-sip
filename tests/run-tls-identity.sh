#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$test_dir/key.pem" 2>/dev/null
make_cert() {
    openssl req -new -x509 -key "$test_dir/key.pem" -days 1 -subj "/CN=$2" -addext "subjectAltName=$3" -out "$test_dir/$1.pem" 2>/dev/null
}
make_cert wildcard '*.omp.ru' 'DNS:*.omp.ru'
make_cert san voice.omp.ru 'DNS:other.omp.ru'
make_cert ip unused.test 'IP:127.0.0.1,IP:::1'
make_cert partial unused.test 'DNS:vo*.omp.ru'
cc -std=c99 -Wall -Wextra -Werror tests/tls_identity_test.c -o "$test_dir/test" $(pkg-config --cflags --libs openssl)
"$test_dir/test" "$test_dir/wildcard.pem" "$test_dir/san.pem" "$test_dir/ip.pem" "$test_dir/partial.pem"
