#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/run-contactsource.sh
./tests/run-sipthread.sh
./tests/run-credentialstore.sh
./tests/run-dtmftone.sh
./tests/run-tls-identity.sh
./tests/run-shutdown-guard.sh
./tests/run-qml-syntax.sh
bash -n tests/run-*.sh
