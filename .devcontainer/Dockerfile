#
# =========================
# Stage 1: Build nvim (glibc)
# =========================
#
FROM ubuntu:22.04 AS nvim-builder

ARG NVIM_REF=stable
ARG CMAKE_BUILD_TYPE=RelWithDebInfo
ENV CMAKE_INSTALL_PREFIX=/nvim-out

# Use bash + pipefail for safety in RUN steps
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Build prerequisites
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build gettext libtool libtool-bin \
  autoconf automake pkg-config curl unzip git ccache ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src/neovim

# Result goes to /nvim-out (bin/nvim, share/runtime, etc.)
RUN git clone --filter=blob:none --depth=1 --branch "${NVIM_REF}" https://github.com/neovim/neovim . \
  && make CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
  && make CMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}" install

#
# =========================================
# Final: Ubuntu 22.04 + OE tools base + JDK
# =========================================
#
FROM openenclavedockerregistry.azurecr.io/oetools-22.04:2025.03.13148

ARG USER_NAME=dev
ARG USER_UID=1000
ARG USER_GID=1000

ARG OE_SDK_VERSION=0.19.13
ARG JDK_MAJOR=21

ARG CMAKE_VERSION=4.1.2
ARG CMAKE_ARCH=x86_64

ARG NODE_VERSION=22.20.0
ARG NODE_ARCH=x64

ARG STORM_VERSION=2.8.2
ENV STORM_HOME=/opt/storm

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ENV NVIM_LISTEN_ADDRESS=0.0.0.0:6666

# Core tools, SSH, JDK, CLI utils used by Neovim plugins
# NOTE: On Ubuntu 22.04, openjdk-21-jdk may require updates/backports on some mirrors.
# If it fails, switch to openjdk-17-jdk or install Temurin via apt.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  openssh-server sudo tini ca-certificates tmux \
  curl wget gnupg apt-transport-https unzip \
  build-essential pkg-config \
  git \
  openjdk-${JDK_MAJOR}-jdk \
  maven \
  nodejs npm \
  ripgrep fzf \
  zlib1g-dev libssl-dev \
  python3 python3-venv \
  golang-go \
  && rm -rf /var/lib/apt/lists/*

# Create dev group/user matching host IDs
RUN set -eux; \
  if getent group "${USER_GID}" >/dev/null; then \
  EXIST_GRP="$(getent group "${USER_GID}" | cut -d: -f1)"; \
  [ "${EXIST_GRP}" = "${USER_NAME}" ] || groupmod -n "${USER_NAME}" "${EXIST_GRP}"; \
  elif getent group "${USER_NAME}" >/dev/null; then \
  groupmod -g "${USER_GID}" "${USER_NAME}"; \
  else \
  groupadd -g "${USER_GID}" "${USER_NAME}"; \
  fi; \
  if getent passwd "${USER_UID}" >/dev/null; then \
  EXIST_USER="$(getent passwd "${USER_UID}" | cut -d: -f1)"; \
  if [ "${EXIST_USER}" != "${USER_NAME}" ]; then \
  usermod -l "${USER_NAME}" "${EXIST_USER}"; \
  usermod -d "/home/${USER_NAME}" -m "${USER_NAME}"; \
  fi; \
  usermod -u "${USER_UID}" -g "${USER_GID}" "${USER_NAME}" || true; \
  elif id -u "${USER_NAME}" >/dev/null 2>&1; then \
  usermod -u "${USER_UID}" -g "${USER_GID}" "${USER_NAME}" || true; \
  else \
  useradd -m -s /bin/bash -u "${USER_UID}" -g "${USER_GID}" "${USER_NAME}"; \
  fi; \
  usermod -aG sudo "${USER_NAME}"; \
  echo "${USER_NAME} ALL=(ALL) NOPASSWD:ALL" >/etc/sudoers.d/90-"${USER_NAME}"; \
  chmod 0440 /etc/sudoers.d/90-"${USER_NAME}"

# SGX group (device access without sudo, device must be mapped at runtime)
RUN if ! getent group sgx >/dev/null; then groupadd sgx; fi && \
  usermod -aG sgx "${USER_NAME}"

# Configure JAVA_HOME for all shells
RUN JH="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"; \
  printf 'export JAVA_HOME=%q\nexport PATH=$JAVA_HOME/bin:$PATH\n' "$JH" \
  > /etc/profile.d/java.sh && chmod +x /etc/profile.d/java.sh

# Optional: install Open Enclave SDK deb (oetools base already includes toolchains; keep if you need exact SDK version)
WORKDIR /tmp
RUN curl -fsSL -o "Ubuntu_2204_open-enclave_${OE_SDK_VERSION}_amd64.deb" \
  "https://github.com/openenclave/openenclave/releases/download/v${OE_SDK_VERSION}/Ubuntu_2204_open-enclave_${OE_SDK_VERSION}_amd64.deb" \
  && dpkg -i "Ubuntu_2204_open-enclave_${OE_SDK_VERSION}_amd64.deb" \
  && rm -f "Ubuntu_2204_open-enclave_${OE_SDK_VERSION}_amd64.deb"

# Install Apache Storm CLI directly from Apache official mirror
# NOTE:
# - The official apache archive is too slow (https://archive.apache.org/dist/storm/apache-storm-${STORM_VERSION}/apache-storm-${STORM_VERSION}.tar.gz)
# - Running from github source is not allowed (storm crashes and warns about this)
RUN curl -fsSL -o "storm.tgz" "https://dlcdn.apache.org/storm/apache-storm-${STORM_VERSION}/apache-storm-${STORM_VERSION}.tar.gz" && \
  mkdir -p /opt && \
  tar -xzf storm.tgz -C /opt && \
  ln -s "/opt/apache-storm-${STORM_VERSION}" "${STORM_HOME}" && \
  ln -s "${STORM_HOME}/bin/storm" /usr/local/bin/storm && \
  rm -f storm.tgz

# Copy nvim from nvim-builder
COPY --from=nvim-builder /nvim-out/ /usr/local/
RUN nvim --version | head -n 3

# Import neovim config from github + fix ownership
RUN git clone https://github.com/lucadibello/neovim.git /home/${USER_NAME}/.config/nvim \
  && chown -R "${USER_UID}:${USER_GID}" "/home/${USER_NAME}/.config"

# tmux plugin manager (install; then fix ownership)
RUN git clone https://github.com/tmux-plugins/tpm "/home/${USER_NAME}/.tmux/plugins/tpm" \
  && TMUX_PLUGIN_MANAGER_PATH="/home/${USER_NAME}/.tmux/plugins/tpm" \
  "/home/${USER_NAME}/.tmux/plugins/tpm/bin/install_plugins" || true \
  && chown -R "${USER_UID}:${USER_GID}" "/home/${USER_NAME}/.tmux"

# lazygit (binary release)
RUN LAZYGIT_VERSION=$(curl -s "https://api.github.com/repos/jesseduffield/lazygit/releases/latest" | \grep -Po '"tag_name": *"v\K[^"]*') && \
  curl -Lo lazygit.tar.gz "https://github.com/jesseduffield/lazygit/releases/download/v${LAZYGIT_VERSION}/lazygit_${LAZYGIT_VERSION}_Linux_x86_64.tar.gz" && \
  tar xf lazygit.tar.gz lazygit && \
  install lazygit -D -t /usr/local/bin/ && \
  echo "LazyGit installed successfully."

# install CMake (currently, we use this format for destination folder /usr/local/cmake-<version>-Linux-<arch>/)
RUN set -eux; \
  echo "Installing CMake ${CMAKE_VERSION}..." && \
  curl -fsSLO "https://cmake.org/files/v${CMAKE_VERSION%.*}/cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}.tar.gz" && \
  tar -xzf "cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}.tar.gz" && \
  mv "cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}" /opt/cmake && \
  ln -sf /opt/cmake/bin/* /usr/local/bin/ && \
  echo "Verifying CMake installation..." && \
  cmake --version && \
  rm -rf /tmp/cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}* && \
  echo "CMake installed successfully."

# install Node.JS LST (useful for tools!)
RUN set -eux; \
  echo "Installing Node.js v${NODE_VERSION} for ${NODE_ARCH}..." && \
  curl -fsSLO "https://nodejs.org/dist/v${NODE_VERSION}/node-v${NODE_VERSION}-linux-${NODE_ARCH}.tar.xz" && \
  tar -xf "node-v${NODE_VERSION}-linux-${NODE_ARCH}.tar.xz" && \
  mv "node-v${NODE_VERSION}-linux-${NODE_ARCH}" /usr/local/node-v${NODE_VERSION} && \
  ln -sf /usr/local/node-v${NODE_VERSION}/bin/node /usr/local/bin/node && \
  ln -sf /usr/local/node-v${NODE_VERSION}/bin/npm /usr/local/bin/npm && \
  ln -sf /usr/local/node-v${NODE_VERSION}/bin/npx /usr/local/bin/npx && \
  echo "Verifying Node.js installation..." && \
  node -v && npm -v && \
  rm -rf /tmp/node-v${NODE_VERSION}-linux-${NODE_ARCH}* && \
  echo "Node.js installed successfully."

# Workspace and env for Open Enclave
WORKDIR /workspaces/project
RUN chown -R "${USER_UID}:${USER_GID}" /workspaces
RUN printf '%s\n' \
  'export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:/opt/openenclave/share/pkgconfig"' \
  'export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}:/opt/openenclave/lib/openenclave/cmake"' \
  'export PATH="${PATH}:/opt/openenclave/bin"' \
  'export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/opt/openenclave/lib"' \
  > /etc/profile.d/openenclave.sh && chmod +x /etc/profile.d/openenclave.sh

# Generate an SSH key (NOTE: this bakes secrets into the image layers — prefer doing this at runtime)
RUN install -d -m 700 -o "${USER_UID}" -g "${USER_GID}" "/home/${USER_NAME}/.ssh" \
  && ssh-keygen -t ed25519 -f "/home/${USER_NAME}/.ssh/id_ed25519" -N '' -C 'devcontainer-key' \
  && chown -R "${USER_UID}:${USER_GID}" "/home/${USER_NAME}/.ssh" \
  && echo "==== Generated SSH Public Key ====" \
  && cat "/home/${USER_NAME}/.ssh/id_ed25519.pub" \
  && echo "================================="

# Create a basic Storm config file to connect to remote Nimbus service (referenced by service name as we are in the same Docker network)
RUN mkdir -p /home/${USER_NAME}/.storm \
  && echo 'nimbus.seeds: ["nimbus"]' > "/home/${USER_NAME}/.storm/storm.yaml"

# Entrypoint
COPY .devcontainer/devcontainer-entrypoint.sh /usr/local/bin/devcontainer-entrypoint
RUN chmod +x /usr/local/bin/devcontainer-entrypoint

# Drop root for normal usage
USER ${USER_UID}:${USER_GID}

# expose SSH + Neovim remote
EXPOSE 22 6666

# Define default command
ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/devcontainer-entrypoint"]
CMD ["/usr/sbin/sshd", "-D", "-e"]
