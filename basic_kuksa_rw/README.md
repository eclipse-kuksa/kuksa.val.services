# To run minimal example

1) docker run -e RUST_LOG="debug" --rm -it -p 55555:55555/tcp ghcr.io/eclipse/kuksa.val/databroker:master

2) pip3 install -r requirements-dev.txt

3) python3 test_rw.py