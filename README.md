
---

# Lightweight Unitree L2 & RealSense ROS 2 Recorder

A zero-overhead, multi-stage Dockerized ROS 2 (Jazzy) workspace designed for high-performance raw data acquisition from **Unitree L2 LiDARs** and **Intel RealSense** cameras.

Built specifically for resource-constrained environments — such as UAV onboard computers or custom Terrestrial Laser Scanner (TLS) rigs — this architecture ensures ultra-low latency recording and minimizes disk space usage through advanced compression pipelines.

---

## 🧠 Architecture Highlights

Recording dense `PointCloud2` and raw uncompressed `Image` messages directly to a `.bag` file can cause severe CPU bottlenecks, network latency, and massive storage consumption on embedded systems. This package solves this by splitting the workflow into two distinct phases:

1. **Raw Capture & Compressed Recording:** A highly optimized C++ node intercepts pure UDP packets from the LiDAR and publishes them instantly as custom `LidarMetadata`. Simultaneously, the RealSense camera utilizes `image_transport` plugins to publish JPEG/PNG compressed streams. **The recorder exclusively captures these lightweight `/compressed` and `/compressedDepth` topics**, completely bypassing the heavy raw images. This keeps CPU usage low and reduces `.bag` file sizes by up to 90% during in-field recording.
2. **Offline Unified Decoder (`decode_all.launch.py`):** A unified post-capture launch file. It translates the raw `.bag` LiDAR packets into standard `sensor_msgs/PointCloud2`, decodes the embedded IMU data, and simultaneously decompresses the recorded RealSense JPEG/PNG images back into pure raw formats. Everything is perfectly synchronized for SLAM algorithms like FAST-LIVO.
3. **Multi-stage Docker Build:** The final distributed image contains only the compiled binaries, RealSense drivers, and compression plugins. It leaves all C++ source code and build caches behind, resulting in a minimal footprint.

---

## ⚡ Quick Start (No compilation required)

You do not need to pollute your host machine with ROS 2, C++ compilers, or SDKs. Just pull the latest pre-built image:

```bash
docker pull lucasfroes232/unitree_realsense_jazzy:latest

```

Run the container with hardware acceleration and host network privileges:

```bash
docker run -it --net=host --privileged \
  -v /dev:/dev \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  lucasfroes232/unitree_realsense_jazzy:latest

```

---

## 🛠️ Usage Guide

### 1. Wake up the LiDAR motor

Before capturing data, ensure the LiDAR is spinning by sending the initialization command to port 6101. You can run the standard Unitree UDP example for this:

```bash
./unitree_examples/example_lidar_udp

```

### 2. Start the Raw Capture Node

Launch the lightweight node to start listening to the LiDAR's UDP broadcast on port 6201:

```bash
ros2 run unitree_lidar_ros2 unitree_lidar_ros2_node --ros-args -p udp_port:=6201

```

### 3. Launch the RealSense Camera

> ⚠️ **Pro-Tip for Embedded Systems:** To prevent USB bus saturation and keep the `.bag` file size manageable during flights or TLS scanning, it is highly recommended to run the RealSense camera at a lower resolution (e.g., 640x480) and capped at 15 FPS.

```bash
ros2 launch realsense2_camera rs_launch.py \
  align_depth.enable:=true \
  enable_sync:=true \
  depth_module.depth_profile:=640x480x15 \
  rgb_camera.color_profile:=640x480x15

```

### 4. Record the Dataset (In-Flight / Field)

Start recording the compressed visual data and the lightweight raw LiDAR packets. By targeting the `/compressed` and `/compressedDepth` topics, you save gigabytes of storage while preserving full data integrity. The `/unilidar/raw` topic contains **both LiDAR and IMU data** multiplexed in the original UDP stream.

```bash
ros2 bag record -o optimized_forest_scan \
  /camera/camera/color/image_raw/compressed \
  /camera/camera/aligned_depth_to_color/image_raw/compressedDepth \
  /camera/camera/color/camera_info \
  /camera/camera/aligned_depth_to_color/camera_info \
  /tf \
  /tf_static \
  /unilidar/raw

```

### 5. Post-Processing (Decoding)

Once you are back at your workstation, launch the unified decoder and play the `.bag` file. The decoder will automatically inflate the images and unpack the LiDAR/IMU packets.

```bash
# Terminal 1 — Start the Universal Decoder (Images + LiDAR)
ros2 launch unitree_lidar_ros2 decode_all.launch.py

# Terminal 2 — Replay the compressed recording
ros2 bag play optimized_forest_scan

# Terminal 3 — Visualize everything perfectly synced!
rviz2

```

**Decoded output topics ready for SLAM/RViz:**

| Topic | Type | Description |
| --- | --- | --- |
| `/unilidar/cloud` | `sensor_msgs/PointCloud2` | Decoded 3D point cloud |
| `/unilidar/imu` | `sensor_msgs/Imu` | Decoded IMU (accel + gyro) |
| `/camera/camera/color/image_raw` | `sensor_msgs/Image` | Decompressed RGB pure image |
| `/camera/camera/aligned_depth_to_color/image_raw` | `sensor_msgs/Image` | Decompressed aligned Depth image |

---

## 📂 Repository Structure

```plaintext
.
├── Dockerfile                  # Multi-stage build definition
├── .dockerignore               # Cache and bag exclusion rules
├── README.md    
├── docs                
└── src/
    └── unitree_lidar_ros2/     # The core ROS 2 package
        ├── launch/
        │   └── decode_all.launch.py # Unified decompression and LiDAR decoding
        ├── src/
        │   ├── unitree_lidar_ros2_node.cpp
        │   └── decoder_node.cpp
        ├── CMakeLists.txt
        └── package.xml
    └── unitree_lidar_sdk/      # Statically linked Unitree C++ dependencies

```

---

## 🤝 Contributing

Contributions are welcome! If you are working on robotic navigation, SLAM optimization, or UAV/TLS data acquisition, feel free to open an issue or submit a Pull Request.

## 📄 License

This project is open-source and available under the standard MIT License.