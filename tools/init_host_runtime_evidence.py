#!/usr/bin/env python3
"""Initialize docs/sdk_smoke_evidence.json from the template for host-runtime runs."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--template",
        default="docs/sdk_smoke_evidence.template.json",
        help="Path to input template JSON.",
    )
    parser.add_argument(
        "--out",
        default="docs/sdk_smoke_evidence.json",
        help="Path to output evidence JSON.",
    )
    parser.add_argument(
        "--acrobat-version",
        default="TBD",
        help="Default Acrobat version for all rows.",
    )
    parser.add_argument(
        "--sdk-version",
        default="TBD",
        help="Default SDK version for all rows.",
    )
    parser.add_argument(
        "--timestamp-now",
        action="store_true",
        help="Fill run_timestamp_utc with the current UTC timestamp.",
    )
    args = parser.parse_args()

    template_path = Path(args.template)
    if not template_path.exists():
        raise SystemExit(f"ERROR: template not found: {template_path}")

    data = json.loads(template_path.read_text(encoding="utf-8"))
    rows = data.get("rows", [])
    if not isinstance(rows, list):
        raise SystemExit("ERROR: template JSON must contain a list field 'rows'")

    timestamp = ""
    if args.timestamp_now:
        timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    for row in rows:
        row["execution_mode"] = "host-runtime"
        row["acrobat_version"] = args.acrobat_version
        row["sdk_version"] = args.sdk_version
        if timestamp:
            row["run_timestamp_utc"] = timestamp

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote host-runtime evidence scaffold: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
