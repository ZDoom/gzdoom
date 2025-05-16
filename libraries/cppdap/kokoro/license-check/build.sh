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

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd )"
ROOT_DIR="$( cd "${SCRIPT_DIR}/../.." >/dev/null 2>&1 && pwd )"

docker run --rm -i \
  --volume "${ROOT_DIR}:${ROOT_DIR}:ro" \
  --workdir "${ROOT_DIR}" \
  us-east4-docker.pkg.dev/shaderc-build/radial-docker/ubuntu-24.04-amd64/license-checker
