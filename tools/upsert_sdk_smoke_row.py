#!/usr/bin/env python3
"""
Upsert one platform row in sdk smoke evidence JSON.

Usage example:
  python3 tools/upsert_sdk_smoke_row.py \
    --evidence docs/sdk_smoke_evidence.json \
    --os "Windows 11 23H2" \
    --cpu "x64" \
    --set plugin_load=pass --set menu_actions=pass --set preset_validate=pass \
    --set preset_preview=pass --set preset_run_bundle=pass --set runtime_quality_gate=pass \
    --set imposed_output_open=pass --set panel_quick_actions=pass \
    --set bundle_path="C:/evidence/run-001" \
    --set proof_pdf_path="C:/evidence/run-001/proof.pdf" \
    --set imposed_output_path="C:/evidence/run-001/imposed-output.pdf"
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--os", required=True)
    parser.add_argument("--cpu", required=True)
    parser.add_argument("--set", action="append", default=[], help="key=value entries to upsert into the selected row")
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    if evidence_path.exists():
        data = json.loads(evidence_path.read_text(encoding="utf-8"))
    else:
        data = {"rows": []}

    rows = data.setdefault("rows", [])
    row = None
    for item in rows:
        if str(item.get("os", "")) == args.os and str(item.get("cpu", "")) == args.cpu:
            row = item
            break
    if row is None:
        row = {"os": args.os, "cpu": args.cpu}
        rows.append(row)

    for entry in args.set:
        if "=" not in entry:
            raise SystemExit(f"Invalid --set entry (expected key=value): {entry}")
        key, value = entry.split("=", 1)
        row[key] = value

    evidence_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Updated evidence row: {args.os} / {args.cpu}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
