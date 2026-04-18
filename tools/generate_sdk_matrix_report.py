#!/usr/bin/env python3
"""
Generate a markdown status report for the SDK smoke matrix.

Usage:
  python3 tools/generate_sdk_matrix_report.py \
    --evidence docs/sdk_smoke_evidence.json \
    --out docs/SDK_SMOKE_MATRIX_REPORT.md
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


CHECKS = (
    "plugin_load",
    "menu_actions",
    "preset_validate",
    "preset_preview",
    "preset_run_bundle",
    "runtime_quality_gate",
    "imposed_output_open",
    "panel_quick_actions",
)


def _is_pass(value: str) -> bool:
    return value.strip().lower() in {"pass", "ok", "true", "1"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    data = json.loads(evidence_path.read_text(encoding="utf-8"))
    rows = list(data.get("rows", []))

    lines: list[str] = []
    lines.append("# SDK Smoke Matrix Report")
    lines.append("")
    lines.append(f"Source: `{evidence_path}`")
    lines.append("")
    lines.append("| OS | CPU | Mode | Acrobat | SDK | Pass ratio | Status |")
    lines.append("|---|---|---|---|---|---:|---|")

    total_checks = 0
    passed_checks = 0
    for row in rows:
        row_pass = 0
        for check in CHECKS:
            total_checks += 1
            if _is_pass(str(row.get(check, ""))):
                passed_checks += 1
                row_pass += 1
        ratio = f"{row_pass}/{len(CHECKS)}"
        status = "PASS" if row_pass == len(CHECKS) else "BLOCKED"
        lines.append(
            f"| {row.get('os', '')} | {row.get('cpu', '')} | {row.get('execution_mode', '')} | "
            f"{row.get('acrobat_version', '')} | {row.get('sdk_version', '')} | {ratio} | {status} |"
        )

    lines.append("")
    lines.append(f"Overall pass ratio: **{passed_checks}/{total_checks}**")
    lines.append("")
    lines.append("## Missing checks")
    lines.append("")
    for row in rows:
        missing = [check for check in CHECKS if not _is_pass(str(row.get(check, "")))]
        if not missing:
            continue
        lines.append(f"- **{row.get('os', '')} / {row.get('cpu', '')}:** {', '.join(missing)}")

    out_path = Path(args.out)
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote report: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
