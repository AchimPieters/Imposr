#!/usr/bin/env python3
"""Run end-to-end host evidence finalization and GO/NO-GO checks."""

from __future__ import annotations

import argparse
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def run_cmd(cmd: list[str]) -> tuple[int, str]:
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return proc.returncode, proc.stdout


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", default="docs/sdk_smoke_evidence.json")
    parser.add_argument("--fill", required=True, help="Filled JSON input for finalize_host_release_evidence.py")
    parser.add_argument("--verify-files", action="store_true")
    parser.add_argument("--report-out", default="docs/HOST_GO_NO_GO_REPORT.md")
    args = parser.parse_args()

    commands: list[list[str]] = []

    finalize_cmd = [
        "python3",
        "tools/finalize_host_release_evidence.py",
        "--evidence",
        args.evidence,
        "--input",
        args.fill,
    ]
    if args.verify_files:
        finalize_cmd.append("--verify-files")
    commands.append(finalize_cmd)

    validate_cmd = [
        "python3",
        "tools/validate_sdk_smoke.py",
        "--evidence",
        args.evidence,
        "--require-host-runtime",
    ]
    score_cmd = [
        "python3",
        "tools/score_sdk_readiness.py",
        "--evidence",
        args.evidence,
        "--require-100",
    ]
    commands.append(validate_cmd)
    commands.append(score_cmd)

    results: list[tuple[list[str], int, str]] = []
    for cmd in commands:
        rc, out = run_cmd(cmd)
        results.append((cmd, rc, out))
        if rc != 0 and cmd is finalize_cmd:
            break

    report_lines = []
    report_lines.append("# Host GO/NO-GO Report\n")
    report_lines.append(f"Generated UTC: {datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}\n")
    report_lines.append(f"Evidence file: `{args.evidence}`\n")
    report_lines.append(f"Fill input: `{args.fill}`\n")

    overall_ok = True
    for cmd, rc, out in results:
        status = "PASS" if rc == 0 else "FAIL"
        if rc != 0:
            overall_ok = False
        report_lines.append(f"## `{ ' '.join(cmd) }`\n")
        report_lines.append(f"Status: **{status}**\n")
        report_lines.append("```text")
        report_lines.append(out.rstrip())
        report_lines.append("```\n")

    report_lines.append(f"## Final decision\n**{'GO' if overall_ok else 'NO-GO'}**\n")

    out_path = Path(args.report_out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(report_lines) + "\n", encoding="utf-8")
    print(f"Wrote report: {out_path}")

    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
