#!/bin/bash

if [ -z "$BASH_VERSION" ]; then
	echo "Invalid shell, re-running using bash..."
	exec bash "$0" "$@"
	exit $?
fi
SRCLOC="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$SRCLOC/../../../build/utils/functions.sh"

prepareUpstreamFromTarArchive "$SRCLOC" "http://download.osmand.net/prebuilt/giflib-4.2.3.tar.bz2"
patchUpstream "$SRCLOC"
