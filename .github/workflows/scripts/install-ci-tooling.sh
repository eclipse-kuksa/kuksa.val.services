#!/bin/bash
#********************************************************************************
# Copyright (c) 2022 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License 2.0 which is available at
# http://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
#*******************************************************************************/

# To prevent interactive shells
export ENV DEBIAN_FRONTEND=noninteractive

# Install apt & root utils needed inside devcontainer
sudo apt-get -qqy update &&
	sudo apt-get -qqy install --no-install-recommends apt-utils software-properties-common sudo curl file

# Install dev utilities
sudo apt-get -qqy install --no-install-recommends git doxygen can-utils python3

# Install build tools
sudo apt-get -qqy install --no-install-recommends \
	cmake \
	make

# compilers
#   compiler version must be sync with conan build profile
sudo apt-get -qqy install --no-install-recommends \
	g++ \
	g++-aarch64-linux-gnu

sudo apt-get -qqy install --no-install-recommends \
	lcov \
	gcovr \
	clang-format \
	cppcheck \
	valgrind

# Install PIP
[ -z "$(which pip3)" ] && sudo apt-get -qqy install --no-install-recommends --fix-missing python3-pip

# Default pip and setuptools are too old...
pip3 install --upgrade pip setuptools

# conan: dependency management
# - conan needed > 1.43 for gtest
# - conan >= 2.0 breaks commandline interface
# cantools: code generation from .dbc file
pip3 install \
	'conan==1.56.0' \
	'cantools==37.0.1'

# install docker
# curl -fsSL https://get.docker.com -o - | bash -
# docker --version

echo "### Running on:"
cat /etc/os-release