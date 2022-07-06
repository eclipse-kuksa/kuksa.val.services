#!/bin/bash
# To prevent interactive shells
export ENV DEBIAN_FRONTEND=noninteractive

if [ -z "$(which g++)" ] || [ -z "$(which cmake)" ]; then
  # Install apt & root utils needed inside devcontainer
  sudo apt-get -qqy update && \
  sudo apt-get -qqy install --no-install-recommends apt-utils software-properties-common sudo curl

  # Install dev utilities
  sudo apt-get -qqy install git doxygen can-utils python3

  # Install build tools
  sudo apt-get -qqy install \
    cmake \
    make

  # compilers
  #   compiler version must be sync with conan build profile
  sudo apt-get -qqy install \
    g++ \
    g++-aarch64-linux-gnu

  sudo apt-get -qqy install \
    lcov \
    gcovr \
    clang-format \
    cppcheck \
    valgrind
fi

# Install PIP
[ -z "$(which pip3)" ] && sudo apt-get -qqy install --fix-missing python3-pip

# conan: dependency management
# conan needed > 1.43 for gtest
# cantools: code generation from .dbc file
pip3 install \
  'conan==1.48.1' \
  'cantools==37.0.1'

# install docker
# curl -fsSL https://get.docker.com -o - | bash -
# docker --version
