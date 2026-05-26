# Lightweight Unitree L2 & RealSense ROS 2 Recorder 🚀

![ROS 2](https://img.shields.io/badge/ROS_2-Jazzy-342B5A?logo=ros)
![Docker](https://img.shields.io/badge/Docker-Multi--stage-2496ED?logo=docker)
![C++](https://img.shields.io/badge/C++-17-00599C?logo=c%2B%2B)

A zero-overhead, multi-stage Dockerized ROS 2 (Jazzy) workspace designed for high-performance raw data acquisition from **Unitree L2 LiDARs** and **Intel RealSense** cameras.

Built specifically for resource-constrained environments — such as UAV onboard computers — this architecture ensures ultra-low latency recording, making it ideal for downstream SLAM (FAST-LIVO, LIO-SAM) and dense biomass estimation pipelines.

---

## 🧠 Architecture Highlights

Recording dense `PointCloud2` messages directly to a `.bag` file can cause severe CPU bottlenecks and network latency on embedded systems. This package solves this by splitting the workflow into two distinct phases:

1. **Raw Capture Node (`unitree_lidar_ros2_node`):** A highly optimized C++ node that intercepts pure UDP packets (port 6201) and publishes them instantly as custom `LidarMetadata` messages. This keeps CPU usage near 0% during in-flight recording.
2. **Offline Decoder Node (`decoder_node`):** A secondary node used post-flight. It translates the raw `.bag` packets into standard `sensor_msgs/PointCloud2` and aligns the IMU data using the statically linked Unitree SDK, preserving perfect timestamp synchronization for SLAM ingestion.
3. **Multi-stage Docker Build:** The final distributed image contains only the compiled binaries and RealSense drivers. It leaves all C++ source code and build caches behind, resulting in a minimal footprint.

---

## ⚡ Quick Start (No compilation required)

You do not need to pollute your host machine with ROS 2, C++ compilers, or SDKs. Just pull the pre-built image:

docker pull your_dockerhub_user/unitree_realsense_jazzy:v1

*(Note: Replace `your_dockerhub_user` with your actual Docker Hub username).*

Run the container with hardware acceleration and host network privileges:

docker run -it --net=host \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  your_dockerhub_user/unitree_realsense_jazzy:v1

---

## 🛠️ Usage Guide

### 1. Wake up the LiDAR motor
Before capturing data, ensure the LiDAR is spinning by sending the initialization command to port `6101`. (You can run the standard Unitree UDP example for this).

### 2. Start the Raw Capture Node
Launch the lightweight node to start listening to the LiDAR's broadcast on port `6201`:

ros2 run unitree_lidar_ros2 unitree_lidar_ros2_node --ros-args -p udp_port:=6201


### 3. Record the Dataset (In-Flight / Field)
Launch your RealSense camera node, and start recording both the visual data and the lightweight raw LiDAR packets:

ros2 bag record /unilidar/raw /camera/camera/color/image_raw /camera/camera/imu


### 4. Post-Processing (Decoding)
Once you are back at your workstation, play the `.bag` file and run the decoder node to translate the raw packets into heavy 3D point clouds for RViz visualization or SLAM processing:

ros2 run unitree_lidar_ros2 decoder_node


---

## 📂 Repository Structure

.
├── Dockerfile                  # Multi-stage build definition
├── .dockerignore               # Cache and bag exclusion rules
├── README.md                   
└── src/
    ├── unitree_lidar_ros2/     # The core ROS 2 package (Capture & Decode)
    └── unitree_lidar_sdk/      # Statically linked Unitree C++ dependencies


## 🤝 Contributing
Contributions are welcome! If you are working on robotic navigation, SLAM optimization, or UAV data acquisition, feel free to open an issue or submit a Pull Request.

## 📄 License
This project is open-source and available under the standard MIT License.