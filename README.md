# Smart Tello Drone (C++ / Dear ImGui Rewrite)

A high-performance Ground Control Station (GCS) for the DJI Tello drone, written from scratch in C++ using Dear ImGui. This project replaces the original Python implementation, offering superior performance, a professional dockable user interface, and efficient true zero-copy GPU video rendering.

## Features

- **Custom UDP Protocol:** Direct, dependency-free implementation of the Tello SDK 1.3 protocol.
- **Hardware-Accelerated Video:** H.264 video streams are decoded via FFmpeg and uploaded directly to OpenGL textures for tearing-free, zero-copy display in Dear ImGui.
- **Advanced Telemetry Dashboard:** Parses state data at ~10Hz, featuring custom-drawn artificial horizons, real-time plotting, and battery status.
- **Professional UI:** Built with Dear ImGui (docking branch) using a custom dark theme (Catppuccin Mocha inspired). Layout is fully customizable, dockable, and floating window capable.
- **Flight Controls:** Virtual dual-joysticks (RC control) via UI and keyboard hotkeys (WASD, QE, Space, L, Esc).
- **Visual Odometry (SLAM):** Built-in monocular SLAM tracking using OpenCV optical flow, projecting movement onto an interactive 3D map.
- **Recording & Snapshots:** OpenCV-powered background thread for saving H.264 `.mp4` videos and `.png` snapshots without dropping GUI frames.
- **Data Replay Sessions:** Time-synchronized playback of previously recorded flight sessions, re-rendering video, telemetry, and SLAM plots exactly as they occurred live.

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

### Command Line Arguments

The application accepts optional CLI flags for different execution modes:

- `--help` or `-h`: Display the help message outlining the available CLI commands.
- `--simulate`: Launches the UI without attempting to connect to a drone. Assumes dummy parameters useful for testing layouts.
- `--replay <video.mp4> <telemetry.csv>`: Loads a past flight session. The video frames and CSV rows are synchronized and injected directly into the application timeline, simulating a live flight on your screen.

Example Replay:
```bash
./build/SmartTelloDrone --replay recordings/flight_vid.mp4 logs/flight_data.csv
```

## UI & Controls

The application relies heavily on keyboard override commands mapped directly to the UI panel:

| Action | Keybinding |
| :--- | :--- |
| **Takeoff** | `Spacebar` |
| **Land** | `L` |
| **Emergency Kill** | `Esc` |
| **Pitch Forward / Backward** | `W` / `S` |
| **Roll Left / Right** | `A` / `D` |
| **Throttle Up / Down** | `Up Arrow` / `Down Arrow` |
| **Yaw Left / Right** | `Left Arrow` / `Right Arrow` (or `Q` / `E`) |

> **Note:** SLAM map points require both an active video stream (**Drone -> Stream On**) and significant texture density in the camera view to lock onto trackable features.

## Output Directories

When running the application in live-flight mode, it will generate data in two folders at the project root:
- `logs/`: Contains `telemetry_YYYYMMDD_HHMMSS.csv` files with raw drone state data recorded at ~10Hz.
- `recordings/`: Contains saved `.mp4` videos and `.png` snapshots triggered from the Recording Panel.
