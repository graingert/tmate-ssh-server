#!/bin/sh
set -e

if [ "${USE_PROXY_PROTOCOL}" == "1" ]; then
  set -- -x "$@"
fi

if [ "${HAS_WEBSOCKET}" == "1" ]; then
  set -- -w localhost "$@"
fi

if [ ! -z "${SSH_HOSTNAME}" ]; then
  set -- -h "${SSH_HOSTNAME}" "$@"
fi

exec tmate-ssh-server -p ${SSH_PORT:-2200} -k ${SSH_KEYS_PATH} "$@"