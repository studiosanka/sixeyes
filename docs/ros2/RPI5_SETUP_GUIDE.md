# Raspberry Pi 5 — ROS2 Setup Guide

**Status**: Current. Target: Gazebo-simulation-only for now — no physical firmware bridge yet (v1 hardware/firmware isn't ready; see `docs/V1_TODO.md`).
**Audience**: written for someone with no prior ROS2 experience.
**Stack**: Ubuntu Server 24.04 LTS (arm64) + ROS2 Jazzy + Gazebo Harmonic.

---

## 1. What ROS2 actually is (read this before installing anything)

You don't need a deep understanding to get started, just these five ideas:

- **Node**: one running program that does one job (e.g. "publish camera images," "compute a safety check"). A robot is normally many small nodes talking to each other, not one big program.
- **Topic**: a named, typed message stream nodes publish to or subscribe from — e.g. `/joint_states` carries the arm's current joint angles. Nodes don't call each other directly; they publish/subscribe to topics.
- **Message**: the data structure sent over a topic (a fixed schema, like a struct).
- **Workspace**: a folder of ROS2 packages you build together with a tool called `colcon`. This repo's workspace is `ros2_ws/`.
- **Launch file**: a Python script that starts a group of nodes together with the right settings, instead of starting each one by hand.

That's enough to follow the rest of this guide. `ros2 topic list`, `ros2 node list`, and `ros2 topic echo <name>` (introduced in §5) are the main commands you'll use to see what's actually happening.

---

## 2. Flash the RPi5

1. Download **Raspberry Pi Imager**: https://www.raspberrypi.com/software/
2. Insert the SD card (or NVMe drive, if the RPi5 has an NVMe hat — either works).
3. In Imager: choose **Other general-purpose OS → Ubuntu → Ubuntu Server 24.04.x LTS (64-bit)**. Do not pick Ubuntu Desktop — you don't need a GUI on the Pi itself; you'll drive it over SSH and view Gazebo output either via VNC/remote desktop later or (simpler) run Gazebo on your own laptop instead of the Pi (see §7 note).
4. Click the gear icon (⚙) before writing to pre-configure:
   - Hostname (e.g. `sixeyes-pi`)
   - Enable SSH, set a password or add your SSH public key
   - Wi-Fi credentials if not using Ethernet
5. Write the image, then boot the Pi.

## 3. First boot and SSH in

```bash
ssh <username>@sixeyes-pi.local
# or use the IP address directly if .local resolution doesn't work on your network
```

Update the system before installing anything else:

```bash
sudo apt update && sudo apt upgrade -y
sudo reboot
```

## 4. Install ROS2 Jazzy

Ubuntu 24.04 is the officially supported OS for ROS2 Jazzy — no version mismatch to work around.

```bash
sudo apt install -y software-properties-common curl
sudo add-apt-repository universe

sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

sudo apt update
sudo apt install -y ros-jazzy-desktop
```

`ros-jazzy-desktop` includes RViz and demo tools, not just the bare libraries — worth the extra install size while you're still learning, since several of the debugging commands below depend on it.

Source ROS2 in every new shell (or add to `~/.bashrc` to make it permanent):

```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

Verify:

```bash
ros2 doctor --report | head -20
```

## 5. Sanity-check the install

Open two SSH sessions to the Pi. In the first:

```bash
ros2 run demo_nodes_cpp talker
```

In the second:

```bash
ros2 run demo_nodes_py listener
```

You should see the listener printing messages the talker is sending. If that works, ROS2 itself is installed correctly and this is a good checkpoint to stop and actually look at what happened: run `ros2 topic list` in a third session while both are running — you'll see a `/chatter` topic, which is exactly the publish/subscribe pattern described in §1.

## 6. Install Gazebo Harmonic

```bash
sudo apt install -y curl lsb-release gnupg

sudo curl https://packages.osrfoundation.org/gazebo.gpg \
  -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/gazebo-stable.list > /dev/null

sudo apt update
sudo apt install -y gz-harmonic ros-jazzy-ros-gz
```

`ros-jazzy-ros-gz` is the ROS2↔Gazebo bridge package family — it's what lets ROS2 topics and Gazebo's simulation talk to each other. You'll also want `ros-jazzy-ros2-control` and `ros-jazzy-gz-ros2-control` once the arm's URDF and control setup exist (not needed yet for the sanity checks in this guide):

```bash
sudo apt install -y ros-jazzy-ros2-control ros-jazzy-gz-ros2-control
```

## 7. Get the SixEyes workspace onto the Pi and build it

```bash
git clone https://github.com/studiosanka/sixeyes.git
cd sixeyes/ros2_ws
colcon build
source install/setup.bash
```

If `colcon` isn't found:

```bash
sudo apt install -y python3-colcon-common-extensions
```

**Note on running Gazebo's GUI over SSH**: Gazebo's 3D viewer is a real OpenGL application — running it headless over plain SSH won't show you anything. Two practical options once the sim launch files exist (§8, not built yet — see `docs/V1_TODO.md`):
- Run Gazebo on your own laptop instead of the Pi, with the Pi only running the ROS2 nodes, connected over the same network (`ROS_DOMAIN_ID` set the same on both machines so they discover each other).
- Or use `ssh -X` (X11 forwarding) or a VNC session to the Pi — works, but is noticeably slower for anything graphics-heavy than running the GUI locally.

For a first pass, running Gazebo on your laptop and only using the Pi for the ROS2/control side is the simpler setup.

## 8. What's not built yet

This guide gets ROS2 + Gazebo installed and verified. The simulated SixEyes arm itself lives in `ros2_ws/src/sixeyes_description/` (placeholder URDF/Xacro) and `ros2_ws/src/sixeyes_bringup/launch/sim.launch.py` — both exist now but are untested (see `docs/V1_TODO.md` for status). Once verified, this guide's next section will cover `ros2 launch sixeyes_bringup sim.launch.py` and what you should see.

## 9. Useful commands while you're learning

```bash
ros2 node list          # what's currently running
ros2 topic list         # what data streams exist
ros2 topic echo /chatter  # watch messages flow live on a topic
ros2 topic hz /chatter  # how fast a topic is publishing
rqt_graph                # visual graph of nodes and topics (needs a display)
```

## Troubleshooting

- **`ros2: command not found`**: you forgot to `source /opt/ros/jazzy/setup.bash` in this shell — check §4.
- **Workspace nodes not found after `colcon build`**: you built but forgot to `source install/setup.bash` in the shell you're running from. Sourcing only applies to the shell session it's run in.
- **Two machines can't see each other's topics**: check `ROS_DOMAIN_ID` is set identically on both (`echo $ROS_DOMAIN_ID`), and that they're on the same network/subnet with multicast not blocked (some Wi-Fi routers block multicast by default — Ethernet is more reliable for this).
