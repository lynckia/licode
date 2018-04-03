#!/usr/bin/env bash
ulimit -c unlimited
exec node $ERIZOJS_NODE_OPTIONS $*
