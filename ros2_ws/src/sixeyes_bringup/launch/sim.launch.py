"""
SixEyes Gazebo virtual arm bringup.

Starts Gazebo Harmonic with an empty world, spawns the placeholder SixEyes
arm (sixeyes_description), and loads ros2_control controllers so it can be
commanded over the standard joint_trajectory_controller action interface.

No physical firmware bridge here -- this is the Gazebo-only track from
docs/V1_TODO.md. usb_bridge_node is not started by this launch file.

Usage:
  ros2 launch sixeyes_bringup sim.launch.py

Then, from another shell, command the arm, e.g.:
  ros2 action send_goal /arm_controller/follow_joint_trajectory \\
    control_msgs/action/FollowJointTrajectory "{trajectory: {joint_names: \\
    [waist_joint, shoulder_joint, elbow_joint, wrist_pitch_joint, \\
    wrist_yaw_joint, gripper_joint], points: [{positions: \\
    [0.5, 0.3, -0.4, 1.2, 0.5, 0.3], time_from_start: {sec: 2}}]}}"
"""

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    pkg_description = get_package_share_directory('sixeyes_description')
    xacro_path = os.path.join(pkg_description, 'urdf', 'sixeyes.urdf.xacro')

    try:
        pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')
        gz_sim_launch = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')
            ),
            launch_arguments={'gz_args': 'empty.sdf -r'}.items(),
        )
    except Exception:
        # ros_gz_sim not installed -- fail loudly with a clear message rather
        # than a cryptic downstream error, since this is easy to miss when
        # following docs/ros2/RPI5_SETUP_GUIDE.md's install steps.
        raise RuntimeError(
            "ros_gz_sim not found. Install it with: "
            "sudo apt install -y ros-jazzy-ros-gz ros-jazzy-ros2-control "
            "ros-jazzy-gz-ros2-control ros-jazzy-joint-trajectory-controller "
            "ros-jazzy-joint-state-broadcaster"
        )

    use_sim_time = LaunchConfiguration('use_sim_time')
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time', default_value='true',
        description='Use Gazebo simulation clock instead of wall clock',
    )

    robot_description = ParameterValue(
        Command(['xacro ', xacro_path]), value_type=str
    )

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': robot_description,
            'use_sim_time': use_sim_time,
        }],
    )

    # Spawns the robot into the already-running Gazebo world, reading URDF
    # from the /robot_description topic robot_state_publisher just published.
    spawn_robot = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=['-topic', 'robot_description', '-name', 'sixeyes', '-z', '0.05'],
        output='screen',
    )

    # Bridges Gazebo's simulation clock onto /clock so use_sim_time works for
    # every ROS2 node in this launch, not just the ones Gazebo starts itself.
    clock_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=['/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock'],
        output='screen',
    )

    joint_state_broadcaster_spawner = Node(
        package='controller_manager',
        executable='spawner',
        arguments=['joint_state_broadcaster'],
        output='screen',
    )

    arm_controller_spawner = Node(
        package='controller_manager',
        executable='spawner',
        arguments=['arm_controller'],
        output='screen',
    )

    return LaunchDescription([
        use_sim_time_arg,
        gz_sim_launch,
        clock_bridge,
        robot_state_publisher,
        spawn_robot,
        joint_state_broadcaster_spawner,
        arm_controller_spawner,
    ])
