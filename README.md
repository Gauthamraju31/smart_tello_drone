# Smart Tello Drone (C++ / Dear ImGui Rewrite)

A high-performance Ground Control Station (GCS) for the DJI Tello drone, written from scratch in C++ using Dear ImGui. This project replaces the original Python implementation, offering superior performance, a professional dockable user interface, and efficient true zero-copy GPU video rendering.

## Features

- **Custom UDP Protocol:** Direct, dependency-free implementation of the Tello SDK 1.3 protocol.
- **Hardware-Accelerated Video:** H.264 video streams are decoded via FFmpeg and uploaded directly to OpenGL textures for tearing-free, zero-copy display in Dear ImGui.
- **Advanced Telemetry Dashboard:** Parses state data at ~10Hz, featuring custom-drawn artificial horizons, real-time plotting, and battery status.
- **Professional UI:** Built with Dear ImGui (docking branch) using a custom dark theme (Catppuccin Mocha inspired). Layout is fully customizable, dockable, and floating window capable.
- **Flight Controls:** Virtual dual-joysticks (RC control) via UI and keyboard hotkeys (WASD, QE, Space, L, Esc).
- **Recording & Snapshots:** OpenCV-powered background thread for saving H.264 `.mp4` videos and `.png` snapshots without dropping GUI frames.
- **Extensible Architecture:** Designed with placeholder interfaces for AI edge processing (YOLOv8 tracking, SAM segmentation) and SLAM (ORB-SLAM3).

## Prerequisites (Linux)

You will need the following development libraries installed on your system:

- CMake (≥ 3.20)
- GCC/G++ (C++17 support)
- FFmpeg (`libavcodec`, `libavformat`, `libavutil`, `libswscale`)
- OpenCV 4
- spdlog
- yaml-cpp
- Eigen3
- OpenGL & Wayland/X11 Development Headers

*On Ubuntu/Debian:*
```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libopencv-dev libspdlog-dev libyaml-cpp-dev libeigen3-dev libgl1-mesa-dev libwayland-dev libxkbcommon-dev xorg-dev
```

## Building the Project

The UI components (Dear ImGui) and Windowing System (GLFW) are vendored as git submodules.

```bash
# 1. Clone the repository with submodules
git clone --recursive <your-repo-url> smart_tello_drone
cd smart_tello_drone

# If you already cloned without submodules:
# git submodule update --init --recursive

# 2. Build via CMake
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Running the Application

Connect your computer to the Tello's WiFi network (usually `TELLO-XXXXXX`), then launch the executable:

```bash
./build/SmartTelloDrone
```

### Simulation Mode

If you don't have a drone connected but want to test the GUI layout and resource usage:

```bash
./build/SmartTelloDrone --simulate
```
*(Note: `--simulate` flag handler is stubbed out in the current main.cpp, but the GUI runs safely unconnected.)*

## Architecture

- `src/core/`: Network communication (`TelloSDK`), Video Decoding (`VideoDecoder`), Telemetry CSV logging, and OpenCV `Recorder`.
- `src/gui/`: ImGui windows (`AppGui`, `VideoWindow`, `TelemetryPanel`, `ControlPanel`, `LogTerminal`, `SettingsPanel`).
- `src/ai/`: Placeholders inheriting from `FrameProcessor` enabling AI hooks before frames are rendered.
- `src/slam/`: SLAM interface and a `MapViewer` that renders 3D point clouds to an OpenGL Framebuffer Object (FBO) for display within ImGui.

## Output Directories

When running the application, it will generate data in two folders at the project root:
- `logs/`: Contains `telemetry_YYYYMMDD_HHMMSS.csv` files with raw drone state data.
- `recordings/`: Contains saved `.mp4` videos and `.png` snapshots.
