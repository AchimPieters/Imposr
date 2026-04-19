#!/usr/bin/env python3
"""Orchestrate endproduct release preparation with hard quality gates."""

from __future__ import annotations

import argparse
import shlex
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


@dataclass
class StepResult:
    name: str
    command: str
    returncode: int
    stdout: str
    stderr: str


@dataclass
class RunnerContext:
    repo_root: Path
    report_path: Path


def run_step(name: str, cmd: Sequence[str], cwd: Path) -> StepResult:
    process = subprocess.run(
        cmd,
        cwd=str(cwd),
        text=True,
        capture_output=True,
        check=False,
    )
    return StepResult(
        name=name,
        command=" ".join(shlex.quote(part) for part in cmd),
        returncode=process.returncode,
        stdout=process.stdout,
        stderr=process.stderr,
    )


def write_report(ctx: RunnerContext, results: list[StepResult]) -> None:
    lines = [
        "# Endproduct release preparation report",
        "",
        "Deze run toont welke release-stappen geslaagd/mislukt zijn.",
        "",
    ]

    for result in results:
        status = "PASS" if result.returncode == 0 else "FAIL"
        lines.extend(
            [
                f"## {result.name}: {status}",
                "",
                "```bash",
                result.command,
                "```",
                "",
            ]
        )

        if result.stdout.strip():
            lines.extend(["### stdout", "", "```text", result.stdout.rstrip(), "```", ""])
        if result.stderr.strip():
            lines.extend(["### stderr", "", "```text", result.stderr.rstrip(), "```", ""])

    final_status = "PASS" if all(step.returncode == 0 for step in results) else "FAIL"
    lines.extend([f"## Overall: {final_status}", ""])

    ctx.report_path.parent.mkdir(parents=True, exist_ok=True)
    ctx.report_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", default="docs/sdk_smoke_evidence.json")
    parser.add_argument("--build-dir", default="build-package")
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--jobs", default="4")
    parser.add_argument("--allow-mock", action="store_true")
    parser.add_argument("--allow-simulated-runtime", action="store_true")
    parser.add_argument("--allow-incomplete-commercial-docs", action="store_true")
    parser.add_argument("--report-out", default="docs/ENDPRODUCT_RELEASE_PREP_REPORT.md")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    ctx = RunnerContext(repo_root=repo_root, report_path=repo_root / args.report_out)

    gate_cmd = [
        "python3",
        "tools/release_endproduct_gate.py",
        "--evidence",
        args.evidence,
    ]
    if args.allow_mock:
        gate_cmd.append("--allow-mock")
    if args.allow_simulated_runtime:
        gate_cmd.append("--allow-simulated-runtime")
    if args.allow_incomplete_commercial_docs:
        gate_cmd.append("--allow-incomplete-commercial-docs")

    steps: list[StepResult] = []
    steps.append(run_step("Hard endproduct gate", gate_cmd, repo_root))

    if steps[-1].returncode != 0:
        write_report(ctx, steps)
        print(f"Release prep report written to: {ctx.report_path}")
        print("Stopping: hard gate failed.")
        return 1

    steps.append(
        run_step(
            "Configure packaging build",
            [
                "cmake",
                "-S",
                ".",
                "-B",
                args.build_dir,
                "-DAIMP_BUILD_PLUGIN=OFF",
                "-DAIMP_BUILD_CLI=ON",
                "-DAIMP_BUILD_TESTS=OFF",
                "-DAIMP_ENABLE_PACKAGING=ON",
                f"-DCMAKE_BUILD_TYPE={args.build_type}",
            ],
            repo_root,
        )
    )

    if steps[-1].returncode == 0:
        steps.append(
            run_step(
                "Build packaging target",
                ["cmake", "--build", args.build_dir, "-j", str(args.jobs)],
                repo_root,
            )
        )

    if steps[-1].returncode == 0:
        steps.append(
            run_step(
                "Generate packages",
                [
                    "cpack",
                    "--config",
                    f"{args.build_dir}/CPackConfig.cmake",
                    "-C",
                    args.build_type,
                    "-B",
                    args.build_dir,
                ],
                repo_root,
            )
        )

    if steps[-1].returncode == 0:
        steps.append(
            run_step(
                "Generate checksums",
                [
                    "python3",
                    "tools/generate_package_checksums.py",
                    "--dir",
                    args.build_dir,
                    "--out",
                    f"{args.build_dir}/SHA256SUMS.txt",
                ],
                repo_root,
            )
        )

    write_report(ctx, steps)
    print(f"Release prep report written to: {ctx.report_path}")

    return 0 if all(step.returncode == 0 for step in steps) else 1


if __name__ == "__main__":
    raise SystemExit(main())
