#!/usr/bin/env python3
"""Run KiCad DRC on the joint board and print a short, de-duplicated summary.

Usage:  python hardware/tools/run_drc.py [--keep]
Runs on a COPY of the project, so it never touches your files and works while KiCad is open
(it checks the last SAVED board, so save first). Exit code 1 if any short or open is found.

Courtyard checks are switched off in the project (tight placement is intentional); every other
check, and the custom rules in SixEyes_v1.2.kicad_dru, still run.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent / "pcb" / "kicad"
NAME = "SixEyes_v1.2"
CLI_CANDIDATES = [
    shutil.which("kicad-cli"),
    *[str(p) for p in Path(os.environ.get("LOCALAPPDATA", "")).glob("Programs/KiCad/*/bin/kicad-cli.exe")],
    *[str(p) for p in Path("C:/Program Files/KiCad").glob("*/bin/kicad-cli.exe")],
]

# Copper of different nets touching, crossing or overlapping: the real "short" family.
SHORTS = {"shorting_items", "tracks_crossing"}
# Near-misses on tracks/vias that can become shorts at the fab.
NEAR = {"clearance", "hole_clearance", "copper_edge_clearance", "via_dangling", "track_dangling"}
RULES = {"track_width", "via_diameter", "hole_size", "hole_to_hole", "diff_pair_gap",
         "diff_pair_uncoupled_length_too_long", "skew_out_of_range", "length_out_of_range",
         "annular_width", "track_width_too_small", "connection_width"}


def _desc(item):
    pos = item["pos"]
    return f"{item['description']} @({pos['x']:.2f},{pos['y']:.2f})"


def _key(v):
    return (v["type"], tuple(sorted(_desc(i) for i in v["items"])))


def _unique(items):
    seen, out = set(), []
    for v in items:
        k = _key(v)
        if k not in seen:
            seen.add(k)
            out.append(v)
    return out


def main():
    cli = next((c for c in CLI_CANDIDATES if c and Path(c).exists()), None)
    if not cli:
        sys.exit("kicad-cli not found; install KiCad 10 or add it to PATH")
    root = Path(tempfile.mkdtemp(prefix="sixeyes_drc_"))
    # Mirror the hardware/pcb layout so the project's relative library paths (${KIPRJMOD}/../libraries) resolve.
    work = root / "kicad"
    work.mkdir()
    for name in (f"{NAME}.kicad_pcb", f"{NAME}.kicad_pro", f"{NAME}.kicad_sch", f"{NAME}.kicad_dru",
                 "fp-lib-table", "sym-lib-table"):
        shutil.copy(PROJECT / name, work)
    shutil.copytree(PROJECT.parent / "libraries", root / "libraries", ignore=shutil.ignore_patterns("*.step", "*.STEP"))
    report = root / "drc.json"
    r = subprocess.run([cli, "pcb", "drc", "--format", "json", "--severity-all", "--all-track-errors",
                        "--schematic-parity", "--units", "mm", "-o", str(report), str(work / f"{NAME}.kicad_pcb")],
                       capture_output=True, text=True)
    if not report.exists():
        sys.exit(f"DRC did not produce a report:\n{r.stdout}\n{r.stderr}")
    d = json.loads(report.read_text(encoding="utf-8"))
    viol = _unique(d.get("violations", []))
    unconnected = _unique(d.get("unconnected_items", []))
    parity = _unique(d.get("schematic_parity", []))

    def show(title, items):
        print(f"\n== {title}: {len(items)}")
        for v in items:
            print(f"  [{v['type']}] {v['description']}")
            for i in v["items"]:
                print(f"      {_desc(i)}")

    def zone_nets(v):
        return {i["description"].split("[")[1].split("]")[0] for i in v["items"] if "[" in i["description"]}

    # Overlapping zones only matter when their nets differ (same-net overlap just needs distinct priorities).
    shorts = [v for v in viol if v["type"] in SHORTS
              or (v["type"] == "zones_intersect" and len(zone_nets(v)) > 1)]
    near = [v for v in viol if v["type"] in NEAR]
    rules = [v for v in viol if v["type"] in RULES and v not in shorts and v not in near]
    other = [v for v in viol if v not in shorts and v not in near and v not in rules]

    show("SHORTS (different nets touching)", shorts)
    show("OPEN / UNCONNECTED nets", unconnected)
    show("Schematic vs PCB mismatches", parity)
    show("NEAR-SHORTS (clearance / hole / edge)", near)
    show("Custom-rule violations (widths, vias, CAN pair)", rules)
    counts = {}
    for v in other:
        counts[(v["severity"], v["type"])] = counts.get((v["severity"], v["type"]), 0) + 1
    print("\n== Other findings (counts; silk, mask, library notes):")
    for (sev, t), n in sorted(counts.items()):
        print(f"  {sev:8} {t}: {n}")
    print(f"\nFull report: {report}")
    sys.exit(1 if shorts or unconnected or parity else 0)


if __name__ == "__main__":
    main()
