# =======================================================
# ESTÁGIO 1: BUILDER (A Fábrica que compila o código)
# =======================================================
# Usamos uma imagem oficial pública que qualquer um no mundo pode baixar
FROM osrf/ros:jazzy-desktop AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /ros2_ws

# Copiamos a sua estrutura limpa (o .dockerignore deixa o lixo de fora)
COPY . /ros2_ws/

# Compilamos o código C++ do seu pacote Unitree L2 aqui dentro
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && colcon build"


# =======================================================
# ESTÁGIO 2: RUNTIME (A Imagem Final Distribuível)
# =======================================================
FROM osrf/ros:jazzy-desktop

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /ros2_ws

# Instala a RealSense primeiro (aproveitando o cache do Docker)
RUN apt-get update && apt-get install -y \
    ros-jazzy-realsense2-camera \
    nano \
    psmisc \
    && rm -rf /var/lib/apt/lists/*

# A MÁGICA: Puxamos do estágio 1 apenas a pasta 'install' já compilada!
# O código fonte C++ original não vai para a imagem final, deixando ela minúscula.
COPY --from=builder /ros2_ws/install ./install

# Documenta as portas de comunicação do LiDAR
EXPOSE 6101/udp
EXPOSE 6201/udp

# Configura a inicialização automática do ambiente
RUN echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc && \
    echo "source /ros2_ws/install/setup.bash" >> ~/.bashrc

CMD ["/bin/bash"]