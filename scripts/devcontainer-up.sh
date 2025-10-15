#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck source=scripts/lib/devcontainer-env.sh
source "${SCRIPT_DIR}/lib/devcontainer-env.sh"
devcontainer::load_env
devcontainer::validate_ssh_pubkey
ENV_FILE="$(devcontainer::write_env_file)"

# Build if image missing
if ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
  "${SCRIPT_DIR}/devcontainer-build.sh"
fi

# FIXME:  we need to add aesmd service on the cluster in order to have remote attestation work!
# -v /var/run/aesmd:/var/run/aesmd

if docker ps -a --format '{{.Names}}' | grep -qx "${CONTAINER_NAME}"; then
  echo "Container ${CONTAINER_NAME} already exists. Starting…"
  docker start "${CONTAINER_NAME}" >/dev/null
else
  docker run -d \
    --name "${CONTAINER_NAME}" \
    --ulimit memlock=-1:-1 \
    -p "${SSH_BIND_HOST}:${SSH_PORT}:22" \
    -p "${NVIM_BIND_HOST}:${NVIM_PORT}:6666" \
    -v "${PROJECT_ROOT}:${WORKDIR}:rw" \
    --env-file "${ENV_FILE}" \
    --device /dev/sgx_enclave \
    --device /dev/sgx_provision \
    --device /dev/sgx_vepc \
    -w "${WORKDIR}" \
    "${IMAGE_NAME}" >/dev/null
  echo "Started ${CONTAINER_NAME}"
fi

"${SCRIPT_DIR}/devcontainer-ssh-info.sh"
