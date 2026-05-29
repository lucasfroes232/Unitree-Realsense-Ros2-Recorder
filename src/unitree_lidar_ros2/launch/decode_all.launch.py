from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # 1. Decodificador RGB (Transforma JPEG em imagem pura)
        Node(
            package='image_transport',
            executable='republish',
            name='republish_color',
            parameters=[{
                'in_transport': 'compressed',
                'out_transport': 'raw'
            }],
            remappings=[
                ('in/compressed', '/camera/camera/color/image_raw/compressed'),
                ('out', '/camera/camera/color/image_raw')
            ]
        ),
        
        # 2. Decodificador Depth (Transforma PNG/16-bit em profundidade pura)
        Node(
            package='image_transport',
            executable='republish',
            name='republish_depth',
            parameters=[{
                'in_transport': 'compressedDepth',
                'out_transport': 'raw'
            }],
            remappings=[
                ('in/compressedDepth', '/camera/camera/aligned_depth_to_color/image_raw/compressedDepth'),
                ('out', '/camera/camera/aligned_depth_to_color/image_raw')
            ]
        ),

        # 3. Decodificador LiDAR
         Node(
             package='unitree_lidar_ros2',
             executable='decoder_node',
             name='unitree_lidar_decoder'
         )
    ])