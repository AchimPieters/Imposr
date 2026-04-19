#!/usr/bin/env python3
"""Hard release gate for deciding whether Imposr qualifies as an end product release."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

REQUIRED_ROWS = (
    ("Windows 11 23H2", "x64"),
    ("macOS 14 Sonoma", "arm64"),
    ("macOS 14 Sonoma", "x64 (Rosetta/Intel)"),
)

REQUIRED_CHECKS = (
    "plugin_load",
    "menu_actions",
    "preset_validate",
    "preset_preview",
    "preset_run_bundle",
    "runtime_quality_gate",
    "imposed_output_open",
    "panel_quick_actions",
)

REQUIRED_METADATA = (
    "execution_mode",
    "acrobat_version",
    "sdk_version",
    "run_timestamp_utc",
)

REQUIRED_ARTIFACTS = (
    "bundle_path",
    "proof_pdf_path",
    "imposed_output_path",
    "preflight_json_path",
    "sdk_ops_path",
    "control_surface_path",
)

REQUIRED_COMMERCIAL_DOCS = (
    "docs/COMMERCIAL_RELEASE_CHECKLIST.md",
    "docs/commercial/THIRD_PARTY_NOTICES.md",
    "docs/commercial/EULA.md",
    "docs/commercial/PRIVACY_STATEMENT.md",
    "docs/commercial/TELEMETRY_DISCLOSURE.md",
    "docs/release/VERSIONING_POLICY.md",
    "docs/release/ROLLBACK_POLICY.md",
    "docs/release/UPGRADE_COMPATIBILITY_MATRIX.md",
    "docs/support/INCIDENT_SLA.md",
    "docs/security/SECURITY_RELEASE_CHECKLIST.md",
    "docs/security/SBOM_POLICY.md",
    "docs/customer-ops/ENTERPRISE_DEPLOYMENT.md",
    "docs/customer-ops/TROUBLESHOOTING_PLAYBOOK.md",
)


def _normalize(value: str) -> str:
    return value.strip().lower()


def _is_pass(value: str) -> bool:
    return _normalize(value) in {"pass", "ok", "true", "1"}


def _is_placeholder(value: str) -> bool:
    return _normalize(value) in {"", "tbd", "fill-me", "placeholder", "vul-hier-versie-in", "vul-hier-sdk-in"}


def _row_looks_mock(row: dict) -> bool:
    notes = _normalize(str(row.get("notes", "")))
    if "mock" in notes:
        return True
    for key in REQUIRED_ARTIFACTS:
        if "mock" in _normalize(str(row.get(key, ""))):
            return True
    if "mock" in _normalize(str(row.get("acrobat_version", ""))):
        return True
    if "mock" in _normalize(str(row.get("sdk_version", ""))):
        return True
    return False


def _load_rows(path: Path) -> list[dict]:
    if not path.exists():
        raise SystemExit(f"ERROR: evidence file not found: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    rows = data.get("rows")
    if not isinstance(rows, list):
        raise SystemExit("ERROR: evidence JSON must contain list field 'rows'")
    return rows


def _find_row(rows: list[dict], os_name: str, cpu: str) -> dict | None:
    for row in rows:
        if _normalize(str(row.get("os", ""))) == _normalize(os_name) and _normalize(str(row.get("cpu", ""))) == _normalize(cpu):
            return row
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", default="docs/sdk_smoke_evidence.json")
    parser.add_argument("--allow-mock", action="store_true", help="Allow mock evidence (for local dry-runs only).")
    parser.add_argument(
        "--allow-simulated-runtime",
        action="store_true",
        help="Allow simulated-runtime rows instead of requiring host-runtime.",
    )
    parser.add_argument(
        "--artifact-root",
        default=".",
        help="Base directory for resolving artifact paths inside the evidence file.",
    )
    parser.add_argument(
        "--skip-commercial-docs-check",
        action="store_true",
        help="Skip required commercial governance docs check (not recommended for real releases).",
    )
    args = parser.parse_args()

    rows = _load_rows(Path(args.evidence))
    artifact_root = Path(args.artifact_root).resolve()

    issues: list[str] = []

    for os_name, cpu in REQUIRED_ROWS:
        row = _find_row(rows, os_name, cpu)
        if row is None:
            issues.append(f"missing row: {os_name} / {cpu}")
            continue

        execution_mode = _normalize(str(row.get("execution_mode", "")))
        if args.allow_simulated_runtime:
            if execution_mode not in {"host-runtime", "simulated-runtime"}:
                issues.append(f"{os_name}/{cpu}: invalid execution_mode={row.get('execution_mode')!r}")
        else:
            if execution_mode != "host-runtime":
                issues.append(f"{os_name}/{cpu}: execution_mode must be host-runtime (got {row.get('execution_mode')!r})")

        if not args.allow_mock and _row_looks_mock(row):
            issues.append(f"{os_name}/{cpu}: mock evidence detected")

        for key in REQUIRED_CHECKS:
            if not _is_pass(str(row.get(key, ""))):
                issues.append(f"{os_name}/{cpu}: {key} must be pass (got {row.get(key)!r})")

        for key in REQUIRED_METADATA:
            value = str(row.get(key, ""))
            if _is_placeholder(value):
                issues.append(f"{os_name}/{cpu}: {key} is missing/placeholder")

        for key in REQUIRED_ARTIFACTS:
            value = str(row.get(key, "")).strip()
            if value == "":
                issues.append(f"{os_name}/{cpu}: {key} missing")
                continue
            artifact_path = (artifact_root / value).resolve() if not Path(value).is_absolute() else Path(value)
            if not artifact_path.exists():
                issues.append(f"{os_name}/{cpu}: artifact missing on disk: {artifact_path}")

    if not args.skip_commercial_docs_check:
        for doc in REQUIRED_COMMERCIAL_DOCS:
            doc_path = (artifact_root / doc).resolve()
            if not doc_path.exists():
                issues.append(f"commercial-docs: missing required file {doc_path}")
                continue
            if doc_path.stat().st_size == 0:
                issues.append(f"commercial-docs: empty file {doc_path}")

    if issues:
        print("ENDPRODUCT RELEASE GATE: FAIL")
        for issue in issues:
            print(f"- {issue}")
        return 1

    print("ENDPRODUCT RELEASE GATE: PASS")
    print("All required host-runtime rows, checks, metadata and artifact files are valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
