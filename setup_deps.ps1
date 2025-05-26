$ErrorActionPreference = "Stop"
$TF_VERSION = "2.18.1"
$TF_DIR = "libs/tensorflow/windows"
$TF_URL = "https://storage.googleapis.com/tensorflow/versions/$TF_VERSION/libtensorflow-cpu-windows-x86_64.zip"

New-Item -ItemType Directory -Force -Path $TF_DIR

Write-Host "Downloading TensorFlow C API for Windows..."
Invoke-WebRequest -Uri $TF_URL -OutFile "tf.zip"

Write-Host "Extracting..."
Expand-Archive -LiteralPath "tf.zip" -DestinationPath $TF_DIR
Remove-Item "tf.zip"

Write-Host "TensorFlow installed in $TF_DIR"
