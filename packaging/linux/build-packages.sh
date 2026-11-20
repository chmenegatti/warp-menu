#!/bin/sh
# Builds the .deb and .rpm inside a Rocky Linux 9 container (glibc 2.34), so
# the packages run on Ubuntu 22.04+, Debian 12+, Fedora 35+ and RHEL/Rocky 9+.
#
#   docker run --rm -v "$PWD":/src -w /src -e HOST_UID="$(id -u)" \
#       rockylinux:9 sh packaging/linux/build-packages.sh
#
# Output: build-pkg/warp-menu-<version>-Linux.{deb,rpm} (renamed below).
set -eu
dnf install -y -q dnf-plugins-core >/dev/null
dnf config-manager --set-enabled crb
dnf install -y -q gcc-c++ libstdc++-static cmake git rpm-build \
    mesa-libGL-devel libX11-devel libXrandr-devel libXi-devel \
    libXinerama-devel libXcursor-devel alsa-lib-devel >/dev/null

cmake -S . -B build-pkg -DCMAKE_BUILD_TYPE=Release
cmake --build build-pkg -j"$(nproc)"
(cd build-pkg && cpack -G "DEB;RPM")

if [ -n "${HOST_UID:-}" ]; then chown -R "$HOST_UID" build-pkg; fi
ls -l build-pkg/*.deb build-pkg/*.rpm
