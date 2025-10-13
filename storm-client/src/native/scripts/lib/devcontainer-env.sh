#!/usr/bin/env bash

# shellcheck shell=bash

# This helper is meant to be sourced by other scripts – bail out if executed directly.
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  echo "This script provides helper functions and is not intended to be executed directly." >&2
  exit 1
fi

if [[ -n "${DEVCONTAINER_ENV_HELPERS_LOADED:-}" ]]; then
  return 0
fi
DEVCONTAINER_ENV_HELPERS_LOADED=1

DEVCONTAINER_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEVCONTAINER_ENV_FILE_DEFAULT="${DEVCONTAINER_ENV_FILE_DEFAULT:-${DEVCONTAINER_PROJECT_ROOT}/.devcontainer.env}"
DEVCONTAINER_RUNTIME_ENV_FILE_DEFAULT="${DEVCONTAINER_RUNTIME_ENV_FILE_DEFAULT:-${DEVCONTAINER_PROJECT_ROOT}/.devcontainer/.env}"

# Load configuration from .devcontainer.env (if present) and materialise defaults.
devcontainer::load_env() {
  if [[ "${DEVCONTAINER_ENV_VARS_READY:-}" == "1" ]]; then
    return 0
  fi

  local env_file="${1:-$DEVCONTAINER_ENV_FILE_DEFAULT}"
  if [[ -f "${env_file}" ]]; then
    # shellcheck disable=SC1090
    set -a
    source "${env_file}"
    set +a
  fi

  IMAGE_NAME="${IMAGE_NAME:-openenclave-dev}"
  CONTAINER_NAME="${CONTAINER_NAME:-openenclave-devcontainer}"
  DOCKERFILE="${DOCKERFILE:-.devcontainer/Dockerfile}"
  WORKDIR="${WORKDIR:-/workspaces/project}"
  DEVCONTAINER_ENV_FILE="${env_file}"
  DEVCONTAINER_RUNTIME_ENV_FILE="${DEVCONTAINER_RUNTIME_ENV_FILE:-$DEVCONTAINER_RUNTIME_ENV_FILE_DEFAULT}"

  SSH_BIND_HOST="${SSH_BIND_HOST:-127.0.0.1}"
  NVIM_BIND_HOST="${NVIM_BIND_HOST:-127.0.0.1}"
  SSH_PORT="${SSH_PORT:-2222}"
  NVIM_PORT="${NVIM_PORT:-6666}"
  LOCAL_TUNNEL_PORT="${LOCAL_TUNNEL_PORT:-8022}"

  if [[ -z "${DEV_USERNAME:-}" ]]; then
    DEV_USERNAME="$(whoami)"
  fi
  if [[ -z "${DEV_UID:-}" ]]; then
    DEV_UID="$(id -u)"
  fi
  if [[ -z "${DEV_GID:-}" ]]; then
    DEV_GID="$(id -g)"
  fi

  if [[ -z "${GIT_USER_NAME:-}" ]]; then
    GIT_USER_NAME="$(git config --global user.name 2>/dev/null || true)"
  fi
  if [[ -z "${GIT_USER_EMAIL:-}" ]]; then
    GIT_USER_EMAIL="$(git config --global user.email 2>/dev/null || true)"
  fi

  if [[ -z "${SSH_PUBKEY:-}" ]]; then
    for key_file in "${HOME}/.ssh/id_ed25519.pub" "${HOME}/.ssh/id_rsa.pub"; do
      if [[ -f "${key_file}" ]]; then
        SSH_PUBKEY="$(<"${key_file}")"
        break
      fi
    done
  fi

  NVIM_LISTEN_ADDRESS="${NVIM_LISTEN_ADDRESS:-0.0.0.0:${NVIM_PORT}}"

  export IMAGE_NAME CONTAINER_NAME DOCKERFILE WORKDIR
  export SSH_BIND_HOST NVIM_BIND_HOST SSH_PORT NVIM_PORT LOCAL_TUNNEL_PORT
  export DEV_USERNAME DEV_UID DEV_GID SSH_PUBKEY GIT_USER_NAME GIT_USER_EMAIL NVIM_LISTEN_ADDRESS
  export DEVCONTAINER_ENV_FILE DEVCONTAINER_RUNTIME_ENV_FILE

  DEVCONTAINER_ENV_VARS_READY=1
}

# Materialise all relevant variables into an env-file that can be consumed by Docker/Compose.
devcontainer::write_env_file() {
  devcontainer::load_env

  local output="${1:-$DEVCONTAINER_RUNTIME_ENV_FILE_DEFAULT}"
  mkdir -p "$(dirname "${output}")"

  {
    printf 'IMAGE_NAME=%s\n' "${IMAGE_NAME}"
    printf 'CONTAINER_NAME=%s\n' "${CONTAINER_NAME}"
    printf 'DOCKERFILE=%s\n' "${DOCKERFILE}"
    printf 'WORKDIR=%s\n' "${WORKDIR}"
    printf 'SSH_BIND_HOST=%s\n' "${SSH_BIND_HOST}"
    printf 'NVIM_BIND_HOST=%s\n' "${NVIM_BIND_HOST}"
    printf 'SSH_PORT=%s\n' "${SSH_PORT}"
    printf 'NVIM_PORT=%s\n' "${NVIM_PORT}"
    printf 'LOCAL_TUNNEL_PORT=%s\n' "${LOCAL_TUNNEL_PORT}"
    printf 'DEV_USERNAME=%s\n' "${DEV_USERNAME}"
    printf 'DEV_UID=%s\n' "${DEV_UID}"
    printf 'DEV_GID=%s\n' "${DEV_GID}"
    printf 'DEVUSER=%s\n' "${DEV_USERNAME}"
    printf 'DEVUID=%s\n' "${DEV_UID}"
    printf 'DEVGID=%s\n' "${DEV_GID}"
    printf 'SSH_PUBKEY=%s\n' "${SSH_PUBKEY:-}"
    printf 'GIT_USER_NAME=%s\n' "${GIT_USER_NAME:-}"
    printf 'GIT_USER_EMAIL=%s\n' "${GIT_USER_EMAIL:-}"
    printf 'NVIM_LISTEN_ADDRESS=%s\n' "${NVIM_LISTEN_ADDRESS}"
  } >"${output}"

  echo "${output}"
}

# Validate that SSH_PUBKEY looks like a plausible ssh public key.
devcontainer::validate_ssh_pubkey() {
  devcontainer::load_env

  local key="${SSH_PUBKEY:-}"
  if [[ -z "${key// }" ]]; then
    echo "Error: SSH_PUBKEY environment variable is not set. Please set it to your public SSH key." >&2
    return 1
  fi

  if ! [[ "${key}" =~ ^ssh-(rsa|ed25519|dss) ]]; then
    echo "Error: SSH_PUBKEY does not look like a valid SSH key." >&2
    return 1
  fi

  return 0
}
