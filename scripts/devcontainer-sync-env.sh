#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=scripts/lib/devcontainer-env.sh
source "${SCRIPT_DIR}/lib/devcontainer-env.sh"
devcontainer::load_env
devcontainer::validate_ssh_pubkey
ENV_FILE_PATH="$(devcontainer::write_env_file)"

echo "Wrote devcontainer environment to ${ENV_FILE_PATH}"
