# Building Lisper

This document provides detailed instructions for building Lisper on various platforms.

## Table of Contents

- [Prerequisites](#prerequisites)
- [macOS](#macos)
- [Linux](#linux)
- [Windows](#windows)
- [Build Options](#build-options)
- [Hardware Acceleration](#hardware-acceleration)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required
- CMake 3.14 or higher
- C++17 compatible compiler
- FFmpeg (for audio/video processing)

### Optional
- SDL2 2.0+ (for GUI and live transcription)
- OpenVINO (for Intel hardware acceleration)
- CUDA Toolkit (for NVIDIA GPU support)

## macOS

### Install Dependencies

Using Homebrew:
```bash
# Install Xcode Command Line Tools (if not already installed)
xcode-select --install

# Install dependencies
brew install cmake ffmpeg sdl2

# Optional: Install pkg-config for better library detection
brew install pkg-config
```

### Build Instructions

```bash
# Clone the repository
git clone --recursive https://github.com/Lagmator22/Lisper.git
cd Lisper

# Configure and build
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLISPER_ENABLE_LIVE=ON
cmake --build build -j$(sysctl -n hw.ncpu)

# Install (optional)
sudo cmake --install build --prefix /usr/local
```

### Apple Silicon (M1/M2/M3) Optimization

```bash
# Build with Apple Silicon optimizations
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DWHISPER_METAL=ON \
  -DLISPER_ENABLE_LIVE=ON
cmake --build build -j
```

## Linux

### Ubuntu/Debian

```bash
# Update package lists
sudo apt-get update

# Install build tools
sudo apt-get install -y \
  build-essential \
  cmake \
  git

# Install dependencies
sudo apt-get install -y \
  ffmpeg \
  libsdl2-dev \
  libsdl2-ttf-dev

# Clone and build
git clone --recursive https://github.com/Lagmator22/Lisper.git
cd Lisper

cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLISPER_ENABLE_LIVE=ON
cmake --build build -j$(nproc)
```

### Fedora/RHEL/CentOS

```bash
# Install development tools
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git

# Install dependencies
sudo dnf install \
  ffmpeg \
  ffmpeg-devel \
  SDL2-devel \
  SDL2_ttf-devel

# Build
git clone --recursive https://github.com/Lagmator22/Lisper.git
cd Lisper

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Arch Linux

```bash
# Install dependencies
sudo pacman -S base-devel cmake git ffmpeg sdl2 sdl2_ttf

# Build
git clone --recursive https://github.com/Lagmator22/Lisper.git
cd Lisper

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Windows

### Using Visual Studio

#### Prerequisites
1. Install [Visual Studio 2019 or 2022](https://visualstudio.microsoft.com/)
   - Select "Desktop development with C++" workload
   - Include CMake tools for Windows

2. Install [Git for Windows](https://git-scm.com/download/win)

3. Install FFmpeg:
   ```powershell
   # Using Chocolatey (if installed)
   choco install ffmpeg

   # Or download from https://ffmpeg.org/download.html
   # Add to PATH: C:\ffmpeg\bin
   ```

4. Install SDL2 (optional, for GUI):
   - Download SDL2 development libraries from https://www.libsdl.org/download-2.0.php
   - Extract to `C:\SDL2`
   - Add to PATH: `C:\SDL2\lib\x64`

#### Build with Visual Studio

```powershell
# Clone repository
git clone --recursive https://github.com/Lagmator22/Lisper.git
cd Lisper

# Generate Visual Studio solution
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build from command line
cmake --build build --config Release -j

# Or open build\Lisper.sln in Visual Studio
```

### Using MSYS2/MinGW

```bash
# Install MSYS2 from https://www.msys2.org/

# Update MSYS2
pacman -Syu

# Install build tools and dependencies
pacman -S mingw-w64-x86_64-gcc \
         mingw-w64-x86_64-cmake \
         mingw-w64-x86_64-ffmpeg \
         mingw-w64-x86_64-SDL2

# Clone and build
git clone --recursive https://github.com/Lagmator22/Lisper.git
cd Lisper

cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Build Options

### CMake Configuration Options

```bash
# Basic options
-DCMAKE_BUILD_TYPE=Release|Debug|RelWithDebInfo  # Build type
-DLISPER_ENABLE_LIVE=ON|OFF                      # Enable live transcription (requires SDL2)
-DLISPER_BUILD_GUI=ON|OFF                        # Build GUI application
-DLISPER_BUILD_TESTS=ON|OFF                      # Build test suite

# Whisper.cpp options
-DWHISPER_CUBLAS=ON                              # NVIDIA GPU support (requires CUDA)
-DWHISPER_OPENVINO=ON                            # Intel OpenVINO support
-DWHISPER_METAL=ON                               # Apple Metal support (macOS)
-DWHISPER_CLBLAST=ON                             # OpenCL support
-DWHISPER_NO_ACCELERATE=ON                       # Disable Apple Accelerate framework

# Optimization options
-DWHISPER_AVX=ON                                 # Enable AVX instructions
-DWHISPER_AVX2=ON                                # Enable AVX2 instructions
-DWHISPER_FMA=ON                                 # Enable FMA instructions
-DWHISPER_F16C=ON                                # Enable F16C instructions
```

### Example Configurations

#### Maximum Performance (x86_64)
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DWHISPER_AVX2=ON \
  -DWHISPER_FMA=ON \
  -DWHISPER_F16C=ON \
  -DLISPER_ENABLE_LIVE=ON
```

#### Debug Build
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLISPER_BUILD_TESTS=ON
```

#### Static Build
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
```

## Hardware Acceleration

### NVIDIA GPU (CUDA)

```bash
# Ensure CUDA Toolkit is installed
# Download from https://developer.nvidia.com/cuda-toolkit

# Build with CUDA support
cmake -B build \
  -DWHISPER_CUBLAS=ON \
  -DCMAKE_CUDA_ARCHITECTURES="75;80;86"  # Adjust for your GPU
cmake --build build -j
```

### Intel OpenVINO

```bash
# Install OpenVINO
# Follow instructions at https://docs.openvino.ai/latest/openvino_docs_install_guides.html

# Source OpenVINO environment
source /opt/intel/openvino/setupvars.sh  # Linux/macOS
# or
"C:\Program Files (x86)\Intel\openvino\bin\setupvars.bat"  # Windows

# Convert model to OpenVINO format
cd third_party/whisper.cpp/models
python3 convert-whisper-to-openvino.py --model base.en

# Build with OpenVINO support
cd ../../..
cmake -B build -DWHISPER_OPENVINO=ON
cmake --build build -j
```

### AMD GPU (ROCm)

```bash
# Install ROCm (Linux only)
# Follow instructions at https://docs.amd.com/

# Build with CLBlast for OpenCL support
cmake -B build -DWHISPER_CLBLAST=ON
cmake --build build -j
```

## Troubleshooting

### Common Issues

#### FFmpeg Not Found
```bash
# Linux: Install development headers
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswresample-dev

# macOS: Ensure pkg-config is installed
brew install pkg-config

# Windows: Add FFmpeg to PATH or specify manually
cmake -B build -DFFMPEG_DIR="C:/ffmpeg"
```

#### SDL2 Not Found
```bash
# Specify SDL2 path manually
cmake -B build -DSDL2_DIR="/path/to/SDL2"
```

#### Compilation Errors with AVX/AVX2
```bash
# Disable AVX instructions for older CPUs
cmake -B build \
  -DWHISPER_AVX=OFF \
  -DWHISPER_AVX2=OFF \
  -DWHISPER_FMA=OFF
```

#### Link Errors on Windows
```bash
# Use static runtime libraries
cmake -B build \
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>"
```

### Performance Testing

After building, test performance with different configurations:

```bash
# Test CPU performance
./build/lisper --model-profile fast test.wav --device cpu

# Test GPU performance (if available)
./build/lisper --model-profile fast test.wav --device gpu

# Benchmark with different thread counts
for threads in 1 2 4 8; do
  echo "Testing with $threads threads:"
  time ./build/lisper -m models/ggml-base.en.bin test.wav -t $threads
done
```

### Debug Build Verification

```bash
# Build with debug symbols and sanitizers
cmake -B build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined" \
  -DCMAKE_C_FLAGS="-fsanitize=address -fsanitize=undefined"
cmake --build build-debug

# Run with debug output
WHISPER_DEBUG=1 ./build-debug/lisper --help
```

## Cross-Compilation

### Raspberry Pi (ARM)

```bash
# On the host machine
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf.cmake \
  -DWHISPER_NO_ACCELERATE=ON \
  -DWHISPER_NO_AVX=ON \
  -DWHISPER_NO_AVX2=ON
cmake --build build -j
```

### Android (using NDK)

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24
cmake --build build
```

## Docker Build

```dockerfile
# Dockerfile example
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    build-essential cmake git ffmpeg libsdl2-dev
WORKDIR /app
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j
```

Build with Docker:
```bash
docker build -t lisper .
docker run -it lisper ./build/lisper --help
```

## Verification

After building, verify the installation:

```bash
# Check CLI
./build/lisper --help
./build/lisper --list-model-profiles

# Check GUI (if built with SDL2)
./build/lisper_gui

# Run a simple transcription test
# First download a test model
cd models
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
cd ..

# Create a test audio file or use existing
./build/lisper -m models/ggml-base.en.bin test_audio.wav
```

## Support

If you encounter build issues:
1. Check the [GitHub Issues](https://github.com/Lagmator22/Lisper/issues)
2. Ensure all submodules are updated: `git submodule update --init --recursive`
3. Try a clean build: `rm -rf build && cmake -B build`
4. Enable verbose output: `cmake --build build --verbose`