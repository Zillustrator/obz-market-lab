FROM ubuntu:24.04

# Ubuntu's minimized container configuration normally discards manual pages.
# Retain them before installing the C++ build tools and Linux diagnostics.
RUN sed -i '\|path-exclude=/usr/share/man/\*|d' /etc/dpkg/dpkg.cfg.d/excludes \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        cmake \
        gdb \
        git \
        iproute2 \
        man-db \
        manpages \
        manpages-dev \
        ninja-build \
        procps \
        strace \
        util-linux \
    && rm -rf /var/lib/apt/lists/*

# The minimized base leaves a warning script at /usr/bin/man.
RUN ln -sf /usr/bin/man.REAL /usr/bin/man

WORKDIR /workspace

CMD ["/bin/bash"]
