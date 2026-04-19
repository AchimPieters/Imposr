#!/usr/bin/env python3
"""Mark one host-runtime SDK smoke row as pass with required fields."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

PASS_KEYS = (
    "plugin_load",
    "menu_actions",
    "preset_validate",
    "preset_preview",
    "preset_run_bundle",
    "runtime_quality_gate",
    "imposed_output_open",
    "panel_quick_actions",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", default="docs/sdk_smoke_evidence.json")
    parser.add_argument("--os", required=True)
    parser.add_argument("--cpu", required=True)
    parser.add_argument("--acrobat-version", required=True)
    parser.add_argument("--sdk-version", required=True)
    parser.add_argument("--bundle-path", required=True)
    parser.add_argument("--proof-pdf-path", required=True)
    parser.add_argument("--imposed-output-path", required=True)
    parser.add_argument("--preflight-json-path", required=True)
    parser.add_argument("--sdk-ops-path", required=True)
    parser.add_argument("--control-surface-path", required=True)
    parser.add_argument("--notes", default="")
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    if not evidence_path.exists():
        raise SystemExit(f"ERROR: evidence file not found: {evidence_path}")

    data = json.loads(evidence_path.read_text(encoding="utf-8"))
    rows = data.get("rows", [])
    if not isinstance(rows, list):
        raise SystemExit("ERROR: evidence JSON must contain list field 'rows'")

    row = None
    for item in rows:
        if str(item.get("os", "")) == args.os and str(item.get("cpu", "")) == args.cpu:
            row = item
            break
    if row is None:
        raise SystemExit(f"ERROR: row not found for {args.os} / {args.cpu}")

    row["execution_mode"] = "host-runtime"
    row["acrobat_version"] = args.acrobat_version
    row["sdk_version"] = args.sdk_version
    row["run_timestamp_utc"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    for key in PASS_KEYS:
        row[key] = "pass"

    row["bundle_path"] = args.bundle_path
    row["proof_pdf_path"] = args.proof_pdf_path
    row["imposed_output_path"] = args.imposed_output_path
    row["preflight_json_path"] = args.preflight_json_path
    row["sdk_ops_path"] = args.sdk_ops_path
    row["control_surface_path"] = args.control_surface_path
    if args.notes:
        row["notes"] = args.notes

    evidence_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Marked pass row: {args.os} / {args.cpu}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
