# Linux development environment

The development image provides a Linux C++ toolchain. Docker Desktop on macOS
runs its containers against the Linux kernel in its VM.

## Build the image

From the repository root:

```sh
docker build -t obz-market-lab-linux-dev:ubuntu24.04 docker/linux-dev
```

The final argument is the build context: the directory Docker can use during
the build. This small context contains only the environment recipe, so the
project's source and existing macOS build directories are not sent to Docker.

The Dockerfile instructions have these roles:

- `FROM` selects Ubuntu userspace as the starting point, not a new kernel.
- `RUN` installs tools while building the image. `build-essential` supplies
  GCC/G++, standard development headers and Make; Clang is installed separately.
  CMake configures builds and Ninja executes their generated build rules.
  `man-db`, `manpages` and `manpages-dev` provide Linux command, protocol and
  system-call reference pages. The recipe also restores those pages and the
  normal `man` executable removed by Ubuntu's minimized container setup.
- `WORKDIR` sets the initial directory for subsequent commands and the shell.
- `CMD` selects Bash as the default process when a container starts.

`apt-get update` refreshes package metadata; `apt-get install` installs the
listed packages. They run together so an installation does not reuse an old
cached metadata layer. Removing the downloaded package lists keeps the image
smaller without removing installed tools.

The Ubuntu release is fixed, but its image tag and package repositories receive
updates. This recipe is repeatable setup, not a byte-for-byte locked toolchain.
Record image IDs and compiler versions when comparing results.

## Explore Linux

```sh
docker run --rm -it obz-market-lab-linux-dev:ubuntu24.04
```

`-it` provides an interactive terminal. `--rm` removes the container when its
shell exits; the image remains available. This first command mounts no source
directories. Changes made inside this container disappear when it is removed.

Inside the shell, try:

```sh
uname -srm
cat /etc/os-release
g++ --version
clang++ --version
cmake --version
ninja --version
pwd
exit
```

`uname` identifies the kernel and architecture, while `/etc/os-release`
identifies the userspace distribution. Their versions describe different parts
of the environment and need not match.

The initial shell runs as root inside the container. This does not make it root
on macOS. Mount permissions still matter when adding access to host files.

## Project workflow

The repository includes `scripts/linux-dev.sh` for the development loop after
the underlying Docker and CMake commands have been exercised manually.

By default it expects the repositories to be adjacent:

```text
Projects/
├── ObzLib/repo
└── Obz Market Lab/repo
```

Override that assumption with `OBZ_SOURCE_DIR`:

```sh
OBZ_SOURCE_DIR=/path/to/ObzLib/repo scripts/linux-dev.sh test-market-gcc
```

The source directories are mounted read-only. Linux build outputs live in the
Docker-managed `obz-linux-build` volume and survive removal of an individual
container.

Available commands are:

```sh
scripts/linux-dev.sh build-image
scripts/linux-dev.sh shell
scripts/linux-dev.sh test-obz-gcc
scripts/linux-dev.sh test-obz-clang
scripts/linux-dev.sh test-market-gcc
scripts/linux-dev.sh test-market-clang
```

Each test command configures its own Debug build directory, builds with four
parallel jobs, and runs CTest with failure output and a 60-second per-test
timeout. GCC and Clang never share a CMake build directory.

The same commands are available from VS Code through `Tasks: Run Task`, with
labels beginning `OML Linux:`. Each task rebuilds the development image first;
Docker normally reuses cached image layers when its recipe has not changed.
