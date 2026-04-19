#!/usr/bin/env python3
"""Generate SHA-256 checksums for package artifacts in a directory."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

PACKAGE_SUFFIXES = (".zip", ".exe", ".dmg", ".tar.gz", ".deb")


def is_package_file(path: Path) -> bool:
    name = path.name.lower()
    return any(name.endswith(suffix) for suffix in PACKAGE_SUFFIXES)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", required=True, help="Directory containing built package artifacts.")
    parser.add_argument("--out", required=True, help="Output checksum file path.")
    args = parser.parse_args()

    package_dir = Path(args.dir)
    if not package_dir.exists() or not package_dir.is_dir():
        raise SystemExit(f"ERROR: package dir does not exist: {package_dir}")

    package_files = sorted([p for p in package_dir.iterdir() if p.is_file() and is_package_file(p)])
    if not package_files:
        raise SystemExit(f"ERROR: no package files found in {package_dir}")

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    lines = []
    for package in package_files:
        lines.append(f"{sha256_file(package)}  {package.name}\n")

    out_path.write_text("".join(lines), encoding="utf-8")
    print(f"Wrote checksums: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
