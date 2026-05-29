cmake_minimum_required(VERSION 3.5)
project(unitree_lidar_ros2)

# ==========================================
# 1. COMPILER SETTINGS
# ==========================================
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ==========================================
# 2. FIND ROS 2 PACKAGES & DEPENDENCIES
# ==========================================
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(PCL REQUIRED)
find_package(rosidl_default_generators REQUIRED)

# ==========================================
# 3. DIRECTORIES & LIBRARIES (CORRIGIDO)
# ==========================================
include_directories(
  include
  ${PCL_INCLUDE_DIRS} 
  ${CMAKE_CURRENT_SOURCE_DIR}/../unitree_lidar_sdk/include
)

link_directories(
  ${PCL_LIBRARY_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/../unitree_lidar_sdk/lib/${CMAKE_SYSTEM_PROCESSOR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../unitree_lidar_sdk/lib
)
add_definitions(${PCL_DEFINITIONS})

# ==========================================
# 4. CUSTOM MESSAGES GENERATION
# ==========================================
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/LidarMetadata.msg"
  DEPENDENCIES std_msgs
)

# ==========================================
# 5. TARGET 1: NODE DE GRAVAÇÃO BRUTA
# ==========================================
add_executable(unitree_lidar_ros2_node src/unitree_lidar_ros2_node.cpp)

ament_target_dependencies(unitree_lidar_ros2_node
  rclcpp 
  std_msgs 
  sensor_msgs
  geometry_msgs 
  tf2_ros
  pcl_conversions
)

target_link_libraries(unitree_lidar_ros2_node
  ${PCL_LIBRARIES}
  libunilidar_sdk2.a
)

# Integração limpa das mensagens no ROS 2 Jazzy
rosidl_get_typesupport_target(cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_cpp")
target_link_libraries(unitree_lidar_ros2_node "${cpp_typesupport_target}")

# ==========================================
# 6. TARGET 2: NODE DECODIFICADOR OFFLINE
# ==========================================
add_executable(decoder_node src/decoder_node.cpp)

ament_target_dependencies(decoder_node
  rclcpp
  sensor_msgs
)

target_link_libraries(decoder_node
  libunilidar_sdk2.a
  "${cpp_typesupport_target}"
)

# ==========================================
# 7. INSTALL RULES
# ==========================================
install(TARGETS
  unitree_lidar_ros2_node
  decoder_node
  DESTINATION lib/${PROJECT_NAME}
)

install(FILES 
  rviz/view.rviz 
  launch/launch.py
  DESTINATION share/${PROJECT_NAME}
)

# Instala a pasta de launch para o ROS 2 enxergar
install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()