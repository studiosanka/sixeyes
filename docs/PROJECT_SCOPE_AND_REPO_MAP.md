# Project Scope And Repo Map

This document defines the authoritative scope split across the three SixEyes repositories.

## TLDR

- Use sixeyes for the standard robotic arm build (hardware + firmware) with laptop-assisted operation.
- Use nodemesh only when you want optional 4-MCU edge training/inference firmware.
- Use cyclocad only for cycloidal gear design tooling.

## Repository Ownership

### 1) sixeyes (standard package)

Repository:
- https://github.com/SixEyes-Open-Source/sixeyes

Purpose:
- The default project that most users should start with.
- Covers standard arm hardware, follower and leader firmware, ROS2/laptop workflows, assembly, validation, and deployment.

What is included:
- Main arm hardware and wiring docs.
- Standard follower and leader firmware.
- ROS2 and laptop teleoperation/control tooling.
- Hardware options that preserve future compatibility.

What is intentionally excluded:
- NodeMesh multi-node edge-perception firmware internals.
- NodeMesh camera-node firmware and ESP-NOW mesh implementation details.

Important policy:
- SD card reader hardware may be physically present for compatibility, but it is not part of default runtime behavior in the standard sixeyes flow.
- ESP32-CAM perception nodes are not part of the standard sixeyes package.

### 2) nodemesh (optional firmware overlay)

Repository:
- https://github.com/SixEyes-Open-Source/nodemesh

Purpose:
- Optional distributed firmware system for edge-first operation using 4 MCUs.

What is included:
- Node0 orchestrator firmware (control, data sync, logging, lightweight learning scaffold).
- Node1 input firmware (teleoperation stream source).
- Node2 and Node3 camera-node firmware and ESP-NOW transport.
- NodeMesh-specific docs, pinouts, checklists, and phased roadmap.

NodeMesh hardware assumptions:
- Builds on compatible SixEyes hardware foundation.
- Uses SD logging in NodeMesh mode.
- Supports low-cost ESP32-CAM based perception nodes (and can be adapted to S3 camera variants when needed).

### 3) cyclocad (mechanical design tool)

Repository:
- https://github.com/SixEyes-Open-Source/cyclocad

Purpose:
- Cycloidal gear generation and CAD export tooling.

What is included:
- Gear profile generation scripts.
- DXF and CSV export artifacts.
- SolidWorks macro and related docs.

## Folder To Repo Routing (Current)

From the workspace root:

- nodemesh/ → nodemesh repository
- cyclocad/ → cyclocad repository
- firmware/, ros2_ws/, tools/, simulation/, hardware_assets/, SixEyes Follower PCB/, docs/, README.md, CONTRIBUTING.md, LICENSE → sixeyes repository

> **Note**: The `sixeyes/` wrapper folder has been removed. All SixEyes project code now lives directly at the repository root.  
> If you cloned before June 2026 and had paths like `sixeyes/firmware/...`, update them to `firmware/...`.

## Documentation Simplification Rules

Use these rules to keep docs understandable and avoid overlap:

1. One authoritative page per topic.
- Keep one source-of-truth doc for each of: pinout, wiring, flashing, protocol, test plan.

2. Turn duplicates into pointers.
- If a doc repeats material from the authoritative page, replace duplicated sections with a short summary and a direct link.

3. Keep repo boundaries explicit at the top of entry docs.
- Every root README should state what the repo is for and what it is not for.

4. Keep standard and optional paths separate.
- Standard sixeyes flow should not require NodeMesh docs.
- NodeMesh docs can reference sixeyes hardware baseline, but should not redefine sixeyes standards unless required.

5. Archive instead of deleting first.
- For uncertain duplicate docs, move to an archive folder before permanent deletion.

## Recommended Next Cleanup Pass

- In sixeyes: keep only one primary hardware bring-up path and one protocol source-of-truth.
- In nodemesh: keep one architecture overview plus one implementation checklist; convert extra long-form narrative docs into concise reference pages.
- In both repos: add a short "Read this first" section at top-level docs index with no more than 3 starting links.