# /********************************************************************************
# * Copyright (c) 2023 Contributors to the Eclipse Foundation
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

# Build stage, to create a Virtual Environent
FROM python:3.11-slim-bullseye

COPY . /

RUN python3 -m pip install --no-cache-dir -r requirements.txt

# useful dumps about feeding values
ENV LOG_LEVEL="info,databroker=debug,mock_service=debug"

# Vehicle Data Broker host:port
#ENV VDB_ADDRESS="localhost:55555"
# Override VDB_ADDRESS port if set
#ENV DAPR_GRPC_PORT="55555"
# VDB DAPR APP ID
ENV VEHICLEDATABROKER_DAPR_APP_ID=vehicledatabroker

ENV PYTHONUNBUFFERED=yes

LABEL org.opencontainers.image.source="https://github.com/eclipse/kuksa.val.services"

CMD ["python3", "/mock/mockservice.py"]
