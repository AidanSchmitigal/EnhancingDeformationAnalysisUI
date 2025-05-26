#!/bin/bash
set -e

TENSORFLOW_VERSION="2.18.0"

# Create directory structure
mkdir -p libs/tensorflow
mkdir -p libs/opencv

detect_platform() {
  UNAME=$(uname)
  ARCH=$(uname -m)
  if [[ "$UNAME" == "Linux" ]]; then
    echo "linux"
  elif [[ "$UNAME" == "Darwin" ]]; then
    if [[ "$ARCH" == "arm64" ]]; then
      echo "macos-arm64"
    else
      echo "macos-x86"
    fi
  else
    echo "unsupported"
  fi
}

install_tensorflow() {
  PLATFORM=$1
  TF_DIR="libs/tensorflow/$PLATFORM"
  mkdir -p "$TF_DIR"

  case "$PLATFORM" in
    linux)
      URL="https://storage.googleapis.com/tensorflow/versions/${TENSORFLOW_VERSION}/libtensorflow-cpu-linux-x86_64.tar.gz"
      ;;
    macos-x86)
      URL="https://storage.googleapis.com/tensorflow/versions/2.16.2/libtensorflow-cpu-darwin-x86_64.tar.gz"
      ;;
    macos-arm64)
      URL="https://storage.googleapis.com/tensorflow/versions/${TENSORFLOW_VERSION}/libtensorflow-cpu-darwin-arm64.tar.gz"
      ;;
    *)
      echo "Unsupported platform: $PLATFORM"
      return 1
      ;;
  esac

  echo "Downloading TensorFlow C API for $PLATFORM..."
  curl -L "$URL" -o tf.tar.gz
  tar -xzf tf.tar.gz -C "$TF_DIR"
  rm tf.tar.gz
  echo "TensorFlow installed in $TF_DIR"
}

PLATFORM=$(detect_platform)
echo "Detected platform: $PLATFORM"

install_tensorflow "$PLATFORM"
