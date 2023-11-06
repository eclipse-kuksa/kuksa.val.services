# Integration test custom mapping

## Overview

For more details on mapping format, check [dbc2val/mapping.md](https://github.com/eclipse/kuksa.val.feeders/blob/main/dbc2val/mapping/mapping.md)

## Generating json config for Integration Tests

1. Edit [it-mapping.vspec](./it-mapping.vspec) if needed (e.g. ucomment DogMode datapoints)

1. Clone COVESA VSS and vss-tools

    ```console
    git clone --recurse-submodules  https://github.com/COVESA/vehicle_signal_specification
    
    ```

1. Get VSS4.0 released yaml

    ```console
    cd vehicle_signal_specification/
    wget https://github.com/COVESA/vehicle_signal_specification/releases/download/v4.0/vss_rel_4.0.yaml
    ```

1. Convert it-mapping.vspec to json

    ```console
    vss-tools/vspec2json.py -I ./spec --uuid -u ./spec/units.yaml --json-pretty -e dbc2vss -o ../it-mapping.vspec ./spec/VehicleSignalSpecification.vspec ../../it-vss_4.0.json
    ```

    **NOTE:** Running vss-tools (and dbc2val) on python 3.8 was problematic, you may need to use python 3.9 in pyenv, e.g.:

    ```console
    curl https://pyenv.run | bash
    pyenv install 3.9.18
    pyenv versions
    pyenv global 3.9.18
    python3.9 --version
    ```
