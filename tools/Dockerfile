# /********************************************************************************
# * Copyright (c) 2022 Contributors to the Eclipse Foundation
# *
# * See the NOTICE file(s) distributed with this work for additional
# * information regarding copyright ownership.
# *
# * This program and the accompanying materials are made available under the
# * terms of the Apache License 2.0 which is available at
# * http://www.apache.org/licenses/LICENSE-2.0
# *
# * SPDX-License-Identifier: Apache-2.0
# ********************************************************************************/

# Download base image of Ubuntu LTS
FROM ubuntu:20.04

# If you get proxy errors, make sure Docker is configured correctly.
# Hint: https://dev.to/zyfa/setup-the-proxy-for-dockerfile-building--4jc8

# User permission issues explained 
# https://jtreminio.com/blog/running-docker-containers-as-current-host-user/

#TODO: fix and uncomment:  This is needed for local development under linux
# ARG USERNAME=dockeruser
# ARG USER_ID=1000
# ARG GROUP_ID=$USER_ID

# To prevent interactive shells
ENV DEBIAN_FRONTEND=noninteractive

# Install apt & root utils needed inside devcontainer
RUN \
  sed -i 's/# \(.*multiverse$\)/\1/g' /etc/apt/sources.list && \
  apt-get update && \
  apt-get -y install --no-install-recommends apt-utils 2>&1 && \
  apt-get install -y software-properties-common && \
  apt-get install -qqy sudo

# Upgrade as needed
RUN apt-get update && apt-get upgrade -y

# Install dev utilities
RUN apt-get -y install \
  git \
  doxygen \
  can-utils

# Install build tools
RUN apt-get -y install \
  cmake make ccache

# compilers
#   compiler version must be sync with conan build profile
RUN apt-get -y install \
  g++ \
  g++-aarch64-linux-gnu

RUN apt-get -y install \
  lcov \
  gcovr \
  clang-format \
  cppcheck \
  valgrind 


# Install PIP
RUN apt-get -y install --fix-missing \
  python3-pip

# conan: dependency management
# conan needed > 1.43 for gtest
# cantools: code generation from .dbc file
RUN pip3 install \
  'conan==1.48.1' \
  'cantools==37.0.1'
  
# Install Rust tools and prerequists needed for the databroker
RUN apt-get -y install \
  curl
# RUN curl https://sh.rustup.rs -sSf | sh -s -- --profile default --default-toolchain 1.60 -y

# Install python tools and prerequists needed for all python using subfolders
COPY hvac_service/requirements-dev.txt hvac_service/requirements-dev.txt 
COPY integration_test/requirements-dev.txt integration_test/requirements-dev.txt 
RUN pip3 install \
  -r hvac_service/requirements-dev.txt \
  -r integration_test/requirements-dev.txt

WORKDIR /workspace
