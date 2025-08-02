#!/bin/bash

# Variables
CONTAINER_NAME="spectator-build"
IMAGE_NAME="spectator-cpp-image"
CONTAINER_FILE_PATH="/home/ubuntu/spectator-cpp/cmake-build"
HOST_FILE_PATH="$PWD"

if [[ -z "$BUILD_TYPE" ]]; then
  BUILD_TYPE="Debug"
fi

# Build image if it does not exist
if [[ -z "$(docker images -q $IMAGE_NAME 2>/dev/null)" ]]; then
  echo "Docker image $IMAGE_NAME not found. Building..."
  docker build -t $IMAGE_NAME -f Dockerfiles/Ubuntu.Dockerfile .
fi

## Remove any previous container
docker rm $CONTAINER_NAME 2>/dev/null || true

## Run the container and execute build.sh inside the virtual environment
docker run --name $CONTAINER_NAME \
  --entrypoint bash $IMAGE_NAME -lc \
    "source /home/ubuntu/spectator-cpp/venv/bin/activate && set -x && BUILD_TYPE=$BUILD_TYPE bash /home/ubuntu/spectator-cpp/build.sh"

## Copy the build output from the container to the host
docker cp $CONTAINER_NAME:$CONTAINER_FILE_PATH $HOST_FILE_PATH

## Remove the container after copying
docker rm $CONTAINER_NAME