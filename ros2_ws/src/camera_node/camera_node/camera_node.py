#!/usr/bin/env python3
"""Camera node publishing /camera/image_raw using OpenCV (if available)."""

import time

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image, CameraInfo
from std_msgs.msg import Header
from builtin_interfaces.msg import Time


class CameraNode(Node):
    def __init__(self):
        super().__init__('camera_node')

        self.declare_parameter('camera_index', 0)
        self.declare_parameter('fps', 15.0)
        self.declare_parameter('width', 640)
        self.declare_parameter('height', 480)

        camera_index = self.get_parameter('camera_index').value
        fps = self.get_parameter('fps').value
        width = self.get_parameter('width').value
        height = self.get_parameter('height').value

        self._pub_image = self.create_publisher(Image, '/camera/image_raw', 10)
        self._pub_info = self.create_publisher(CameraInfo, '/camera/camera_info', 10)

        self._cap = None
        self._opencv_available = True
        self._no_frame_warned_at = 0.0

        try:
            import cv2
            import numpy  # noqa: F401 – imported here to ensure it's available
            self._cv2 = cv2

            cap = cv2.VideoCapture(camera_index)
            cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
            cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)

            if not cap.isOpened():
                self.get_logger().error(
                    f'CameraNode: could not open camera index {camera_index}')
                self._cap = None
            else:
                self._cap = cap
        except ImportError:
            self.get_logger().error('CameraNode: cv2 (OpenCV) not available; no frames will be published')
            self._opencv_available = False

        period = 1.0 / fps if fps > 0.0 else 1.0 / 15.0
        self.create_timer(period, self._timer_cb)

        self.get_logger().info(
            f'camera_node started (index={camera_index}, fps={fps}, '
            f'{width}x{height})')

    def _timer_cb(self):
        if not self._opencv_available:
            self.get_logger().error('CameraNode: OpenCV not available', once=True)
            return

        if self._cap is None:
            return

        ret, frame = self._cap.read()
        if not ret or frame is None:
            now = time.monotonic()
            if now - self._no_frame_warned_at >= 1.0:
                self.get_logger().warn('CameraNode: failed to grab frame from camera')
                self._no_frame_warned_at = now
            return

        frame_rgb = self._cv2.cvtColor(frame, self._cv2.COLOR_BGR2RGB)
        h, w = frame_rgb.shape[:2]

        stamp = self.get_clock().now().to_msg()

        img_msg = Image()
        img_msg.header.stamp = stamp
        img_msg.header.frame_id = 'camera_link'
        img_msg.height = h
        img_msg.width = w
        img_msg.encoding = 'rgb8'
        img_msg.is_bigendian = 0
        img_msg.step = w * 3
        img_msg.data = frame_rgb.tobytes()
        self._pub_image.publish(img_msg)

        info_msg = CameraInfo()
        info_msg.header.stamp = stamp
        info_msg.header.frame_id = 'camera_link'
        info_msg.height = h
        info_msg.width = w
        # K, D, R, P left as zeros/identity — calibration comes later
        info_msg.distortion_model = 'plumb_bob'
        self._pub_info.publish(info_msg)

    def destroy_node(self):
        if self._cap is not None and self._opencv_available:
            self._cap.release()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = CameraNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
