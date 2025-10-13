#!/usr/bin/env bash
set -euo pipefail

# Re-exec as root when the image is configured with a non-root default user.
# The entrypoint needs elevated privileges to manage users and SSHD.
if [ "${DEVCONTAINER_ALREADY_ROOT:-0}" != "1" ] && [ "$(id -u)" -ne 0 ]; then
  exec sudo -E DEVCONTAINER_ALREADY_ROOT=1 "$0" "$@"
fi

# Defaults match Dockerfile ARGs (can be overridden via env)
DEVUSER="${DEVUSER:-${USER_NAME:-dev}}"
DEVUID="${DEVUID:-${USER_UID:-1000}}"
DEVGID="${DEVGID:-${USER_GID:-1000}}"
NVIM_LISTEN_ADDRESS="${NVIM_LISTEN_ADDRESS:-0.0.0.0:6666}"
export NVIM_LISTEN_ADDRESS
if [ "$NVIM_LISTEN_ADDRESS" = "none" ]; then
  NVIM_LISTEN_ADDRESS=""
fi

# Ensure ssh runtime dir
mkdir -p /var/run/sshd

# Generate host keys if missing (idempotent)
if ! ls /etc/ssh/ssh_host_* >/dev/null 2>&1; then
  ssh-keygen -A
fi

# Ensure group/user exist and match requested IDs
if getent group "$DEVGID" >/dev/null; then
  EXIST_GRP="$(getent group "$DEVGID" | cut -d: -f1)"
  [ "$EXIST_GRP" = "$DEVUSER" ] || groupmod -n "$DEVUSER" "$EXIST_GRP"
elif getent group "$DEVUSER" >/dev/null; then
  groupmod -g "$DEVGID" "$DEVUSER"
else
  groupadd -g "$DEVGID" "$DEVUSER"
fi

if id -u "$DEVUSER" >/dev/null 2>&1; then
  CUR_UID="$(id -u "$DEVUSER")"
  CUR_GID="$(id -g "$DEVUSER")"
  [ "$CUR_GID" = "$DEVGID" ] || groupmod -g "$DEVGID" "$DEVUSER" || true
  if [ "$CUR_UID" != "$DEVUID" ]; then
    usermod -u "$DEVUID" -g "$DEVGID" "$DEVUSER" || true
    USER_HOME="$(getent passwd "$DEVUSER" | cut -d: -f6)"
    chown -R "$DEVUID:$DEVGID" "$USER_HOME" 2>/dev/null || true
  fi
else
  useradd -m -s /bin/bash -u "$DEVUID" -g "$DEVGID" "$DEVUSER"
  usermod -aG sudo "$DEVUSER" || true
fi

# Configure authorized_keys from either:
#  - env SSH_PUBKEY (single key string)
#  - mounted file /ssh/authorized_keys (one or more keys)
USER_HOME="$(getent passwd "$DEVUSER" | cut -d: -f6)"
SSH_DIR="$USER_HOME/.ssh"
AUTH_KEYS="$SSH_DIR/authorized_keys"

mkdir -p "$SSH_DIR"
touch "$AUTH_KEYS"
chmod 700 "$SSH_DIR"
chmod 600 "$AUTH_KEYS"
chown -R "$DEVUSER:$DEVGID" "$SSH_DIR"

if [ -n "${SSH_PUBKEY:-}" ]; then
  # Append (dedup later)
  echo "$SSH_PUBKEY" >>"$AUTH_KEYS"
fi
if [ -f /ssh/authorized_keys ]; then
  cat /ssh/authorized_keys >>"$AUTH_KEYS"
fi
# Deduplicate keys
sort -u "$AUTH_KEYS" -o "$AUTH_KEYS" || true
chown "$DEVUSER:$DEVGID" "$AUTH_KEYS"

# Helpful: print how to connect (once)
if [ "${PRINT_SSH_HINTS:-1}" != "0" ]; then
  echo "SSHD ready. Example:"
  echo "  docker run -d -p 2222:22 -e SSH_PUBKEY=\"\$(cat ~/.ssh/id_ed25519.pub)\" <image>"
  echo "  ssh -p 2222 ${DEVUSER}@127.0.0.1"
fi

# setup git config for the dev user
if [ -n "${GIT_USER_NAME:-}" ] && [ -n "${GIT_USER_EMAIL:-}" ]; then
  sudo -u "$DEVUSER" git config --global user.name "$GIT_USER_NAME"
  sudo -u "$DEVUSER" git config --global user.email "$GIT_USER_EMAIL"
fi

# start headless nvim watchdog
cat >/usr/local/bin/nvim-server.sh <<'EOS'
#!/usr/bin/env bash
set -euo pipefail
SOCK="${NVIM_LISTEN_ADDRESS:-/tmp/nvim.sock}"
mkdir -p "$(dirname "$SOCK")"
while :; do
  rm -f "$SOCK" || true
  nvim --headless --listen "$SOCK" || true
  sleep 1
done
EOS
chmod +x /usr/local/bin/nvim-server.sh

NVIM_ENV_PRESERVE="NVIM_LISTEN_ADDRESS"
if [ -n "${SSH_AUTH_SOCK:-}" ]; then
  NVIM_ENV_PRESERVE="${NVIM_ENV_PRESERVE},SSH_AUTH_SOCK"
fi

if [ "$DEVUSER" = "root" ]; then
  /usr/local/bin/nvim-server.sh &
  NVIM_SERVER_PID=$!
else
  sudo --preserve-env="$NVIM_ENV_PRESERVE" -u "$DEVUSER" /usr/local/bin/nvim-server.sh &
  NVIM_SERVER_PID=$!
fi
echo "[entrypoint] nvim server started (pid $NVIM_SERVER_PID)"

# Exec original CMD (sshd -D -e by default)
exec "$@"
