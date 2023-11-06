#!/bin/bash
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
# shellcheck disable=SC2086

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

DATABROKER_PROTO="$SCRIPT_DIR/../proto"

echo "# Get proto files from master:"
set -x

curl -o $DATABROKER_PROTO/sdv/databroker/v1/broker.proto    --create-dirs  https://raw.githubusercontent.com/eclipse/kuksa.val/master/kuksa_databroker/proto/sdv/databroker/v1/broker.proto
curl -o $DATABROKER_PROTO/sdv/databroker/v1/collector.proto --create-dirs  https://raw.githubusercontent.com/eclipse/kuksa.val/master/kuksa_databroker/proto/sdv/databroker/v1/collector.proto
curl -o $DATABROKER_PROTO/sdv/databroker/v1/types.proto     --create-dirs  https://raw.githubusercontent.com/eclipse/kuksa.val/master/kuksa_databroker/proto/sdv/databroker/v1/types.proto
curl -o $DATABROKER_PROTO/sdv/databroker/v1/README.md       --create-dirs  https://raw.githubusercontent.com/eclipse/kuksa.val/master/kuksa_databroker/proto/sdv/databroker/v1/README.md

# NOTE: not using kuksa_databroker/proto/kuksa for kuksa proto, as it only has symlinks
curl -o $DATABROKER_PROTO/kuksa/val/v1/types.proto --create-dirs https://raw.githubusercontent.com/eclipse/kuksa.val/master/proto/kuksa/val/v1/types.proto
curl -o $DATABROKER_PROTO/kuksa/val/v1/val.proto   --create-dirs https://raw.githubusercontent.com/eclipse/kuksa.val/master/proto/kuksa/val/v1/val.proto
curl -o $DATABROKER_PROTO/kuksa/val/v1/README.md   --create-dirs https://raw.githubusercontent.com/eclipse/kuksa.val/master/proto/kuksa/val/v1/README.md