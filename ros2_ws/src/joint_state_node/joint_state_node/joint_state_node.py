#!/usr/bin/env python3
"""Joint state aggregator: re-publishes /sixeyes/joint_states as /joint_states."""

import math

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from builtin_interfaces.msg import Time  # noqa: F401 – available for type hints


class JointStateNode(Node):
    def __init__(self):
        super().__init__('joint_state_node')

        self.declare_parameter(
            'joint_names',
            ['base', 'shoulder', 'elbow', 'wrist_pitch', 'wrist_yaw', 'gripper'])

        self._pub = self.create_publisher(JointState, '/joint_states', 10)

        self.create_subscription(
            JointState,
            '/sixeyes/joint_states',
            self._joint_state_cb,
            10)

        self._msg_count = 0
        self._hz_count = 0
        self.create_timer(1.0, self._alive_cb)

        self.get_logger().info('joint_state_node started')

    def _joint_state_cb(self, msg: JointState):
        joint_names = self.get_parameter('joint_names').value

        out = JointState()
        out.header.stamp = self.get_clock().now().to_msg()
        out.header.frame_id = 'base_link'

        # Validate / replace joint names
        if not msg.name or len(msg.name) != len(joint_names):
            out.name = list(joint_names)
        else:
            out.name = list(msg.name)

        # Convert positions from degrees to radians
        out.position = [p * math.pi / 180.0 for p in msg.position]

        # Pass through velocity and effort unchanged
        out.velocity = list(msg.velocity)
        out.effort = list(msg.effort)

        self._pub.publish(out)
        self._hz_count += 1

    def _alive_cb(self):
        self.get_logger().info(
            f'joint_state_node: publishing at {self._hz_count} Hz')
        self._hz_count = 0


def main(args=None):
    rclpy.init(args=args)
    node = JointStateNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
