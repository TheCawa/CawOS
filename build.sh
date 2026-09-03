#!/usr/bin/env bash
# Convenience wrapper around scripts/build.py
set -e
cd "$(dirname "$0")"
python3 scripts/build.py "$@"
