#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck source=scripts/lib/devcontainer-env.sh
source "${SCRIPT_DIR}/lib/devcontainer-env.sh"
devcontainer::load_env

DOCKERFILE_PATH="${PROJECT_ROOT}/${DOCKERFILE}"
if [[ ! -f "${DOCKERFILE_PATH}" ]]; then
  echo "Missing Dockerfile at ${DOCKERFILE_PATH}" >&2
  exit 1
fi

docker build -t "${IMAGE_NAME}" \
  --build-arg USER_NAME="${DEV_USERNAME}" \
  --build-arg USER_UID="${DEV_UID}" \
  --build-arg USER_GID="${DEV_GID}" \
  -f "${DOCKERFILE_PATH}" \
  "${PROJECT_ROOT}"
