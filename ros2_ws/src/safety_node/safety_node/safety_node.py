#!/usr/bin/env python3
"""Safety node: monitors firmware status and publishes a clean safety interface."""

import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, String


class SafetyNode(Node):
    def __init__(self):
        super().__init__('safety_node')

        # --- parameters ---
        self.declare_parameter('fault_timeout_s', 2.0)
        self._fault_timeout = (
            self.get_parameter('fault_timeout_s').get_parameter_value().double_value
        )

        # --- internal state ---
        self._fault = False
        self._motors_en = False
        self._ros2_alive = False
        # start in "comms lost" state until the first real packet arrives
        self._last_status_time: float = time.monotonic() - 9999.0

        # --- subscriber ---
        self.create_subscription(
            String, '/sixeyes/firmware_status', self._firmware_status_cb, 10
        )

        # --- publishers ---
        self._pub_safety_status = self.create_publisher(
            String, '/sixeyes/safety_status', 10
        )
        self._pub_is_safe = self.create_publisher(Bool, '/sixeyes/is_safe', 10)

        # --- 1 Hz status timer ---
        self.create_timer(1.0, self._status_timer_cb)

        self.get_logger().info(
            f'safety_node started — fault_timeout_s={self._fault_timeout}'
        )

    # ------------------------------------------------------------------
    # firmware_status subscriber callback
    # ------------------------------------------------------------------

    def _firmware_status_cb(self, msg: String):
        """Parse "SB:<fault>,<motors_en>,<ros2_alive>" and update state."""
        line = msg.data.strip()
        if not line.startswith('SB:'):
            return

        try:
            parts = line[3:].split(',')
            new_fault = bool(int(parts[0]))
            new_motors_en = bool(int(parts[1]))
            new_ros2_alive = bool(int(parts[2]))
        except (IndexError, ValueError) as exc:
            self.get_logger().warning(f'Malformed firmware_status line "{line}": {exc}')
            return

        # fault transition logging
        if not self._fault and new_fault:
            self.get_logger().error('Firmware fault detected — motors disabled')
        elif self._fault and not new_fault:
            self.get_logger().info('Firmware fault cleared')

        self._fault = new_fault
        self._motors_en = new_motors_en
        self._ros2_alive = new_ros2_alive
        self._last_status_time = time.monotonic()

    # ------------------------------------------------------------------
    # 1 Hz timer
    # ------------------------------------------------------------------

    def _status_timer_cb(self):
        comms_ok = (time.monotonic() - self._last_status_time) < self._fault_timeout
        is_safe = comms_ok and not self._fault

        safe_msg = Bool()
        safe_msg.data = is_safe
        self._pub_is_safe.publish(safe_msg)

        status_msg = String()
        status_msg.data = (
            f'fault={self._fault} motors_en={self._motors_en} '
            f'ros2_alive={self._ros2_alive} comms_ok={comms_ok}'
        )
        self._pub_safety_status.publish(status_msg)


def main(args=None):
    rclpy.init(args=args)
    node = SafetyNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
