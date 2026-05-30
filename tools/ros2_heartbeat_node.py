#!/usr/bin/env python3
"""
ROS2 Safety Node - Heartbeat Protocol Implementation
Communicates with SixEyes ESP32 firmware via USB-CDC

Protocol:
  FW → ROS2: "SB:<fault>,<motors_en>,<ros2_alive>\n"
  ROS2 → FW: "HB:0,<sequence>\n"

Usage:
  python ros2_heartbeat_node.py --port /dev/ttyUSB0 --baud 115200
"""

import argparse
import serial
import threading
import time
from dataclasses import dataclass
from typing import Optional, Callable


@dataclass
class FirmwareStatus:
    """Parsed firmware status packet"""
    fault: bool
    motors_enabled: bool
    ros2_alive: bool
    timestamp: float


class SixEyesHeartbeatBridge:
    """
    Bidirectional heartbeat bridge for SixEyes ESP32 safety system
    
    Features:
    - Sends heartbeat at configurable rate (default 50 Hz)
    - Parses status updates from firmware (10 Hz)
    - Detects firmware faults and triggers E-stop
    - Thread-safe for integration with ROS2 nodes
    """
    
    def __init__(self, port: str, baudrate: int = 115200, heartbeat_hz: int = 50):
        """
        Args:
            port: Serial port (e.g., '/dev/ttyUSB0', 'COM3')
            baudrate: Serial speed (default 115200)
            heartbeat_hz: Heartbeat send frequency (default 50 Hz)
        """
        self.port = port
        self.baudrate = baudrate
        self.heartbeat_hz = heartbeat_hz
        self.heartbeat_interval = 1.0 / heartbeat_hz
        
        self.serial = None
        self._serial_lock = threading.Lock()
        self.running = False
        self.sequence = 0
        
        # Status tracking
        self.last_status = None
        self.fault_detected = False
        self.packets_sent = 0
        self.packets_received = 0
        self.packets_lost = 0
        
        # Callbacks
        self.on_status_change: Optional[Callable[[FirmwareStatus], None]] = None
        self.on_fault: Optional[Callable[[str], None]] = None
        
        # Threads
        self.sender_thread = None
        self.receiver_thread = None
        
    def connect(self) -> bool:
        """Open serial connection"""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1.0,
                write_timeout=1.0
            )
            print(f"[INFO] Connected to {self.port} @ {self.baudrate} baud")
            return True
        except serial.SerialException as e:
            print(f"[ERROR] Failed to open {self.port}: {e}")
            return False
            
    def disconnect(self) -> None:
        """Close serial connection"""
        if self.serial and self.serial.is_open:
            self.serial.close()
            print(f"[INFO] Disconnected from {self.port}")
            
    def start(self) -> bool:
        """Start sender and receiver threads"""
        if not self.serial or not self.serial.is_open:
            print("[ERROR] Not connected. Call connect() first.")
            return False
            
        self.running = True
        
        # Start sender thread (heartbeat)
        self.sender_thread = threading.Thread(target=self._sender_loop, daemon=True)
        self.sender_thread.start()
        
        # Start receiver thread (status updates)
        self.receiver_thread = threading.Thread(target=self._receiver_loop, daemon=True)
        self.receiver_thread.start()
        
        print(f"[INFO] Heartbeat bridge started ({self.heartbeat_hz} Hz)")
        return True
        
    def stop(self) -> None:
        """Stop threads and disconnect"""
        self.running = False
        
        # Wait for threads to finish
        if self.sender_thread:
            self.sender_thread.join(timeout=2.0)
        if self.receiver_thread:
            self.receiver_thread.join(timeout=2.0)
            
        self.disconnect()
        
    def _sender_loop(self) -> None:
        """Send heartbeat packets at configured rate"""
        while self.running:
            try:
                # Format: "HB:0,<sequence>\n"
                msg = f"HB:0,{self.sequence}\n"
                with self._serial_lock:
                    self.serial.write(msg.encode())
                self.packets_sent += 1
                self.sequence += 1

                time.sleep(self.heartbeat_interval)

            except serial.SerialException as e:
                print(f"[ERROR] Send failed: {e}")
                self.serial = None
                print(f"[INFO] Attempting to reconnect to {self.port}...")
                while self.running:
                    time.sleep(2.0)
                    if self.serial is not None:
                        break
                    if self.connect():
                        print(f"[INFO] Reconnected to {self.port}")
                        break
                
    def _receiver_loop(self) -> None:
        """Receive and parse status packets from firmware"""
        while self.running:
            try:
                line = None
                with self._serial_lock:
                    if self.serial is not None and self.serial.in_waiting > 0:
                        line = self.serial.readline().decode('utf-8', errors='ignore').strip()

                if line:
                    if line.startswith("SB:"):
                        self._parse_status(line)
                    else:
                        # Other output (e.g., debug messages)
                        print(f"[FW] {line}")

            except serial.SerialException as e:
                print(f"[ERROR] Receive failed: {e}")
                self.serial = None
                print(f"[INFO] Attempting to reconnect to {self.port}...")
                while self.running:
                    time.sleep(2.0)
                    if self.serial is not None:
                        break
                    if self.connect():
                        print(f"[INFO] Reconnected to {self.port}")
                        break
            except Exception as e:
                print(f"[ERROR] Receive failed: {e}")

            time.sleep(0.01)  # 100 ms polling interval
            
    def _parse_status(self, line: str) -> None:
        """Parse firmware status packet: 'SB:<fault>,<motors_en>,<ros2_alive>'"""
        try:
            # Format: "SB:0,1,1"
            parts = line[3:].split(',')
            if len(parts) != 3:
                return
                
            fault = bool(int(parts[0]))
            motors_en = bool(int(parts[1]))
            ros2_alive = bool(int(parts[2]))
            
            status = FirmwareStatus(
                fault=fault,
                motors_enabled=motors_en,
                ros2_alive=ros2_alive,
                timestamp=time.time()
            )
            
            self.packets_received += 1
            self.last_status = status
            
            # Detect fault edge (transition from OK to fault)
            if fault and not self.fault_detected:
                self.fault_detected = True
                msg = f"Firmware fault detected! Motors: {motors_en}, ROS2: {ros2_alive}"
                print(f"[ALERT] {msg}")
                if self.on_fault:
                    self.on_fault(msg)
                    
            # Detect fault recovery (transition from fault to OK)
            elif not fault and self.fault_detected:
                self.fault_detected = False
                print("[INFO] Fault cleared by firmware")
                
            # Notify callback
            if self.on_status_change:
                self.on_status_change(status)
                
        except (ValueError, IndexError):
            print(f"[WARN] Invalid status packet: {line}")
            
    def get_status(self) -> Optional[FirmwareStatus]:
        """Get last received status (or None if no packets yet)"""
        return self.last_status
        
    def is_firmware_healthy(self) -> bool:
        """Returns True if firmware is OK (no fault and ROS2 connected)"""
        if not self.last_status:
            return False
        return not self.last_status.fault and self.last_status.ros2_alive
        
    def print_stats(self) -> None:
        """Print diagnostic statistics"""
        print("\n[STATS] SixEyes Heartbeat Bridge")
        print(f"  Packets sent:     {self.packets_sent}")
        print(f"  Packets received: {self.packets_received}")
        print(f"  Sequence:         {self.sequence}")
        
        if self.last_status:
            print(f"  Last Status:")
            print(f"    Fault:          {self.last_status.fault}")
            print(f"    Motors enabled: {self.last_status.motors_enabled}")
            print(f"    ROS2 alive:     {self.last_status.ros2_alive}")
            print(f"    Age:            {time.time() - self.last_status.timestamp:.1f}s")
        else:
            print("  Last Status:      (None received yet)")
            
        print()


def main():
    """CLI example"""
    parser = argparse.ArgumentParser(
        description="SixEyes ROS2 Heartbeat Bridge"
    )
    parser.add_argument(
        '--port',
        default='/dev/ttyUSB0',
        help='Serial port (default: /dev/ttyUSB0)'
    )
    parser.add_argument(
        '--baud',
        type=int,
        default=115200,
        help='Baud rate (default: 115200)'
    )
    parser.add_argument(
        '--hz',
        type=int,
        default=50,
        help='Heartbeat frequency in Hz (default: 50)'
    )
    
    args = parser.parse_args()
    
    # Create bridge
    bridge = SixEyesHeartbeatBridge(
        port=args.port,
        baudrate=args.baud,
        heartbeat_hz=args.hz
    )
    
    # Setup callbacks
    def on_status_change(status: FirmwareStatus):
        # Can integrate with ROS2 here
        pass
        
    def on_fault(msg: str):
        # Can trigger ROS2 emergency_stop topic
        print(f"[ROS2] Publishing emergency_stop (reason: {msg})")
        
    bridge.on_status_change = on_status_change
    bridge.on_fault = on_fault
    
    # Connect and start
    if not bridge.connect():
        return 1
        
    if not bridge.start():
        return 1
        
    try:
        # Run indefinitely
        print("\n[INFO] Heartbeat running. Press Ctrl+C to stop.\n")
        
        last_stat_time = time.time()
        while True:
            time.sleep(1.0)
            
            # Print stats every 5 seconds
            if time.time() - last_stat_time > 5.0:
                bridge.print_stats()
                last_stat_time = time.time()
                
    except KeyboardInterrupt:
        print("\n[INFO] Stopping...")
        bridge.stop()
        print("[INFO] Done")
        return 0


if __name__ == "__main__":
    exit(main())
