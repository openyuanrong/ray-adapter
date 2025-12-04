#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

readonly USAGE="
Usage: bash build.sh [-thdDcCrvPSbEm:]

Options:
    -t run test.
    -v the version of yuanrong
    -c coverage
    -C clean build environment
    -p the specified version of python runtime
    -o specify the output directory
    -h usage.
"

BASE_DIR=$(
    cd "$(dirname "$0")"
    pwd
)

BUILD_DIR=$BASE_DIR/build
OUTPUT_DIR=$BASE_DIR/output
COMMAND="build"
BUILD_VERSION="0.0.1"
PYTHON3_BIN_PATH="python3"
usage() {
    echo -e "$USAGE"
}

while getopts 'tv:cCp:o:h' opt; do
    case "$opt" in
    t)
        COMMAND="test"
        ;;
    v)
        BUILD_VERSION="${OPTARG}"
        export BUILD_VERSION="${OPTARG}"
        ;;
    c)
        COMMAND="coverage"
        ;;
    C)
        COMMAND="clean"
        ;;
    p)
        PYTHON3_BIN_PATH="${OPTARG}"
        ;;
    o)
        OUTPUT_DIR=$(readlink -f "${OPTARG}")
        ;;
    h)
        usage
        exit 0
        ;;
    *)
        log_error "invalid command: $opt"
        usage
        exit 1
        ;;
    esac
done

find_yr_python() {
    local yr_cmd
    yr_cmd=$(which yr 2>/dev/null) || {
        echo "ERROR: 'yr' command not found. Please install openyuanrong." >&2
        exit 1
    }
    echo "$(dirname "$yr_cmd")/python3"
}

function install_python_requirements() {
    $PYTHON3_BIN_PATH -m pip install pytest coverage
    $PYTHON3_BIN_PATH -m pip install -r requirements.txt
}

generate_python_coverage_report() {
    "$PYTHON3_BIN_PATH" -m coverage html -d "$COV_HTML_DIR"
    "$PYTHON3_BIN_PATH" -m coverage xml -o "$COV_HTML_DIR/coverage.xml"

    total_cov=$("$PYTHON3_BIN_PATH" -m coverage report | tail -n1 | awk '{print $4}' | tr -d '%')

    echo "[$(date '+%b %d %H:%M:%S')] Python coverage: ${total_cov}%"
    echo "python_coverage: ${total_cov}%" > "$COV_HTML_DIR/coverage_summary.txt"
    echo "[$(date '+%b %d %H:%M:%S')] Coverage report: $COV_HTML_DIR/index.html"
}

function run_python_coverage() {
    local module_name="ray_adapter"
    export PYTHONPATH="$BASE_DIR${PYTHONPATH:+:$PYTHONPATH}"
    "$PYTHON3_BIN_PATH" -m coverage run \
        --source="$module_name" \
        --omit="*/tests/*,*__init__.py" \
        -m pytest "$module_name/tests/" -v
}

if [ $COMMAND == "build" ]; then
    export BUILD_VERSION
    $PYTHON3_BIN_PATH setup.py bdist_wheel -b $BUILD_DIR -d $OUTPUT_DIR
elif [ $COMMAND == "test" ]; then
    PYTHON3_BIN_PATH=$(find_yr_python)
    export PYTHONPATH="$BASE_DIR"
    install_python_requirements
    "$PYTHON3_BIN_PATH" -m pytest -s -vv ray_adapter/tests/
elif [ $COMMAND == "coverage" ]; then
    PYTHON3_BIN_PATH=$(find_yr_python)
    install_python_requirements
    COV_HTML_DIR="$OUTPUT_DIR/coverage_html_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$COV_HTML_DIR"
    run_python_coverage
    generate_python_coverage_report
elif [ $COMMAND == "clean" ]; then
    python setup.py clean
    rm -rf $BUILD_DIR
    rm -rf $OUTPUT_DIR
fi
