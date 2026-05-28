# Documentation Consolidation Plan

Goal: reduce overlap and keep onboarding clear across sixeyes and nodemesh.

## Principles

1. Keep one canonical source per topic.
2. Convert duplicates into short pointer docs.
3. Archive before permanent deletion.
4. Keep standard sixeyes path independent from nodemesh internals.

## sixeyes Repository

Keep as canonical:

- docs/hardware/WIRING_AND_ASSEMBLY.md
- docs/hardware/HARDWARE_VALIDATION.md
- docs/deployment/FLASHING_AND_DEPLOYMENT.md
- docs/protocols/JSON_MESSAGE_PROTOCOL.md
- docs/README.md
- docs/PROJECT_SCOPE_AND_REPO_MAP.md

Candidates to simplify (not delete first):

- docs/firmware/VISUAL_ARCHITECTURE_GUIDE.md
- docs/firmware/IMPLEMENTATION_SUMMARY.md
- docs/references/FIRMWARE_SYSTEM_DATASHEET.md

Action for simplification candidates:

- Keep only architecture diagram and timing details in VISUAL_ARCHITECTURE_GUIDE.
- Keep only implementation specifics and build/runtime limits in IMPLEMENTATION_SUMMARY.
- Keep only datasheet-style interface table in FIRMWARE_SYSTEM_DATASHEET.
- Remove repeated protocol and pinout prose from whichever is not canonical.

Reference folder cleanup:

- docs/references/SixEyes Technical Reference.txt
- docs/references/SixEyes Technical Reference 2.txt

Action:

- Choose one authoritative revision.
- Move the superseded file to an archive folder with a deprecation note.

## nodemesh Repository

Keep as canonical:

- nodemesh/firmware NodeMesh VLA/docs/NODEMESH_FIRMWARE_CHECKLIST.md
- nodemesh/firmware NodeMesh VLA/docs/NODEMESH_VLA_GO_NO_GO.md
- nodemesh/firmware NodeMesh VLA/docs/PINOUT_FOLLOWER_ESP32.md
- nodemesh/firmware NodeMesh VLA/docs/SD_MODULE_WIRING_NODE0.md
- nodemesh/firmware NodeMesh VLA/docs/WHAT_IS_NODEMESH_VLA.md

Candidates to simplify:

- nodemesh/firmware NodeMesh VLA/docs/NODEMESH_VLA_PHASED_ROADMAP.md
- nodemesh/firmware NodeMesh VLA/docs/CONNECTOR_LEGEND_ONE_PAGE.md
- nodemesh/firmware NodeMesh VLA/docs/DEVKITC1_HEADER_TO_PCB_NET_TABLE.md

Action for simplification candidates:

- Keep ROADMAP as execution sequencing only.
- Keep CONNECTOR_LEGEND as wiring quick reference only.
- Keep DEVKITC1_HEADER_TO_PCB_NET_TABLE as strict pin table only.
- Remove repeated narrative and policy text that already exists in WHAT_IS or CHECKLIST.

## Suggested Archive Layout

Create these folders before removals:

- docs/archive/sixeyes/
- docs/archive/nodemesh/

Move docs there first for one release cycle, then delete if no regressions.

## Fast Execution Order

1. Freeze canonical files list.
2. For each non-canonical doc, replace repeated sections with links.
3. Move superseded docs to archive.
4. Update README navigation to only show canonical docs first.
5. Re-run link checks in top-level docs.
