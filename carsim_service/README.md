# CAR sim example

The CAR sim service is a service which simulates a car.

1. Run data broker

    docker run --rm -it -p 55555:55555/tcp ghcr.io/eclipse/kuksa.val/databroker:master

2. Run car sim

    python3 carsim.py
