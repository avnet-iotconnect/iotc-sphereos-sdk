#!/bin/bash

#NOTE: This script is to be used in the Linux Github Action workflow.

set -e

pushd "$(dirname $0)"/../iotc-azsphere-sdk

# initial cleanup

rm -rf iotc-c-lib cJSON

# clone the dependency repos
git clone --depth 1 --branch protocol-2.0 git://github.com/Avnet/iotc-c-lib.git
git clone --depth 1 --branch git://github.com/DaveGamble/cJSON.git

popd >/dev/null
echo Done
