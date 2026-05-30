"""
SixEyes teleoperation mode bringup.

Starts 4 nodes required for live leader→follower mirroring:
  - usb_bridge_node  : owns the serial port; heartbeat TX, telemetry RX, command TX
  - safety_node      : monitors firmware status, publishes /sixeyes/is_safe
  - joint_state_node : /sixeyes/joint_states (deg) → /joint_states (rad)
  - camera_node      : USB camera → /camera/image_raw

Usage:
  ros2 launch sixeyes_bringup teleop.launch.py
  ros2 launch sixeyes_bringup teleop.launch.py port:=/dev/ttyACM1 baud:=115200
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    port_arg = DeclareLaunchArgument(
        'port',
        default_value='/dev/ttyACM0',
        description='Serial port for the follower ESP32 (USB-CDC)',
    )
    baud_arg = DeclareLaunchArgument(
        'baud',
        default_value='115200',
        description='Baud rate for the follower serial port',
    )
    heartbeat_hz_arg = DeclareLaunchArgument(
        'heartbeat_hz',
        default_value='50',
        description='Heartbeat TX rate in Hz (must be > 2 Hz for safety timeout)',
    )
    camera_index_arg = DeclareLaunchArgument(
        'camera_index',
        default_value='0',
        description='OpenCV camera device index',
    )

    port = LaunchConfiguration('port')
    baud = LaunchConfiguration('baud')
    heartbeat_hz = LaunchConfiguration('heartbeat_hz')
    camera_index = LaunchConfiguration('camera_index')

    usb_bridge = Node(
        package='usb_bridge_node',
        executable='usb_bridge_node',
        name='usb_bridge_node',
        parameters=[{
            'port': port,
            'baud': baud,
            'heartbeat_hz': heartbeat_hz,
        }],
        output='screen',
    )

    safety = Node(
        package='safety_node',
        executable='safety_node',
        name='safety_node',
        output='screen',
    )

    joint_state = Node(
        package='joint_state_node',
        executable='joint_state_node',
        name='joint_state_node',
        output='screen',
    )

    camera = Node(
        package='camera_node',
        executable='camera_node',
        name='camera_node',
        parameters=[{
            'camera_index': camera_index,
        }],
        output='screen',
    )

    return LaunchDescription([
        port_arg,
        baud_arg,
        heartbeat_hz_arg,
        camera_index_arg,
        usb_bridge,
        safety,
        joint_state,
        camera,
    ])
