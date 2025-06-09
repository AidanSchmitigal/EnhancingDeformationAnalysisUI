# scripts/setup_pytorch.py
import os
import sys
import shutil
import tarfile  # Though most are zips for libtorch
import zipfile
import platform
import requests
from tqdm import tqdm

# LibTorch (PyTorch C++) package URLs
# Check https://pytorch.org/get-started/locally/ for the latest versions and URLs
# Select LibTorch, your OS, Package (Zip), Language (C++/Java), Compute Platform (CPU/CUDA)
# --- Version 2.3.0 (example, update as needed) ---
LIBTORCH_PACKAGES = {
    "macos_arm64_cpu": "https://download.pytorch.org/libtorch/cpu/libtorch-macos-arm64-2.7.0.zip",
    "linux_x86_64_cpu": "https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.7.0%2Bcpu.zip",
    "linux_x86_64_cu118": "https://download.pytorch.org/libtorch/cu118/libtorch-cxx11-abi-shared-with-deps-2.7.0%2Bcu118.zip",
    "linux_x86_64_cu126": "https://download.pytorch.org/libtorch/cu126/libtorch-cxx11-abi-shared-with-deps-2.7.0%2Bcu126.zip",
    "linux_x86_64_cu128": "https://download.pytorch.org/libtorch/cu128/libtorch-cxx11-abi-shared-with-deps-2.7.0%2Bcu128.zip",
    "windows_x86_64_cpu": "https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-2.7.0%2Bcpu.zip",
    "windows_x86_64_cu118": "https://download.pytorch.org/libtorch/cu118/libtorch-win-shared-with-deps-2.7.0%2Bcu118.zip",
    "windows_x86_64_cu126": "https://download.pytorch.org/libtorch/cu126/libtorch-win-shared-with-deps-2.7.0%2Bcu126.zip",
    "windows_x86_64_cu128": "https://download.pytorch.org/libtorch/cu128/libtorch-win-shared-with-deps-2.7.0%2Bcu128.zip",
}

# Directory name inside libs/pytorch for the extracted content
# e.g. libs/pytorch/libtorch_cpu_linux_x86_64
LIBTORCH_SUBDIR_FORMAT = "libtorch_{compute}_{os}_{arch}"


def get_libtorch_package_info(target_os, target_arch, compute="cpu", cuda_version=None):
    key_suffix = compute
    if compute == "gpu" and cuda_version:
        # Normalize cuda_version, e.g., "11.8" -> "cu118"
        key_suffix = f"cu{cuda_version.replace('.', '')}"
    elif compute == "gpu" and not cuda_version:
        raise ValueError("CUDA version must be specified for GPU compute.")

    key_base = f"{target_os}_{target_arch}"
    key = f"{key_base}_{key_suffix}"

    if key not in LIBTORCH_PACKAGES:
        # Fallback for macos_arm64 if specific GPU version not listed, try CPU
        if target_os == "macos" and target_arch == "arm64" and compute == "gpu":
            print(f"Warning: GPU version for {key_base} not explicitly listed. Falling back to CPU for macos_arm64.")
            key = f"{key_base}_cpu"

        if key not in LIBTORCH_PACKAGES:  # Check again after fallback
            # Special handling for macos_arm64 CPU if it's not in the list (e.g. if only nightly is there)
            if target_os == "macos" and target_arch == "arm64" and "macos_arm64_cpu" not in LIBTORCH_PACKAGES:
                print(f"Warning: No direct stable LibTorch package for {target_os}-{target_arch} with {compute}. " "Check PyTorch website for availability or consider building from source.")
                # Attempt to use the placeholder if it exists, otherwise error
                if "macos_arm64_cpu" in LIBTORCH_PACKAGES:  # Should match the key in LIBTORCH_PACKAGES
                    key = "macos_arm64_cpu"
                else:
                    raise ValueError(f"Unsupported LibTorch configuration: {key}. Please update LIBTORCH_PACKAGES.")
            else:
                raise ValueError(f"Unsupported LibTorch configuration: {key}. Please update LIBTORCH_PACKAGES.")

    url = LIBTORCH_PACKAGES[key]

    # Define the subdirectory name where libtorch will be extracted
    # This helps if you want to switch between CPU/GPU versions or different CUDA versions
    # For CMake, we typically just want one active "libtorch" directory.
    # The convention is that the extracted zip has a "libtorch" folder at its root.
    # We will extract into libs/pytorch/libtorch_dist_name/ and then ensure
    # libs/pytorch/libtorch points to the actual contents (lib, include, share)

    # The actual directory containing include/, lib/, share/ will be called 'libtorch'
    # We'll place it under libs/pytorch/ for consistency.
    # So, the final CMake Torch_DIR could be libs/pytorch/libtorch/share/cmake/Torch

    cmake_torch_dir_parent = "libtorch"  # The name of the folder we want that contains include/, lib/, share/

    return "2.7.0", url, cmake_torch_dir_parent


def download_file(url, dest_path):
    print(f"Downloading {url} to {dest_path}...")
    try:
        response = requests.get(url, stream=True, timeout=300)  # 5 min timeout
        response.raise_for_status()
    except requests.exceptions.RequestException as e:
        print(f"Error downloading {url}: {e}")
        # If it's a known "nightly" URL that might 404, provide a hint
        if "nightly" in url:
            print("This might be a nightly URL which can change or become unavailable.")
            print("Please verify the URL at https://pytorch.org/get-started/pytorch-nightly/")
        sys.exit(1)

    total_size = int(response.headers.get("content-length", 0))

    with open(dest_path, "wb") as f, tqdm(desc=os.path.basename(dest_path), total=total_size, unit="iB", unit_scale=True, unit_divisor=1024) as bar:
        for chunk in response.iter_content(chunk_size=8192):
            f.write(chunk)
            bar.update(len(chunk))


def extract_archive(archive_path, final_extract_target_dir):
    print(f"Extracting {archive_path} to prepare for {final_extract_target_dir}...")

    if os.path.exists(final_extract_target_dir):
        print(f"Removing existing directory: {final_extract_target_dir}")
        shutil.rmtree(final_extract_target_dir)

    # Temporary extraction location
    temp_extract_dir_base = os.path.join(os.path.dirname(final_extract_target_dir), "temp_pytorch_extract_base")
    if os.path.exists(temp_extract_dir_base):
        shutil.rmtree(temp_extract_dir_base)
    os.makedirs(temp_extract_dir_base, exist_ok=True)

    if archive_path.endswith(".tar.gz"):
        with tarfile.open(archive_path, "r:gz") as tar:
            tar.extractall(path=temp_extract_dir_base)
    elif archive_path.endswith(".zip"):
        with zipfile.ZipFile(archive_path, "r") as zip_ref:
            zip_ref.extractall(path=temp_extract_dir_base)
    else:
        shutil.rmtree(temp_extract_dir_base)
        raise ValueError(f"Unsupported archive format: {archive_path}")

    # LibTorch zips typically extract into a single subfolder named 'libtorch'
    extracted_items = os.listdir(temp_extract_dir_base)
    if len(extracted_items) == 1 and os.path.isdir(os.path.join(temp_extract_dir_base, extracted_items[0])) and extracted_items[0].lower() == "libtorch":
        content_source_dir = os.path.join(temp_extract_dir_base, extracted_items[0])
        print(f"Found 'libtorch' subdirectory: {content_source_dir}")
    else:
        # This case should ideally not happen with official libtorch zips
        print(f"Warning: Expected a single 'libtorch' subdirectory in the archive, but found: {extracted_items}. Assuming contents are directly in temp_extract_dir_base.")
        content_source_dir = temp_extract_dir_base  # Or handle error

    # Move the contents of 'libtorch' (or content_source_dir) to final_extract_target_dir
    print(f"Moving contents from {content_source_dir} to {final_extract_target_dir}")
    shutil.move(content_source_dir, final_extract_target_dir)

    shutil.rmtree(temp_extract_dir_base)
    print(f"LibTorch extracted and organized at {final_extract_target_dir}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python setup_pytorch.py <os_type> <arch_type> [--compute <cpu|gpu>] [--cuda <cuda_version>]")
        print("  os_type: linux, macos, windows")
        print("  arch_type: x86_64, arm64")
        print("  --compute: cpu (default) or gpu")
        print("  --cuda: e.g., 11.8, 12.1 (required if --compute gpu)")
        sys.exit(1)

    target_os = sys.argv[1]
    target_arch = sys.argv[2]

    compute_platform = "cpu"
    cuda_ver = None

    if "--compute" in sys.argv:
        try:
            compute_idx = sys.argv.index("--compute")
            compute_platform = sys.argv[compute_idx + 1]
            if compute_platform not in ["cpu", "gpu"]:
                raise ValueError("Invalid compute platform.")
        except (IndexError, ValueError) as e:
            print(f"Error parsing --compute: {e}. Must be 'cpu' or 'gpu'.")
            sys.exit(1)

    if "--cuda" in sys.argv:
        try:
            cuda_idx = sys.argv.index("--cuda")
            cuda_ver = sys.argv[cuda_idx + 1]
        except IndexError:
            print("Error: --cuda option requires a version number.")
            sys.exit(1)

    if compute_platform == "gpu" and not cuda_ver:
        print("Error: --cuda <version> is required when --compute is gpu.")
        sys.exit(1)

    # Special handling for macOS arm64 - LibTorch GPU builds are not standard
    if target_os == "macos" and target_arch == "arm64" and compute_platform == "gpu":
        print("Warning: LibTorch for macOS arm64 GPU is not officially supported in pre-built stable binaries.")
        print("Attempting to find a CPU version or a nightly GPU if configured.")
        # The get_libtorch_package_info will try to fall back or use a placeholder.

    try:
        torch_version, torch_url, torch_cmake_dir_parent_name = get_libtorch_package_info(target_os, target_arch, compute_platform, cuda_ver)
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)

    project_root = os.path.dirname(os.path.abspath(__file__))  # Root of your project

    # The final directory will be libs/pytorch/libtorch, containing include/, lib/, share/
    # This makes Torch_DIR in CMake: ${CMAKE_SOURCE_DIR}/libs/pytorch/libtorch/share/cmake/Torch
    pytorch_base_dir = os.path.join(project_root, "libs", "pytorch")
    final_libtorch_content_dir = os.path.join(pytorch_base_dir, torch_cmake_dir_parent_name)  # e.g. libs/pytorch/libtorch

    os.makedirs(pytorch_base_dir, exist_ok=True)

    archive_name = os.path.basename(torch_url).split("?")[0]  # Handle potential query params in URL
    download_dir = os.path.join(project_root, "temp_downloads")
    os.makedirs(download_dir, exist_ok=True)
    archive_path = os.path.join(download_dir, archive_name)

    if not os.path.exists(archive_path):
        download_file(torch_url, archive_path)
    else:
        print(f"Archive {archive_path} already exists. Skipping download.")

    extract_archive(archive_path, final_libtorch_content_dir)

    print(f"LibTorch {torch_version} ({compute_platform.upper()}{'-CUDA'+cuda_ver if cuda_ver else ''}) " f"for {target_os}-{target_arch} downloaded and extracted.")
    print(f"The LibTorch installation root is: {final_libtorch_content_dir}")
    print(f"For CMake, set Torch_DIR to: {os.path.join(final_libtorch_content_dir, 'share', 'cmake', 'Torch')}")
