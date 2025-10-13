#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=scripts/lib/devcontainer-env.sh
source "${SCRIPT_DIR}/lib/devcontainer-env.sh"
devcontainer::load_env

echo
echo "=== Cluster side (running) ==="
echo "Container:                ${CONTAINER_NAME}"
echo "SSH exposed (cluster):    ${SSH_BIND_HOST}:${SSH_PORT}  -> container :22"
echo "Workspace mounted at:     ${WORKDIR}"
echo
echo "=== From your laptop, create the tunnel ==="
echo "ssh -N -L ${LOCAL_TUNNEL_PORT}:127.0.0.1:${SSH_PORT} <cluster_user>@<cluster_host>"
echo
echo "Then connect your IDE/SSH client to:"
echo "  Host: localhost"
echo "  Port: ${LOCAL_TUNNEL_PORT}"
echo "  User: ${DEV_USERNAME}"
echo
echo "CLI example:"
echo "  ssh -p ${LOCAL_TUNNEL_PORT} ${DEV_USERNAME}@127.0.0.1"
echo
