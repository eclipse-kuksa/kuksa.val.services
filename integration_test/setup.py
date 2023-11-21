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

from setuptools import find_packages, setup

PKG_VERSION = "v0.3.0"

pkg_packages = find_packages()

setup(
    name="kuksa.val_integration",
    packages=pkg_packages,
    version=PKG_VERSION,
    classifiers=[
        'License :: OSI Approved :: Apache Software License'
    ]
)
