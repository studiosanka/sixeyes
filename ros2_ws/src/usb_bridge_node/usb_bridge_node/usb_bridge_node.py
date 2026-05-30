#!/usr/bin/env python3
"""USB-CDC bridge: owns the follower serial port and bridges it to ROS2 topics."""

import json
import threading
import time

import rclpy
from rclpy.node import Node
from builtin_interfaces.msg import Time
from sensor_msgs.msg import JointState
from std_msgs.msg import String

try:
    import serial
    import serial.serialutil
except ImportError:
    serial = None

_JOINT_NAMES = ['base', 'shoulder', 'elbow', 'wrist_pitch', 'wrist_yaw', 'gripper']
_LOG_PREFIXES = ('[INFO]', '[WARN]', '[ERROR]', '[DBG]', '[CAL]')


class UsbBridgeNode(Node):
    def __init__(self):
        super().__init__('usb_bridge_node')

        # --- parameters ---
        self.declare_parameter('port', '/dev/ttyACM0')
        self.declare_parameter('baud', 115200)
        self.declare_parameter('hb_hz', 50.0)

        self._port = self.get_parameter('port').get_parameter_value().string_value
        self._baud = self.get_parameter('baud').get_parameter_value().integer_value
        hb_hz = self.get_parameter('hb_hz').get_parameter_value().double_value

        # --- publishers ---
        self._pub_fw_status = self.create_publisher(String, '/sixeyes/firmware_status', 10)
        self._pub_joint_states = self.create_publisher(JointState, '/sixeyes/joint_states', 10)
        self._pub_telemetry_raw = self.create_publisher(String, '/sixeyes/telemetry_raw', 10)

        # --- subscriber ---
        self.create_subscription(String, '/sixeyes/json_commands',
                                 self._json_commands_cb, 10)

        # --- serial state ---
        self._serial = None
        self._serial_lock = threading.Lock()
        self._hb_seq = 0
        self._running = True

        # attempt initial open (non-fatal)
        self._try_open_serial()

        # --- heartbeat timer ---
        self.create_timer(1.0 / hb_hz, self._heartbeat_cb)

        # --- background reader thread ---
        self._reader_thread = threading.Thread(
            target=self._reader_loop, daemon=True, name='usb_bridge_reader'
        )
        self._reader_thread.start()

        self.get_logger().info(
            f'usb_bridge_node started — port={self._port} baud={self._baud} hb_hz={hb_hz}'
        )

    # ------------------------------------------------------------------
    # serial helpers
    # ------------------------------------------------------------------

    def _try_open_serial(self):
        if serial is None:
            self.get_logger().error('pyserial is not installed; cannot open serial port')
            return False
        try:
            ser = serial.Serial(self._port, self._baud, timeout=1.0)
            with self._serial_lock:
                self._serial = ser
            self.get_logger().info(f'Opened serial port {self._port}')
            return True
        except serial.serialutil.SerialException as exc:
            self.get_logger().error(f'Failed to open {self._port}: {exc}')
            return False

    # ------------------------------------------------------------------
    # heartbeat timer
    # ------------------------------------------------------------------

    def _heartbeat_cb(self):
        line = f'HB:0,{self._hb_seq}\n'
        self._hb_seq += 1
        self._write_serial(line.encode())

    def _write_serial(self, data: bytes):
        with self._serial_lock:
            if self._serial is None or not self._serial.is_open:
                return
            try:
                self._serial.write(data)
            except serial.serialutil.SerialException as exc:
                self.get_logger().warning(f'Serial write error: {exc}')
                self._close_serial_locked()

    def _close_serial_locked(self):
        """Close serial; caller must already hold _serial_lock."""
        if self._serial is not None:
            try:
                self._serial.close()
            except Exception:
                pass
            self._serial = None

    # ------------------------------------------------------------------
    # background reader
    # ------------------------------------------------------------------

    def _reader_loop(self):
        while self._running:
            with self._serial_lock:
                ser = self._serial

            if ser is None or not ser.is_open:
                # reconnect loop
                if not self._try_open_serial():
                    time.sleep(2.0)
                continue

            try:
                raw = ser.readline()  # blocks up to timeout=1s
            except serial.serialutil.SerialException as exc:
                self.get_logger().warning(f'Serial read error: {exc} — reconnecting…')
                with self._serial_lock:
                    self._close_serial_locked()
                continue

            if not raw:
                continue

            try:
                line = raw.decode('utf-8', errors='replace').rstrip('\r\n')
            except Exception:
                continue

            self._handle_line(line)

    def _handle_line(self, line: str):
        if line.startswith('SB:'):
            msg = String()
            msg.data = line
            self._pub_fw_status.publish(msg)
            return

        if line.startswith('{') and 'TELEMETRY_STATE' in line:
            msg_raw = String()
            msg_raw.data = line
            self._pub_telemetry_raw.publish(msg_raw)
            self._publish_joint_state(line)
            return

        for prefix in _LOG_PREFIXES:
            if line.startswith(prefix):
                self.get_logger().info(line[len(prefix):].lstrip())
                return

    def _publish_joint_state(self, line: str):
        try:
            data = json.loads(line)
        except json.JSONDecodeError as exc:
            self.get_logger().warning(f'JSON parse error in TELEMETRY_STATE: {exc}')
            return

        js = JointState()

        ts_ms = data.get('ts', 0)
        ts_sec = int(ts_ms // 1000)
        ts_nsec = int((ts_ms % 1000) * 1_000_000)
        ros_time = Time()
        ros_time.sec = ts_sec
        ros_time.nanosec = ts_nsec
        js.header.stamp = ros_time

        joints = data.get('follower_joints', [])
        js.name = _JOINT_NAMES[:len(joints)]
        js.position = [float(j) for j in joints]

        self._pub_joint_states.publish(js)

    # ------------------------------------------------------------------
    # json_commands subscriber
    # ------------------------------------------------------------------

    def _json_commands_cb(self, msg: String):
        payload = (msg.data + '\n').encode('utf-8')
        self._write_serial(payload)

    # ------------------------------------------------------------------
    # shutdown
    # ------------------------------------------------------------------

    def destroy_node(self):
        self._running = False
        self._reader_thread.join(timeout=2.0)
        with self._serial_lock:
            self._close_serial_locked()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = UsbBridgeNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
