#!/bin/bash

set -e

. /staging-scripts/common.sh

# Register as a new runner if no existing credential files exist
if ! [ "$(ls -A ${RUNNER_CREDS_DIR})" ]; then
    if [ -z "$RUNNER_REG_TOKEN" ]; then
        echo "RUNNER_REG_TOKEN not set"
        exit 1
    fi
    if [ -z "$RUNNER_NAME" ]; then 
        echo "RUNNER_NAME not set"
        exit 1
    fi

    ./config.sh \
        --unattended \
        --url "https://github.com/${RUNNER_REPO:-"dosbox-staging/dosbox-staging"}" \
        --token "$RUNNER_REG_TOKEN" \
        --name "$RUNNER_NAME"
    
    # Move credentials to persistent volume 
    mv .credentials .credentials_rsaparams .runner "${RUNNER_CREDS_DIR}"
fi

# Ensure we have the latest version of the runner
runner_url=$(get_runner_download_url min)
rc="$?"
if [ "$rc" = "0" ]; then
    curl -L -o ../action-runner.tar.gz "$runner_url"
    tar -xzvf ../action-runner.tar.gz ./
fi

# Link credentials from persistent volume to runner root dir
[ -f ${RUNNER_CREDS_DIR}/.credentials ] && ln -sf ${RUNNER_CREDS_DIR}/.credentials ./
[ -f ${RUNNER_CREDS_DIR}/.credentials_rsaparams ] && ln -sf ${RUNNER_CREDS_DIR}/.credentials_rsaparams ./
[ -f ${RUNNER_CREDS_DIR}/.runner ] && ln -sf ${RUNNER_CREDS_DIR}/.runner ./

exec ./run.sh
