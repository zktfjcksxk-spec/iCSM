#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
./check-server.sh
exec python3 ./run_server.py "$@"
