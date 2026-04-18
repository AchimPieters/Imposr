#!/usr/bin/env python3
"""
Collect host smoke evidence from artifact directories and upsert rows.

Each row input expects:
  --row "<os>|<cpu>|<bundle_dir>"

The script marks checks as pass if required artifacts exist in bundle_dir.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _load(path: Path) -> dict:
    if path.exists():
        return json.loads(path.read_text(encoding="utf-8"))
    return {"rows": []}


def _upsert_row(rows: list[dict], os_name: str, cpu: str) -> dict:
    for row in rows:
        if str(row.get("os", "")) == os_name and str(row.get("cpu", "")) == cpu:
            return row
    row = {"os": os_name, "cpu": cpu}
    rows.append(row)
    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--row", action="append", default=[], help="Format: os|cpu|bundle_dir")
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    data = _load(evidence_path)
    rows = data.setdefault("rows", [])

    for raw in args.row:
        parts = raw.split("|")
        if len(parts) != 3:
            raise SystemExit(f"Invalid --row value: {raw}")
        os_name, cpu, bundle_dir = parts
        bundle = Path(bundle_dir)
        row = _upsert_row(rows, os_name, cpu)

        proof = bundle / "proof.pdf"
        imposed = bundle / "imposed-output.pdf"
        panel_state = bundle / "panel-state.json"
        preflight = bundle / "preflight.json"

        row["bundle_path"] = str(bundle)
        row["proof_pdf_path"] = str(proof) if proof.exists() else ""
        row["imposed_output_path"] = str(imposed) if imposed.exists() else ""

        # Conservative auto-marking. Real host checks may still require manual confirmation.
        row["plugin_load"] = row.get("plugin_load", "pending")
        row["menu_actions"] = row.get("menu_actions", "pending")
        row["preset_validate"] = "pass" if panel_state.exists() else "pending"
        row["preset_preview"] = "pass" if proof.exists() else "pending"
        row["preset_run_bundle"] = "pass" if bundle.exists() else "pending"
        row["runtime_quality_gate"] = "pass" if preflight.exists() else "pending"
        row["imposed_output_open"] = row.get("imposed_output_open", "pending")
        row["panel_quick_actions"] = row.get("panel_quick_actions", "pending")

    evidence_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Updated evidence: {evidence_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
