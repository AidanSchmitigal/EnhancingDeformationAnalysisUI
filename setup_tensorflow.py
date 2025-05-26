# scripts/setup_tensorflow.py
import os
import sys
import shutil
import tarfile
import zipfile
import platform
import requests
from tqdm import tqdm

# TensorFlow C library URLs and versions
TF_PACKAGES = {
    "linux_x86_64_cpu": ("2.18.0", "https://storage.googleapis.com/tensorflow/versions/2.18.0/libtensorflow-cpu-linux-x86_64.tar.gz"),
    "linux_x86_64_gpu": ("2.18.0", "https://storage.googleapis.com/tensorflow/versions/2.18.0/libtensorflow-gpu-linux-x86_64.tar.gz"),
    "macos_x86_64_cpu": ("2.16.2", "https://storage.googleapis.com/tensorflow/versions/2.16.2/libtensorflow-cpu-darwin-x86_64.tar.gz"),
    "macos_arm64_cpu": ("2.18.0", "https://storage.googleapis.com/tensorflow/versions/2.18.0/libtensorflow-cpu-darwin-arm64.tar.gz"),
    "windows_x86_64_cpu": ("2.18.1", "https://storage.googleapis.com/tensorflow/versions/2.18.1/libtensorflow-cpu-windows-x86_64.zip"),
    "windows_x86_64_gpu": ("2.10.0", "https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-gpu-windows-x86_64-2.10.0.zip"),
}


def get_tf_package_info(target_os, target_arch, use_gpu=False):
    key = None
    cmake_platform_suffix = None  # This will be part of the path: libs/tensorflow/<suffix>

    match (target_os, target_arch, use_gpu):
        case ("linux", "x86_64", False):
            key = "linux_x86_64_cpu"
            cmake_platform_suffix = "linux"
        case ("linux", "x86_64", True):
            key = "linux_x86_64_gpu"
            cmake_platform_suffix = "linux"
        case ("macos", "x86_64", False):
            key = "macos_x86_64_cpu"
            cmake_platform_suffix = "macos-x86"
        case ("macos", "arm64", False):
            key = "macos_arm64_cpu"
            cmake_platform_suffix = "macos-arm64"
        case ("windows", "x86_64", False):
            key = "windows_x86_64_cpu"
            cmake_platform_suffix = "windows"
        case ("windows", "x86_64", True):
            key = "windows_x86_64_gpu"
            cmake_platform_suffix = "windows"
        case _:
            raise ValueError(f"Unsupported OS/architecture combination: {target_os}/{target_arch} (GPU: {use_gpu})")

    version, url = TF_PACKAGES[key]
    return version, url, cmake_platform_suffix


def download_file(url, dest_path):
    print(f"Downloading {url} to {dest_path}...")
    response = requests.get(url, stream=True)
    response.raise_for_status()
    total_size = int(response.headers.get("content-length", 0))

    with open(dest_path, "wb") as f, tqdm(desc=os.path.basename(dest_path), total=total_size, unit="iB", unit_scale=True, unit_divisor=1024) as bar:
        for chunk in response.iter_content(chunk_size=8192):
            f.write(chunk)
            bar.update(len(chunk))


def extract_archive(archive_path, extract_to_dir):
    print(f"Extracting {archive_path} to {extract_to_dir}...")

    # Ensure extract_to_dir exists and is empty
    if os.path.exists(extract_to_dir):
        shutil.rmtree(extract_to_dir)
    os.makedirs(extract_to_dir, exist_ok=True)

    temp_extract_dir = os.path.join(os.path.dirname(extract_to_dir), "temp_tf_extract")
    if os.path.exists(temp_extract_dir):
        shutil.rmtree(temp_extract_dir)
    os.makedirs(temp_extract_dir, exist_ok=True)

    if archive_path.endswith(".tar.gz"):
        with tarfile.open(archive_path, "r:gz") as tar:
            tar.extractall(path=temp_extract_dir)
    elif archive_path.endswith(".zip"):
        with zipfile.ZipFile(archive_path, "r") as zip_ref:
            zip_ref.extractall(path=temp_extract_dir)
    else:
        shutil.rmtree(temp_extract_dir)
        raise ValueError(f"Unsupported archive format: {archive_path}")

    # Archives might have a root folder. We want contents directly in extract_to_dir.
    # e.g. libtensorflow-cpu-windows-x86_64.zip extracts to a folder.
    # The .tar.gz files for Linux/macOS extract 'include', 'lib', etc. in the current dir.
    # We handle both by checking if there's a single subdirectory in temp_extract_dir.

    extracted_items = os.listdir(temp_extract_dir)
    content_source_dir = temp_extract_dir

    if len(extracted_items) == 1 and os.path.isdir(os.path.join(temp_extract_dir, extracted_items[0])):
        # Archive extracted into a single subfolder (e.g., typical for zip files)
        content_source_dir = os.path.join(temp_extract_dir, extracted_items[0])

    # Copy contents (include/, lib/, tensorflow.dll, etc.) to extract_to_dir
    for item in os.listdir(content_source_dir):
        s = os.path.join(content_source_dir, item)
        d = os.path.join(extract_to_dir, item)
        if os.path.isdir(s):
            shutil.copytree(s, d, dirs_exist_ok=True)
        else:
            shutil.copy2(s, d)

    shutil.rmtree(temp_extract_dir)
    print(f"TensorFlow extracted and organized at {extract_to_dir}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python setup_tensorflow.py <os_type> <arch_type> [--gpu]")
        print("  os_type: linux, macos, windows")
        print("  arch_type: x86_64, arm64")
        sys.exit(1)

    target_os = sys.argv[1]
    target_arch = sys.argv[2]
    use_gpu = "--gpu" in sys.argv

    try:
        tf_version, tf_url, tf_cmake_platform = get_tf_package_info(target_os, target_arch, use_gpu)
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)

    project_root = os.path.dirname(os.path.abspath(__file__))
    print(f"Project root: {project_root}")
    tf_root_dir = os.path.join(project_root, "libs", "tensorflow", tf_cmake_platform)

    archive_name = os.path.basename(tf_url)
    download_dir = os.path.join(project_root, "temp_downloads")
    os.makedirs(download_dir, exist_ok=True)
    archive_path = os.path.join(download_dir, archive_name)

    if not os.path.exists(archive_path):  # Optional: avoid re-download if already present
        download_file(tf_url, archive_path)
    else:
        print(f"Archive {archive_path} already exists. Skipping download.")

    extract_archive(archive_path, tf_root_dir)

    print(f"TensorFlow {tf_version} ({'GPU' if use_gpu else 'CPU'}) for {target_os}-{target_arch} downloaded and extracted to {tf_root_dir}")
