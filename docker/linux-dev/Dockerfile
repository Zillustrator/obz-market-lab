FROM ubuntu:24.04

# Install the C++ build tools and Linux diagnostics in the image.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        cmake \
        gdb \
        git \
        iproute2 \
        ninja-build \
        procps \
        strace \
        util-linux \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
