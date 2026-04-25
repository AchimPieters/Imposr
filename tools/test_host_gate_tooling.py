#!/usr/bin/env python3
"""Integration test for host GO/NO-GO tooling reaching 100/100 with complete evidence."""

from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def run(cmd: list[str], cwd: Path) -> None:
    proc = subprocess.run(cmd, cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if proc.returncode != 0:
        raise SystemExit(f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{proc.stdout}")


def run_capture(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="aimp-host-gate-") as tmp:
        tmp_path = Path(tmp)

        evidence_path = tmp_path / "sdk_smoke_evidence.json"
        fill_path = tmp_path / "host_fill.json"
        report_path = tmp_path / "HOST_GO_NO_GO_REPORT.md"

        evidence = json.loads((REPO_ROOT / "docs" / "sdk_smoke_evidence.json").read_text(encoding="utf-8"))
        evidence_path.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

        # Baseline evidence ships with simulated/mock rows and should be blocked in strict mode.
        strict_score = run_capture(
            [
                "python3",
                "tools/score_sdk_readiness.py",
                "--evidence",
                str(evidence_path),
                "--forbid-mock",
                "--require-100",
            ],
            REPO_ROOT,
        )
        if strict_score.returncode == 0:
            raise SystemExit(
                "Expected strict mock-forbidden score gate to fail on baseline evidence, but it passed."
            )

        # Create per-platform bundle artifacts so --verify-files can be enabled.
        rows = [
            ("Windows 11 23H2", "x64", tmp_path / "win-run"),
            ("macOS 14 Sonoma", "arm64", tmp_path / "mac-arm-run"),
            ("macOS 14 Sonoma", "x64 (Rosetta/Intel)", tmp_path / "mac-x64-run"),
        ]
        fill_rows = []
        for idx, (os_name, cpu, bundle_dir) in enumerate(rows, start=1):
            bundle_dir.mkdir(parents=True, exist_ok=True)
            (bundle_dir / "proof.pdf").write_text("proof", encoding="utf-8")
            (bundle_dir / "imposed-output.pdf").write_text("imposed", encoding="utf-8")
            (bundle_dir / "preflight.json").write_text("{}", encoding="utf-8")
            (bundle_dir / "sdk-ops.json").write_text("{}", encoding="utf-8")
            (bundle_dir / "control-surface.json").write_text("{}", encoding="utf-8")

            fill_rows.append(
                {
                    "os": os_name,
                    "cpu": cpu,
                    "acrobat_version": f"2026.001.{30000 + idx}",
                    "sdk_version": "DC-2026",
                    "bundle_path": str(bundle_dir),
                    "proof_pdf_path": str(bundle_dir / "proof.pdf"),
                    "imposed_output_path": str(bundle_dir / "imposed-output.pdf"),
                    "preflight_json_path": str(bundle_dir / "preflight.json"),
                    "sdk_ops_path": str(bundle_dir / "sdk-ops.json"),
                    "control_surface_path": str(bundle_dir / "control-surface.json"),
                    "notes": "integration tooling test",
                }
            )

        fill_path.write_text(json.dumps({"rows": fill_rows}, indent=2) + "\n", encoding="utf-8")

        run(
            [
                "python3",
                "tools/run_host_go_no_go.py",
                "--evidence",
                str(evidence_path),
                "--fill",
                str(fill_path),
                "--verify-files",
                "--report-out",
                str(report_path),
            ],
            REPO_ROOT,
        )

        strict_score_after_fill = run_capture(
            [
                "python3",
                "tools/score_sdk_readiness.py",
                "--evidence",
                str(evidence_path),
                "--forbid-mock",
                "--require-100",
            ],
            REPO_ROOT,
        )
        if strict_score_after_fill.returncode != 0:
            raise SystemExit(
                "Expected strict mock-forbidden score gate to pass after host fill evidence.\n"
                f"{strict_score_after_fill.stdout}"
            )

        report = report_path.read_text(encoding="utf-8")
        if "**GO**" not in report:
            raise SystemExit(f"Expected GO decision in report, got:\n{report}")

    print("Host GO/NO-GO tooling integration test passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
