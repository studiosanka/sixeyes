# Safety Overview

This document summarizes the safety-critical constraints from the Technical Reference.

- Motors disabled by default on boot.
- EN pins are controlled only by the safety task on the follower.
- Heartbeat timeout default: 500 ms (configurable in code).
- Star grounding required. Follow wiring diagrams in `docs/wiring.md`.

Before connecting real hardware, run full simulation and verify the safety_node behavior.
