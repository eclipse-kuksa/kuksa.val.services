#!/bin/bash
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

# shellcheck disable=SC2181
# shellcheck disable=SC2086
# shellcheck disable=SC2230
# shellcheck disable=SC2034

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

set -e

# shellcheck source=/dev/null
source ${SCRIPT_DIR}/it-config

### cleanup of IT containers and optionally images
cleanup() {
	local force="$1"
	# ensure containers are not running
	echo "# Cleanup iteration test containers..."
	if docker ps -a | grep "${VDB_CONTAINER}"; then
		docker container rm -f "${VDB_CONTAINER}"
	fi
	if docker ps -a | grep "${SEAT_CONTAINER}"; then
		docker container rm -f "${SEAT_CONTAINER}"
	fi
	if docker ps -a | grep "${FEEDER_CONTAINER}"; then
		docker container rm -f "${FEEDER_CONTAINER}"
	fi
	if [ "$force" = "1" ]; then
		echo "# Cleanup VAL ghcr images..."
		docker image rm -f "${VDB_IMAGE}"
		docker image rm -f "${SEAT_IMAGE}"
		docker image rm -f "${FEEDER_IMAGE}"
	fi
}

abort() {
	local code=$1
	local msg="$2"

	printf "[ERR:%d] Aborting: %s\n" ${code} "${msg}"
	# cleanup 0
	exit ${code}
}

### logins to $DOCKER_REPO + pull of images
pull_images() {
	local force="$1"
	if [ -z "${DOCKER_REPO}" ]; then
		return 0
	fi
	if [ "${force}" = "1" ] ||
		! __check_docker_image "${VDB_IMAGE}" ||
		! __check_docker_image "${SEAT_IMAGE}" ||
		! __check_docker_image "${FEEDER_IMAGE}"; then
		echo "- Pulling images form ${DOCKER_REPO} (May need manual login)..."
		docker login "${DOCKER_REPO}"

		docker pull "${VDB_IMAGE}"
		docker pull "${SEAT_IMAGE}"
		docker pull "${FEEDER_IMAGE}"
	fi
}

build_images() {
	local force="$1"
	if [ "$force" = "1" ] || ! __check_docker_image "${SEAT_IMAGE}"; then
		echo "# Building amd64/seat-service:latest ..."
		if cd ${SCRIPT_DIR}/../seat_service && ./docker-build.sh -l x86_64; then
			docker tag amd64/seat-service:latest ${SEAT_IMAGE}
		fi
	fi
	if [ "$force" = "1" ] || ! __check_docker_image "${VDB_IMAGE}"; then
		echo "# Building amd64/databroker:latest ..."
		if cd ${SCRIPT_DIR}/../vehicle_data_broker && ./docker-build.sh -l x86_64; then
			docker tag amd64/databroker:latest ${VDB_IMAGE}
		fi
	fi
	if [ "$force" = "1" ] || ! __check_docker_image "${FEEDER_IMAGE}"; then
		echo "# Building amd64/feeder_can:latest ..."
		if cd ${SCRIPT_DIR}/../feeder_can && ./docker-build.sh -l x86_64; then
			docker tag amd64/feeder_can:latest ${FEEDER_IMAGE}
		fi
	fi
}

### Checks if container (name) is running
__check_container_state() {
	local name="$1"
	local verbose="$2"
	RESULT=$(docker ps -a --filter name=${name} --format='{{.Names}}\t{{.Image}}\tstate:{{.State}}\t{{.Status}}\tNetwork:{{.Networks}} ({{.Ports}})')
	[ "$verbose" = "1" ] && echo "$RESULT" 1>&2
	if echo "$RESULT" | grep -q "state:running"; then
		return 0
	else
		return 1
	fi
}

### Checks if docker image:tag is locally available
__check_docker_image() {
	local name="$1"
	docker_images=$(docker images --format '{{.Repository}}:{{.Tag}}')
	if echo "${docker_images}" | grep -q "${name}"; then
		return 0
	else
		return 1
	fi
}

### Checks if $VDB_CONTAINER and $SEAT_CONTAINER are both running
check_it_containers() {
	local verbose="$1"

	local seat_err=0
	local vdb_err=0
	local feed_err=0
	__check_container_state "${VDB_CONTAINER}" "${verbose}" || vdb_err=1
	__check_container_state "${SEAT_CONTAINER}" "${verbose}" || seat_err=1
	__check_container_state "${FEEDER_CONTAINER}" "${verbose}" || feed_err=1

	if [ ${vdb_err} -ne 0 ] || [ ${seat_err} -ne 0 ] || [ ${feed_err} -ne 0 ]; then
		return 1
	fi
	return 0
}

### Starts $VDB_CONTAINER and $SEAT_CONTAINER
start_containers() {
	echo "- Running ${VDB_CONTAINER} ..."
	# DataBroker container options
	rc=0
	docker run -d ${DOCKER_OPT} ${VDB_DOCKER_OPT} "${VDB_IMAGE}" || rc=1

	echo "- Running ${SEAT_CONTAINER} ..."
	# SeatService container options. BROKER_ADDR is needed to reach it-databroker ports within val-test network
	docker run -d ${DOCKER_OPT} ${SEAT_DOCKER_OPT} "${SEAT_IMAGE}" || rc=2

	echo "- Running ${FEEDER_CONTAINER} ..."
	# SeatService container options. BROKER_ADDR is needed to reach it-databroker ports within val-test network
	docker run -d ${DOCKER_OPT} ${FEEDER_DOCKER_OPT} "${FEEDER_IMAGE}" || rc=3

	echo
	__check_container_state "${VDB_CONTAINER}" 1 || rc=1
	__check_container_state "${SEAT_CONTAINER}" 1 || rc=2
	__check_container_state "${FEEDER_CONTAINER}" 1 || rc=3
	echo

	return ${rc}
}

network_setup() {
	local network="$1"

	if [ "$network" = "host" ]; then
		return 0
	fi
	if ! docker network ls | grep -q "${network}"; then
		echo "- Create ${network} docker network"
		docker network create ${network}
		return $?
	fi
}

it_init() {
	local force="$1"

	# RESULTS dir is defined in it-config
	# shellcheck disable=SC2153
	if [ -n "${RESULTS}" ]; then
		[ -d "${RESULTS}" ] && rm -rf "${RESULTS}"
		mkdir -p "${RESULTS}"
	fi

	if [ -z "$(which docker)" ]; then
		abort 1 "Please install docker!"
	fi

	# auto pull/build images (only if missing)
	if echo "${VDB_TAG}${SEAT_TAG}${FEEDER_TAG}" | grep -q "latest"; then
		build_images "${force}"
	else
		pull_images "${force}"
	fi

	network_setup "${DOCKER_NETWORK}"
	return 0
}

it_start() {
	local force="$1"
	set -e

	# initial cleanup for images/containers
	cleanup "$force"

	# ensure images are pulled, containers are started
	it_init "$force"

	start_containers
	return $?
}

it_stop() {
	set -e

	# cleanup for containres only
	cleanup 0
	return $?
}

it_status() {
	local logs="$1"
	echo
	echo "### Docker Images"
	docker images --format 'table {{.Repository}}:{{.Tag}}\t{{.Size}}\t{{.CreatedSince}}' | grep "${DOCKER_REPO}/eclipse/kuksa\.val"
	echo "-----------------------"
	echo "### Docker Containers"
	check_it_containers 1
	echo "-----------------------"
	echo
	if [ "${logs}" = "1" ]; then
		echo "### [${VDB_CONTAINER}] Logs:"
		docker logs $DOCKER_LOG "${VDB_CONTAINER}"
		echo "-----------------------"
		echo
		echo "### [${SEAT_CONTAINER}] Logs:"
		docker logs $DOCKER_LOG "${SEAT_CONTAINER}"
		echo "-----------------------"
		echo
		echo
		echo "### [${FEEDER_CONTAINER}] Logs:"
		docker logs $DOCKER_LOG "${FEEDER_CONTAINER}"
		echo "-----------------------"
		echo
	fi
}

it_cleanup() {
	local force="$1"
	cleanup "$force"
}

it_usage() {
	# {--force}

	echo "Usage:  $0 {Options} [ init | start | stop | status | cleanup ]"
	echo
	echo "Options:"
	echo "  --force  for 'init' and 'cleanup' commands, forces rebuilding/pulling/removing VAL images"
	echo "  --logs   for 'status' command, shows docker logs per var container"
	echo "  --help   Prints this message and exit."
	echo
	echo "Commands:"
	echo "  init     Pulls VAL images from a repository or builds them if missing (use --force to force init)"
	echo "  start    Starts VAL Containers (also implies init)"
	echo "  stop     Stops VAL Containers"
	echo "  status   Shows status of VAL Containers. Use --log to see last logs from VAL containers"
	echo "  cleanup  Removes VAL Containers. Use --force to also remove configured VAL images"
}

# FIXME: BROKER_ADDR=${VDB_CONTAINER}:55555 should be changed in case of host network...
export DOCKER_OPT="--network ${DOCKER_NETWORK} ${DOCKER_OPT}"

# parse options in $FORCE and $CMD
CMD=""
FORCE=0
LOGS=0
while [ -n "$1" ]; do
	if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
		it_usage
		exit 0
	elif [ "$1" = "--force" ] || [ "$1" = "-f" ]; then
		FORCE=1
	elif [ "$1" = "--logs" ] || [ "$1" = "-l" ]; then
		LOGS=1
	else
		CMD="$1"
	fi
	shift
done

rc=0
case "${CMD}" in
init) # handle source
	it_init $FORCE
	rc=$?
	;;
start)
	it_start $FORCE
	rc=$?
	;;
stop)
	it_stop $FORCE
	rc=$?
	;;
status)
	it_status $LOGS
	rc=$?
	;;
cleanup)
	it_cleanup $FORCE
	rc=$?
	;;
help)
	it_usage
	exit 0
	;;
*)
	echo "Invalid argument: ${CMD}"
	it_usage
	exit 1
	;;
esac

exit $rc
