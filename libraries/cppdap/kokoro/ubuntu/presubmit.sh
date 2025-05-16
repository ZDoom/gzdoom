#!/bin/bash

# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e # Fail on any error.

ROOT_DIR=`pwd`
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd )"

# --privileged is required for some sanitizer builds, as they seem to require PTRACE privileges
docker run --rm -i \
  --privileged \
  --volume "${ROOT_DIR}:${ROOT_DIR}" \
  --volume "${KOKORO_ARTIFACTS_DIR}:/mnt/artifacts" \
  --workdir "${ROOT_DIR}" \
  --env BUILD_SYSTEM=$BUILD_SYSTEM \
  --env BUILD_TARGET_ARCH=$BUILD_TARGET_ARCH \
  --env BUILD_SANITIZER=$BUILD_SANITIZER \
  --entrypoint "${SCRIPT_DIR}/presubmit-docker.sh" \
  us-east4-docker.pkg.dev/shaderc-build/radial-docker/ubuntu-24.04-amd64/cpp-builder
