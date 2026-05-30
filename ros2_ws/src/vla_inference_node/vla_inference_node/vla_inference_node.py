#!/usr/bin/env python3
"""VLA inference orchestration stub (runs on laptop only)."""

import rclpy
from rclpy.node import Node

class VLAInferenceNode(Node):
    def __init__(self):
        super().__init__('vla_inference_node')
        self.get_logger().info('vla_inference_node started (stub) - inference runs on laptop only')

def main(args=None):
    rclpy.init(args=args)
    node = VLAInferenceNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
