#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=scripts/lib/devcontainer-env.sh
source "${SCRIPT_DIR}/lib/devcontainer-env.sh"
devcontainer::load_env

docker exec -it "${CONTAINER_NAME}" bash || docker exec -it "${CONTAINER_NAME}" sh
