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
YR_PY=$(readlink -f $(which yr) | sed 's|/yr$|/python3|')
PYTHON3_BIN_PATH=""
if command -v yr >/dev/null 2>&1; then
    YR_PATH=$(which yr)
    CANDIDATE_PY="${YR_PATH%/yr}/python3"
    if [ -x "$CANDIDATE_PY" ] && "$CANDIDATE_PY" -c "import yr" 2>/dev/null; then
        PYTHON3_BIN_PATH="$CANDIDATE_PY"
    fi
fi

if [ -z "$PYTHON3_BIN_PATH" ]; then
    echo "[BUILD_INFO] No 'yr' found in PATH. Falling back to cached wheel..."

    ARCH=$(uname -m)
    if [ "$ARCH" = "x86_64" ]; then
        WHEEL_NAME="openyuanrong-0.6.0-cp39-cp39-manylinux_2_34_x86_64.whl"
    elif [ "$ARCH" = "aarch64" ]; then
        WHEEL_NAME="openyuanrong-0.6.0-cp39-cp39-manylinux_2_34_aarch64.whl"
    else
        echo "[BUILD_ERROR] Unsupported architecture: $ARCH" >&2
        exit 1
    fi

    YR_CACHE_URL=${YR_CACHE:-"https://build-logs.openeuler.openatom.cn:38080/temp-archived/openeuler/openYuanrong/yr_cache/$ARCH/$WHEEL_NAME"}
    YR_WHL_PATH="/tmp/$WHEEL_NAME"

    if [ ! -f "$YR_WHL_PATH" ]; then
        echo "[BUILD_INFO] Downloading yr wheel from: $YR_CACHE_URL"
        if ! curl -fL --retry 3 --connect-timeout 10 "$YR_CACHE_URL" -o "$YR_WHL_PATH"; then
            echo "[BUILD_ERROR] Failed to download yr wheel." >&2
            exit 1
        fi
    else
        echo "[BUILD_INFO] Using cached yr wheel: $YR_WHL_PATH"
    fi

    PYTHON3_BIN_PATH="/opt/buildtools/python3.9/bin/python3"
    if [ ! -x "$PYTHON3_BIN_PATH" ]; then
        echo "[BUILD_ERROR] Required Python not found: $PYTHON3_BIN_PATH" >&2
        exit 1
    fi

    "$PYTHON3_BIN_PATH" -m pip install "$YR_WHL_PATH" --no-deps --force-reinstall -q
    export PYTHONPATH="$BASE_DIR${PYTHONPATH:+:$PYTHONPATH}"
    echo "[BUILD_INFO] Installed yr from wheel; PYTHONPATH set to project root."
fi

if [ -z "$PYTHON3_BIN_PATH" ]; then
    shopt -s nullglob 2>/dev/null || true
    for py in \
        python3 \
        /opt/buildtools/python3.*/bin/python3 \
        /usr/bin/python3 \
        /usr/local/bin/python3; do
        if [ -x "$py" ] && "$py" -c "import yr" 2>/dev/null; then
            PYTHON3_BIN_PATH="$py"
            break
        fi
    done
fi

if [ -z "$PYTHON3_BIN_PATH" ]; then
    echo "[BUILD_ERROR] Cannot find a python3 with 'yr' installed." >&2
    echo "Please ensure 'yr' is available, or specify with -p <python-path>." >&2
    exit 1
fi

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

if [ $COMMAND == "build" ]; then
    export BUILD_VERSION
    $PYTHON3_BIN_PATH setup.py bdist_wheel -b $BUILD_DIR -d $OUTPUT_DIR
echo "[DEBUG] Using Python: $PYTHON3_BIN_PATH"
echo "[DEBUG] PYTHONPATH: $PYTHONPATH"
elif [ $COMMAND == "test" ]; then
     export PYTHONPATH
    "$PYTHON3_BIN_PATH" -m pytest -s -vv ray_adapter/tests/
elif [ $COMMAND == "coverage" ]; then
    MODULE_NAME="ray_adapter"
    BAZEL_BIN_DIR="$BASE_DIR/bazel-bin"
    TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
    COV_HTML_DIR="$OUTPUT_DIR/coverage_html_$TIMESTAMP"

    mkdir -p "$COV_HTML_DIR"

    $PYTHON3_BIN_PATH -m pip install -q pytest coverage

    if [[ ":$PYTHONPATH:" != *":$BASE_DIR:"* ]]; then
        export PYTHONPATH="$BASE_DIR${PYTHONPATH:+:$PYTHONPATH}"
    fi
    "$PYTHON3_BIN_PATH" -m coverage run \
        --source="$MODULE_NAME" \
        --omit="*/tests/*,*__init__.py" \
        -m pytest "$MODULE_NAME/tests/" -v

    $PYTHON3_BIN_PATH -m coverage html -d "$COV_HTML_DIR"
    $PYTHON3_BIN_PATH -m coverage xml -o "$COV_HTML_DIR/coverage.xml"


    TOTAL_COV=$($PYTHON3_BIN_PATH -m coverage report --format=total 2>/dev/null | tail -n1 | awk '{print $NF}')
    if [ -z "$TOTAL_COV" ]; then
        TOTAL_COV=$($PYTHON3_BIN_PATH -m coverage report | tail -n1 | awk '{print $4}')
    fi

    TOTAL_COV=${TOTAL_COV%\%}

    echo "[BUILD_INFO][$(date '+%b %d %H:%M:%S')] Python coverage: ${TOTAL_COV}%"
    echo "python_coverage: ${TOTAL_COV}%" > "$COV_HTML_DIR/coverage_summary.txt"
    echo "[BUILD_INFO][$(date '+%b %d %H:%M:%S')] Coverage report: $COV_HTML_DIR/index.html"
elif [ $COMMAND == "clean" ]; then
    python setup.py clean
    rm -rf $BUILD_DIR
    rm -rf $OUTPUT_DIR
fi
