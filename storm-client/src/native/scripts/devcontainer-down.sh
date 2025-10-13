#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=scripts/lib/devcontainer-env.sh
source "${SCRIPT_DIR}/lib/devcontainer-env.sh"
devcontainer::load_env

docker rm -f "${CONTAINER_NAME}" 2>/dev/null || true
echo "Removed ${CONTAINER_NAME} (if it existed)."
