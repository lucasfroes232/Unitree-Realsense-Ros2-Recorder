#include "rclcpp/rclcpp.hpp"
#include "unitree_lidar_ros2/msg/lidar_metadata.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include <sensor_msgs/point_cloud2_iterator.hpp>

#include "unitree_lidar_protocol.h"
#include "unitree_lidar_sdk.h"

using std::placeholders::_1;

class OfflineDecoderNode : public rclcpp::Node
{
public:
    OfflineDecoderNode() : Node("offline_decoder_node")
    {
        this->declare_parameter<double>("sweep_duration", 0.1);
        sweep_duration_ = this->get_parameter("sweep_duration").as_double();

        rclcpp::QoS qos_profile(1000);
        qos_profile.best_effort();

        subscription_ = this->create_subscription<unitree_lidar_ros2::msg::LidarMetadata>(
            "/unilidar/raw", qos_profile,
            std::bind(&OfflineDecoderNode::raw_callback, this, _1));

        // Publisher de nuvem de pontos
        publisher_ = this->create_publisher<sensor_msgs::msg::PointCloud2>("/unilidar/cloud", 10);

        // Publisher de IMU — tópico padrão esperado pelo LIO-SAM e FAST-LIO
        imu_publisher_ = this->create_publisher<sensor_msgs::msg::Imu>("/unilidar/imu", 10);

        RCLCPP_INFO(this->get_logger(),
            "Decodificador iniciado | janela de acumulação: %.3f s", sweep_duration_);
    }

private:
    std::vector<unilidar_sdk2::PointUnitree> sweep_points_;
    double sweep_stamp_ = -1.0;
    double sweep_duration_;

    void raw_callback(const unitree_lidar_ros2::msg::LidarMetadata::SharedPtr msg)
    {
        if (msg->data.size() == sizeof(unilidar_sdk2::LidarPointDataPacket)) {
            decode_pointcloud(msg);
        } else if (msg->data.size() == sizeof(unilidar_sdk2::LidarImuDataPacket)) {
            decode_imu(msg);
        } else {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                "Pacote ignorado: tamanho %zu", msg->data.size());
        }
    }

    void decode_pointcloud(const unitree_lidar_ros2::msg::LidarMetadata::SharedPtr msg)
    {
        unilidar_sdk2::LidarPointDataPacket packet;
        memcpy(&packet, msg->data.data(), sizeof(unilidar_sdk2::LidarPointDataPacket));

        unilidar_sdk2::PointCloudUnitree cloud_slice;
        unilidar_sdk2::parseFromPacketToPointCloud(
            cloud_slice,
            packet,
            true,   // use_system_timestamp
            0.0f,   // range_min
            100.0f  // range_max
        );

        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
            "Bruto: %u | Válidos: %zu | Descartados: %u | Acumulados: %zu",
            packet.data.point_num,
            cloud_slice.points.size(),
            packet.data.point_num - (uint32_t)cloud_slice.points.size(),
            sweep_points_.size());

        if (cloud_slice.points.empty()) return;

        if (sweep_stamp_ < 0) {
            sweep_stamp_ = cloud_slice.stamp;
        }

        if (cloud_slice.stamp - sweep_stamp_ >= sweep_duration_) {
            publish_accumulated_cloud();
            sweep_points_.clear();
            sweep_stamp_ = cloud_slice.stamp;
        }

        float time_offset = static_cast<float>(cloud_slice.stamp - sweep_stamp_);

        for (auto pt : cloud_slice.points) {
            pt.time += time_offset;
            sweep_points_.push_back(pt);
        }
    }

    void decode_imu(const unitree_lidar_ros2::msg::LidarMetadata::SharedPtr msg)
    {
        // Copia os bytes brutos para a struct do protocolo Unitree
        unilidar_sdk2::LidarImuDataPacket imu_packet;
        memcpy(&imu_packet, msg->data.data(), sizeof(unilidar_sdk2::LidarImuDataPacket));

        // Monta a mensagem padrão ROS 2 de IMU
        sensor_msgs::msg::Imu imu_msg;

        // Timestamp — usa o relógio do sistema (mesmo critério da nuvem)
        imu_msg.header.stamp = this->now();
        imu_msg.header.frame_id = "unilidar_imu";

        // Quaternion (orientação): x, y, z, w
        imu_msg.orientation.w = imu_packet.data.quaternion[0];
        imu_msg.orientation.x = imu_packet.data.quaternion[1];
        imu_msg.orientation.y = imu_packet.data.quaternion[2];
        imu_msg.orientation.z = imu_packet.data.quaternion[3];

        // Velocidade angular (giroscópio) em rad/s
        imu_msg.angular_velocity.x = imu_packet.data.angular_velocity[0];
        imu_msg.angular_velocity.y = imu_packet.data.angular_velocity[1];
        imu_msg.angular_velocity.z = imu_packet.data.angular_velocity[2];

        // Aceleração linear em m/s²
        imu_msg.linear_acceleration.x = imu_packet.data.linear_acceleration[0];
        imu_msg.linear_acceleration.y = imu_packet.data.linear_acceleration[1];
        imu_msg.linear_acceleration.z = imu_packet.data.linear_acceleration[2];

        // Covariâncias desconhecidas — -1 indica "não disponível"
        imu_msg.orientation_covariance[0] = -1;
        imu_msg.angular_velocity_covariance[0] = -1;
        imu_msg.linear_acceleration_covariance[0] = -1;

        imu_publisher_->publish(imu_msg);
    }

    void publish_accumulated_cloud()
    {
        if (sweep_points_.empty()) return;

        sensor_msgs::msg::PointCloud2 ros_cloud;
        ros_cloud.header.frame_id = "unilidar";
        ros_cloud.header.stamp.sec = (int32_t)sweep_stamp_;
        ros_cloud.header.stamp.nanosec =
            (uint32_t)((sweep_stamp_ - (int32_t)sweep_stamp_) * 1e9);

        ros_cloud.height = 1;
        ros_cloud.width = sweep_points_.size();
        ros_cloud.is_dense = true;

        sensor_msgs::PointCloud2Modifier modifier(ros_cloud);
        modifier.setPointCloud2Fields(6,
            "x",         1, sensor_msgs::msg::PointField::FLOAT32,
            "y",         1, sensor_msgs::msg::PointField::FLOAT32,
            "z",         1, sensor_msgs::msg::PointField::FLOAT32,
            "intensity", 1, sensor_msgs::msg::PointField::FLOAT32,
            "time",      1, sensor_msgs::msg::PointField::FLOAT32,
            "ring",      1, sensor_msgs::msg::PointField::UINT32);

        sensor_msgs::PointCloud2Iterator<float>    iter_x(ros_cloud, "x");
        sensor_msgs::PointCloud2Iterator<float>    iter_y(ros_cloud, "y");
        sensor_msgs::PointCloud2Iterator<float>    iter_z(ros_cloud, "z");
        sensor_msgs::PointCloud2Iterator<float>    iter_i(ros_cloud, "intensity");
        sensor_msgs::PointCloud2Iterator<float>    iter_t(ros_cloud, "time");
        sensor_msgs::PointCloud2Iterator<uint32_t> iter_r(ros_cloud, "ring");

        for (const auto& pt : sweep_points_) {
            *iter_x = pt.x;
            *iter_y = pt.y;
            *iter_z = pt.z;
            *iter_i = pt.intensity;
            *iter_t = pt.time;
            *iter_r = pt.ring;
            ++iter_x; ++iter_y; ++iter_z; ++iter_i; ++iter_t; ++iter_r;
        }

        RCLCPP_INFO(this->get_logger(),
            "Nuvem publicada: %zu pontos | stamp: %.3f",
            sweep_points_.size(), sweep_stamp_);

        publisher_->publish(ros_cloud);
    }

    rclcpp::Subscription<unitree_lidar_ros2::msg::LidarMetadata>::SharedPtr subscription_;
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_publisher_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<OfflineDecoderNode>());
    rclcpp::shutdown();
    return 0;
}