#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/header.hpp>
#include "unitree_lidar_ros2/msg/lidar_metadata.hpp"
#include "unitree_lidar_protocol.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>

class RawLidarCaptureNode : public rclcpp::Node {
public:
    RawLidarCaptureNode() : Node("unitree_lidar_ros2_node") {
        // Declaração de parâmetros do ROS 2 (Porta padrão do Lidar costuma ser 6101)
        this->declare_parameter<int>("udp_port", 6101);
        this->declare_parameter<std::string>("frame_id", "unilidar_lidar");

        int port = this->get_parameter("udp_port").as_int();
        frame_id_ = this->get_parameter("frame_id").as_string();

        // Configuração do Publisher com QoS adequado para sensores (Best Effort / Volátil)
        publisher_ = this->create_publisher<unitree_lidar_ros2::msg::LidarMetadata>(
            "unilidar/raw", rclcpp::SensorDataQoS());

        // Inicialização do Socket UDP do Linux
        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd_ < 0) {
            RCLCPP_FATAL(this->get_logger(), "Falha crítica: Não foi possível criar o socket UDP.");
            return;
        }

        struct sockaddr_in servaddr;
        std::memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = INADDR_ANY; // Escuta em qualquer interface de rede conectada
        servaddr.sin_port = htons(port);

        if (bind(sockfd_, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            RCLCPP_FATAL(this->get_logger(), "Falha crítica: Erro ao realizar o BIND na porta UDP %d.", port);
            close(sockfd_);
            return;
        }

        RCLCPP_INFO(this->get_logger(), "Captura UDP iniciada com sucesso na porta %d.", port);
        RCLCPP_INFO(this->get_logger(), "Aguardando pacotes brutos do Unitree LiDAR...");

        // Dispara uma thread separada para a captura síncrona (evita congelar o executor do ROS 2)
        is_running_ = true;
        capture_thread_ = std::thread(&RawLidarCaptureNode::udpCaptureLoop, this);
    }

    ~RawLidarCaptureNode() {
        is_running_ = false;
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
        RCLCPP_INFO(this->get_logger(), "Nó de captura encerrado.");
    }

private:
    void udpCaptureLoop() {
        // Buffer de 8KB (suficiente para cobrir o pacote 2D de ~5.5KB ou o 3D de 1.03KB)
        std::vector<uint8_t> buffer(8192);

        while (is_running_ && rclcpp::ok()) {
            struct sockaddr_in cliaddr;
            socklen_t len = sizeof(cliaddr);

            // Chamada blocante de leitura de rede
            ssize_t bytes_received = recvfrom(sockfd_, buffer.data(), buffer.size(), 0,
                                              (struct sockaddr *)&cliaddr, &len);

            if (bytes_received < 0) {
                if (is_running_) {
                    RCLCPP_WARN(this->get_logger(), "Erro de leitura no socket UDP.");
                }
                continue;
            }

            // O cabeçalho mínimo do protocolo Unitree possui 12 bytes
            if (bytes_received < 12) continue;

            // Mapeia os bytes iniciais para a estrutura FrameHeader definida em unitree_lidar_protocol.h
            const unilidar_sdk2::FrameHeader* header = 
                reinterpret_cast<const unilidar_sdk2::FrameHeader*>(buffer.data());

            // Validação de assinatura mágica da Unitree: 0x55 0xAA 0x05 0x0A
            if (header->header[0] == 0x55 && header->header[1] == 0xAA &&
                header->header[2] == 0x05 && header->header[3] == 0x0A) {

                // Cria a instância da sua mensagem customizada do ROS 2
                auto msg = unitree_lidar_ros2::msg::LidarMetadata();

                // CARIMBO DE TEMPO CRÍTICO: Registra o nanossegundo exato da chegada do pacote
                msg.header.stamp = this->now();
                msg.header.frame_id = frame_id_;

                // Aloca e copia os bytes brutos do buffer de rede para dentro do vetor da mensagem ROS 2
                msg.data.assign(buffer.begin(), buffer.begin() + bytes_received);

                // Publica a mensagem extremamente leve
                publisher_->publish(msg);
            }
        }
    }

    int sockfd_ = -1;
    bool is_running_ = false;
    std::string frame_id_;
    std::thread capture_thread_;
    rclcpp::Publisher<unitree_lidar_ros2::msg::LidarMetadata>::SharedPtr publisher_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<RawLidarCaptureNode>());
    rclcpp::shutdown();
    return 0;
}