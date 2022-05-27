# HVAC service example

The HVAC service is a service dummy allowing to control the state of the A/C and the desired cabin temperature.
"Dummy" means, that changes of those two states are just forwarded and reflected as two respective data points in the data broker.

```text
                      +----------------+
                      |                |
         ----( O------|  Data broker   |-----O )------ 
         |    Broker  |                |  Collector  |
         |            +----------------+             |
         |                                           |
         |                                           |
+-----------------+                         +----------------+
|                 |                         |                |
|   HVAC client   |-----------( O-----------|  HVAC service  |
|                 |            HVAC         |                |
+-----------------+           service       +----------------+
```

## Configuration

| parameter      | default value         | Env var                                                                          | description                     |
|----------------|-----------------------|----------------------------------------------------------------------------------|---------------------------------|
| listen_address | `"127.0.0.1:50052"`   | `HVAC_ADDR`                                                                      | Listen for rpc calls            |
| broker_address | `"127.0.0.1:55555"`   | `"127.0.0.1:$DAPR_GRPC_PORT"` (if DAPR_GRPC_PORT is set)<br>`VDB_ADDRESS` (else) | Connect to data broker instance |
| broker_app_id  | `"vehicledatabroker"` | `VEHICLEDATABROKER_DAPR_APP_ID`                                                  | Connect to data broker instance |

Configuration options have the following priority (highest at top):
1. environment variable
1. default value
