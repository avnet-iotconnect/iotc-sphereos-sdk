#!/bin/bash

set -e

pushd "$(dirname $0)"/../iotc-azsphere-sdk
# initial cleanup

rm -rf iotc-c-lib cJSON

# clone the dependency repos
git clone --depth 1 --branch protocol-2.0 git://github.com/Avnet/iotc-c-lib.git
git clone --depth 1 --branch v1.7.13 git://github.com/DaveGamble/cJSON.git

popd >/dev/null
echo Done