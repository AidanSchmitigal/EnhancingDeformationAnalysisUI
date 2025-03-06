import os
import platform
import subprocess

# Handles windows, mac, and anything based on debian
def check_if_installed():
    opencv = False
    tensorflow = False
    if platform.system() == "Windows":
        if os.environ.get("OPENCV_DIR") and os.environ["PATH"].find("opencv\\build\\x64\\vc16\\bin") != -1:
            opencv = True
        if os.environ["PATH"].find("tensorflow/include") != -1:
            tensorflow = True
    elif platform.system() == "Darwin":
        if subprocess.call(["brew", "list", "opencv"]) == 0:
            opencv = True
        if os.environ["LD_LIBRARY_PATH"].find("tensorflow") != -1:
            tensorflow = True
    else:
        if subprocess.call(["dpkg", "-s", "libopencv-dev"]) == 0:
            opencv = True
        if os.environ["LD_LIBRARY_PATH"].find("tensorflow") != -1:
            tensorflow = True
    return (opencv, tensorflow)

def install_packages():
    print(f"opencv_dir: {os.environ.get("OPENCV_DIR")}, opencv in path: {os.environ["PATH"].find("opencv\\build\\x64\\vc16\\bin")}")
    print(f"path: {os.environ["PATH"]}")
    (opencv, tensorflow) = check_if_installed()
    if opencv and tensorflow:
        print("OpenCV and TensorFlow are already installed.")
        return

    # Define installation directories
    install_dir = os.path.abspath("third_party")
    os.makedirs(install_dir, exist_ok=True)

    paths = []
    if not opencv:
        print("Installing OpenCV...")
        # Install OpenCV
        if platform.system() == "Windows":
            opencv_url = "https://github.com/opencv/opencv/releases/download/4.10.0/opencv-4.10.0-windows.exe"
            opencv_filename = os.path.join(install_dir, opencv_url.split("/")[-1])
            subprocess.check_call(["curl", "-L", opencv_url, "-o", opencv_filename])
            subprocess.check_call([opencv_filename, "/S", "/D=" + os.path.join(install_dir, "opencv")])
            opencv_path = os.path.join(install_dir, "opencv", "build", "x64", "vc16", "bin")
            opencv_dir = os.path.join(install_dir, "opencv")
            print(f"OpenCV installed to {opencv_dir}")
            paths.append(opencv_path)
            paths.append(opencv_dir)
        elif platform.system() == "Darwin":
            subprocess.check_call(["brew", "install", "opencv"])
        else:
            subprocess.check_call(["sudo", "apt", "install", "-y", "libopencv-dev"])

    if not tensorflow:
        print("Installing TensorFlow...")
        # Install TensorFlow C API
        if platform.system() == "Windows":
            tf_url = "https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-gpu-windows-x86_64-2.10.0.zip"
        elif platform.system() == "Darwin":
            tf_url = "https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-cpu-darwin-arm64-2.18.0.tar.gz"
        else:
            tf_url = "https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-gpu-linux-x86_64-2.18.0.tar.gz"

        tf_filename = os.path.join(install_dir, tf_url.split("/")[-1])
        subprocess.check_call(["curl", "-L", tf_url, "-o", tf_filename])
        if tf_filename.endswith(".zip"):
            command = "Expand-Archive -Force " + tf_filename + " -DestinationPath " + os.path.join(install_dir, "tensorflow")
            subprocess.check_call(["pwsh", "-Command", command])
            print(f"TensorFlow installed to {os.path.join(install_dir, 'tensorflow')}")
            paths.append(os.path.join(install_dir, "tensorflow", "lib"))
            paths.append(os.path.join(install_dir, "tensorflow", "include"))
        else:
            subprocess.check_call(["tar", "-xzf", tf_filename, "-C", os.path.join(install_dir, "tensorflow")])

    if tensorflow or opencv:
        print("Installation complete.")
        print("Please add the following paths to the PATH environment variable:")
    else:
        print("No packages were installed.")

    for path in paths:
        print(path)

install_packages()