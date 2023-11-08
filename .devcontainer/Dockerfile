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

# Download base image of Ubuntu
# NOTE: it may be required to build the image locally (check root README.md)
FROM ghcr.io/eclipse/kuksa.val.services/oci_kuksa-val-services-ci:main

# If you get proxy errors, make sure Docker is configured correctly.
# Hint: https://dev.to/zyfa/setup-the-proxy-for-dockerfile-building--4jc8

USER root

# To prevent interactive shells
ENV DEBIAN_FRONTEND=noninteractive

# Install basic utils needed inside devcontainer
RUN apt-get update && \
  apt-get install -qqy curl wget zip  \
  git \
  bash \
  xz-utils \
  curl \
  apt-transport-https \
  jq

# python3.9 is required for feedercan vscode task
RUN apt-get install -qqy python3.9
# hack to install proper version of pip3.9
RUN curl https://bootstrap.pypa.io/get-pip.py | python3.9
  
# Set the locale
RUN apt-get install -qqy locales locales-all
ENV LC_ALL en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US.UTF-8

# Set timezone inside Docker container
ENV TZ=UTC
RUN echo $TZ > /etc/timezone && \
  apt-get update && apt-get install -qqy tzdata && \
  rm /etc/localtime && \
  dpkg-reconfigure -f noninteractive tzdata

#TODO: fix and uncomment: This is needed for local development under linux
#USER dockeruser
