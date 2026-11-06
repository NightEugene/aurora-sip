#!/usr/bin/env bash

# Usage: AURORA_SDK_BT=<SDK directory> ./build.sh [--sign-sdk] [all|armv7hl|aarch64|x86_64 ...]
# All generated files stay in build/. SDK regular signing is for testing.

set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$repo_dir"

: "${AURORA_SDK_BT:?Set AURORA_SDK_BT to the Aurora SDK Build Tools directory}"

if [[ -n ${SDK_RUN:-} ]]; then
    sdk_run=$SDK_RUN
else
    shopt -s nullglob
    wrappers=("$AURORA_SDK_BT"/sdk/*/tools/run)

    if ((${#wrappers[@]} != 1)); then
        echo 'Set SDK_RUN when multiple SDK versions are installed.' >&2
        exit 2
    fi

    sdk_run=${wrappers[0]}
fi

if [[ ! -x $sdk_run ]]; then
    echo 'SDK wrapper is not executable.' >&2
    exit 2
fi

# SDK wrappers split arguments on spaces; reject paths they cannot represent.
if [[ $repo_dir == *[[:space:]]* || $AURORA_SDK_BT == *[[:space:]]* ]]; then
    echo 'SDK Build Tools requires paths without whitespace.' >&2
    exit 2
fi

sign_sdk=false
targets=()

for arg in "$@"; do
    case "$arg" in
        --sign-sdk)
            sign_sdk=true
            ;;
        all)
            targets+=(armv7hl aarch64 x86_64)
            ;;
        armv7hl|aarch64|x86_64)
            targets+=("$arg")
            ;;
        *)
            echo "Unknown option or architecture: $arg" >&2
            exit 2
            ;;
    esac
done

if ((${#targets[@]} == 0)); then
    targets=(armv7hl aarch64 x86_64)
fi

if [[ ! -f third_party/pjproject/pjsip/include/pjsip.h ]]; then
    echo 'Initialize dependencies: git submodule update --init --recursive' >&2
    exit 2
fi

mkdir -p build/RPMS build/logs

signing_dir=

cleanup() {
    if [[ -n $signing_dir ]]; then
        rm -rf -- "$signing_dir"
    fi
}

trap cleanup EXIT

if $sign_sdk; then
    if [[ ! -r $AURORA_SDK_BT/package-signing/regular_key.pem ||
          ! -r $AURORA_SDK_BT/package-signing/regular_cert.pem ]]; then
        echo 'SDK regular signing credentials are unavailable.' >&2
        exit 2
    fi

    signing_dir=$(mktemp -d "$repo_dir/build/signing.XXXXXX")
    install -m 600 "$AURORA_SDK_BT/package-signing/regular_key.pem" "$signing_dir/key.pem"
    install -m 600 "$AURORA_SDK_BT/package-signing/regular_cert.pem" "$signing_dir/cert.pem"
fi

export WORKSPACE_DIR="$repo_dir"

for target_arch in "${targets[@]}"; do
    case "$target_arch" in
        armv7hl)
            target_flag=--arm32
            ;;
        aarch64)
            target_flag=--arm64
            ;;
        x86_64)
            target_flag=--x64
            ;;
    esac

    target_dir="$repo_dir/build/$target_arch"

    # Avoid reusing objects or packages from an earlier build.
    rm -rf -- "$target_dir"
    mkdir -p "$target_dir"
    rm -f -- build/RPMS/*."$target_arch".rpm

    echo "Building $target_arch (log: build/logs/$target_arch-build.log)"

    if ! "$sdk_run" apptool build "$target_flag" --nosign --novalidate \
        --srcdir="$repo_dir" \
        --dstdir="$target_dir" \
        --specfile "$repo_dir/rpm/ru.nighteugene.aurorasip.spec" \
        >"build/logs/$target_arch-build.log" 2>&1; then
        tail -80 "build/logs/$target_arch-build.log" >&2
        exit 1
    fi

    packages=("$target_dir"/RPMS/ru.nighteugene.aurorasip-*."$target_arch".rpm)

    if ((${#packages[@]} != 1)) || [[ ! -f ${packages[0]} ]]; then
        echo 'Expected exactly one application RPM.' >&2
        exit 1
    fi

    package=${packages[0]}

    if $sign_sdk; then
        if ! "$sdk_run" apptool sign \
            --key "$signing_dir/key.pem" \
            --cert "$signing_dir/cert.pem" \
            "$package" >"build/logs/$target_arch-sign.log" 2>&1; then
            cat "build/logs/$target_arch-sign.log" >&2
            exit 1
        fi
    fi

    validation_log="build/logs/$target_arch-validation.log"

    if ! "$sdk_run" apptool validate "$package" >"$validation_log" 2>&1 ||
        grep -Fq '(ERROR)' "$validation_log"; then
        cat "$validation_log" >&2
        exit 1
    fi

    cp -- "$package" build/RPMS/
    echo "Validated: build/RPMS/$(basename "$package")"
done
